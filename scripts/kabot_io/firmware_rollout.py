#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: Copyright The Kabot Project Contributors

"""Fleet OTA rollout helper using Bonjour discovery and smpclient.

Workflow per robot:
1) Discover robot over Bonjour.
2) Upload MCUboot signed image over SMP/UDP.
3) Mark uploaded image for test boot.
4) Reboot robot.
5) Wait for rediscovery.
6) Verify uploaded image is active.
7) Confirm running image as permanent.
"""

from __future__ import annotations

import argparse
import asyncio
import hashlib
import ipaddress
import json
import socket
import subprocess
import sys
import time
from collections.abc import Iterable
from contextlib import suppress
from dataclasses import dataclass
from pathlib import Path

from proto_codec import decode_bonjour_response, encode_bonjour, initialize_proto_runtime
from smpclient import SMPClient
from smpclient.generics import error, success
from smpclient.requests.image_management import ImageStatesRead, ImageStatesWrite
from smpclient.requests.os_management import ResetWrite
from smpclient.transport.udp import SMPUDPTransport

MIN_DISCOVERY_RESPONSE_WINDOW_SEC = 0.8
LOCALHOST_GRACE_WINDOW_SEC = 0.12
DEFAULT_DISCOVERY_PORT = 30012
DEFAULT_STATE_PORT = 30011
DEFAULT_SMP_PORT = 1337
DEFAULT_IMAGE_PATH = Path("build/esp32s3_devkitc/app/zephyr/zephyr.signed.bin")
DEFAULT_BUILD_CMD = ["./scripts/build.zsh", "--no-flash"]
DEFAULT_DISCOVERY_ROUNDS = 3
DEFAULT_DISCOVERY_ROUND_DELAY_S = 0.5


@dataclass(frozen=True)
class InterfaceSubnet:
    ifname: str
    local_ip: str
    prefixlen: int
    host_count: int


@dataclass
class DiscoveredRobot:
    ip: str
    control_port: int
    serial: str
    human_name: str
    firmware_version: str
    is_claimed: bool = False
    claimed_by_ip: str = ""


@dataclass
class RobotResult:
    robot_key: str
    ip: str
    serial: str
    status: str
    stage: str
    message: str


class RolloutError(RuntimeError):
    pass


class SMPUDPTransportWithPort(SMPUDPTransport):
    """SMP UDP transport variant with configurable target port."""

    def __init__(self, *, mtu: int, port: int) -> None:
        super().__init__(mtu=mtu)
        self._port = port

    async def connect(self, address: str, timeout_s: float) -> None:
        await super().connect(address, timeout_s, port=self._port)


def _linux_ipv4_interfaces() -> list[InterfaceSubnet]:
    cmd = ["ip", "-j", "-4", "addr", "show", "up"]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=5.0, check=False)
    except FileNotFoundError:
        return []
    except subprocess.TimeoutExpired:
        return []

    if proc.returncode != 0:
        return []

    try:
        data = json.loads(proc.stdout)
    except json.JSONDecodeError:
        return []

    subnets: list[InterfaceSubnet] = []
    for iface in data:
        ifname = iface.get("ifname", "unknown")
        for info in iface.get("addr_info", []):
            if info.get("family") != "inet":
                continue
            local = info.get("local")
            prefixlen = info.get("prefixlen")
            if not local or prefixlen is None:
                continue
            prefixlen_int = int(prefixlen)
            host_count = _host_count_for_prefix(prefixlen_int)
            subnets.append(
                InterfaceSubnet(
                    ifname=ifname,
                    local_ip=local,
                    prefixlen=prefixlen_int,
                    host_count=host_count,
                )
            )

    return subnets


def _host_count_for_prefix(prefixlen: int) -> int:
    if prefixlen >= 32:
        return 1
    if prefixlen == 31:
        return 2
    return max(0, (1 << (32 - prefixlen)) - 2)


def _iter_hosts_for_cidr(local_ip: str, prefixlen: int) -> Iterable[str]:
    network = ipaddress.ip_network(f"{local_ip}/{prefixlen}", strict=False)

    if network.prefixlen == 32:
        yield str(network.network_address)
        return

    if network.prefixlen == 31:
        for ip in network:
            yield str(ip)
        return

    for ip in network.hosts():
        yield str(ip)


def _iter_discovery_candidates(include_localhost: bool) -> Iterable[str]:
    seen: set[str] = set()

    if include_localhost:
        seen.add("127.0.0.1")
        yield "127.0.0.1"

    subnets = sorted(_linux_ipv4_interfaces(), key=lambda i: (i.host_count, i.ifname, i.local_ip))

    for subnet in subnets:
        for host in _iter_hosts_for_cidr(subnet.local_ip, subnet.prefixlen):
            if host in seen:
                continue
            seen.add(host)
            yield host


def _robot_key(robot: DiscoveredRobot) -> str:
    serial_key = (robot.serial or "").strip()
    if serial_key:
        return serial_key
    return f"{robot.ip}:{robot.control_port}"


def discover_many(
    *,
    discovery_port: int,
    state_port: int,
    timeout_s: float,
    include_localhost: bool,
) -> list[DiscoveredRobot]:
    response_window_s = max(MIN_DISCOVERY_RESPONSE_WINDOW_SEC, timeout_s)
    deadline = time.monotonic() + response_window_s
    payload = encode_bonjour(state_port, claim=False, release=False)

    found: dict[str, DiscoveredRobot] = {}

    def collect_available(sock: socket.socket) -> None:
        while True:
            try:
                response, addr = sock.recvfrom(2048)
            except (BlockingIOError, TimeoutError, OSError):
                return

            try:
                msg = decode_bonjour_response(response)
            except Exception:
                continue

            control_port = int(msg.control_port) if msg.control_port > 0 else 30010
            robot = DiscoveredRobot(
                ip=addr[0],
                control_port=control_port,
                serial=msg.serial,
                human_name=msg.human_name,
                firmware_version=msg.firmware_version,
                is_claimed=bool(getattr(msg, "is_claimed", False)),
                claimed_by_ip=str(getattr(msg, "claimed_by_ip", "")),
            )
            found[_robot_key(robot)] = robot

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("0.0.0.0", 0))
        sock.setblocking(False)

        for host in _iter_discovery_candidates(include_localhost):
            if time.monotonic() >= deadline:
                break
            try:
                sock.sendto(payload, (host, discovery_port))
            except OSError:
                continue

            if host == "127.0.0.1":
                localhost_deadline = min(deadline, time.monotonic() + LOCALHOST_GRACE_WINDOW_SEC)
                while time.monotonic() < localhost_deadline:
                    collect_available(sock)
                    time.sleep(0.01)

            collect_available(sock)

        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            sock.settimeout(remaining)
            collect_available(sock)

    robots = list(found.values())
    robots.sort(key=lambda r: (r.serial or "", r.human_name or "", r.ip, r.control_port))
    return robots


def discover_many_rounds(
    *,
    discovery_port: int,
    state_port: int,
    timeout_s: float,
    include_localhost: bool,
    rounds: int,
    round_delay_s: float,
) -> list[DiscoveredRobot]:
    merged: dict[str, DiscoveredRobot] = {}
    rounds = max(1, rounds)

    for idx in range(rounds):
        discovered = discover_many(
            discovery_port=discovery_port,
            state_port=state_port,
            timeout_s=timeout_s,
            include_localhost=include_localhost,
        )
        for robot in discovered:
            merged[_robot_key(robot)] = robot

        if idx < (rounds - 1) and round_delay_s > 0:
            time.sleep(round_delay_s)

    robots = list(merged.values())
    robots.sort(key=lambda r: (r.serial or "", r.human_name or "", r.ip, r.control_port))
    return robots


def wait_for_robot_return(
    *,
    wanted_serial: str,
    wanted_ip: str,
    discovery_port: int,
    state_port: int,
    discovery_timeout_s: float,
    include_localhost: bool,
    max_wait_s: float,
    retry_interval_s: float,
) -> DiscoveredRobot | None:
    deadline = time.monotonic() + max_wait_s
    while time.monotonic() < deadline:
        robots = discover_many(
            discovery_port=discovery_port,
            state_port=state_port,
            timeout_s=discovery_timeout_s,
            include_localhost=include_localhost,
        )
        for robot in robots:
            if wanted_serial and robot.serial == wanted_serial:
                return robot
            if not wanted_serial and robot.ip == wanted_ip:
                return robot
        time.sleep(retry_interval_s)
    return None


def _hash_hex(data: bytes | None) -> str:
    if data is None:
        return ""
    return bytes(data).hex()


def _filter_targets(robots: list[DiscoveredRobot], limit: set[str] | None) -> list[DiscoveredRobot]:
    if not limit:
        return robots
    filtered: list[DiscoveredRobot] = []
    for robot in robots:
        keys = {robot.ip, robot.serial, _robot_key(robot)}
        if any(key for key in keys if key in limit):
            filtered.append(robot)
    return filtered


def _state_image_summary(images: list[object]) -> str:
    parts: list[str] = []
    for image in images:
        hash_hex = _hash_hex(getattr(image, "hash", None))
        parts.append(
            f"img={getattr(image, 'image', '?')} slot={getattr(image, 'slot', '?')} "
            f"active={bool(getattr(image, 'active', False))} "
            f"confirmed={bool(getattr(image, 'confirmed', False))} hash={hash_hex[:12]}"
        )
    return " | ".join(parts)


def _pick_uploaded_hash(pre_images: list[object], post_images: list[object]) -> bytes:
    pre_hashes = {bytes(i.hash) for i in pre_images if getattr(i, "hash", None) is not None}

    new_hash_candidates = [
        bytes(i.hash)
        for i in post_images
        if getattr(i, "hash", None) is not None and bytes(i.hash) not in pre_hashes
    ]
    if len(new_hash_candidates) == 1:
        return new_hash_candidates[0]

    # Fallback for same-hash re-upload or already-present secondary image.
    secondary = [
        i
        for i in post_images
        if getattr(i, "slot", None) == 1
        and getattr(i, "hash", None) is not None
        and (getattr(i, "image", None) in (None, 0))
    ]
    if len(secondary) == 1:
        return bytes(secondary[0].hash)

    raise RolloutError(
        "Cannot uniquely determine uploaded image hash from image states; "
        f"new_candidates={len(new_hash_candidates)} secondary_candidates={len(secondary)}"
    )


def _is_test_to_active_denied(response: object) -> bool:
    # Some MCUboot policies disallow "test" upgrades and only allow permanent confirms.
    return "IMAGE_SETTING_TEST_TO_ACTIVE_DENIED" in str(response)


async def _read_image_states(client: SMPClient, timeout_s: float) -> list[object]:
    response = await client.request(ImageStatesRead(), timeout_s=timeout_s)
    if error(response):
        raise RolloutError(f"ImageStatesRead failed: {response}")
    if success(response):
        return list(response.images)
    raise RolloutError("Unexpected response type from ImageStatesRead")


async def _connect_with_retry(
    client: SMPClient,
    *,
    max_wait_s: float,
    retry_interval_s: float,
) -> bool:
    deadline = time.monotonic() + max_wait_s
    while time.monotonic() < deadline:
        try:
            await client.connect()
            return True
        except Exception:
            await asyncio.sleep(retry_interval_s)
    return False


async def _update_robot(
    *,
    robot: DiscoveredRobot,
    image_bytes: bytes,
    smp_port: int,
    smp_timeout_s: float,
    mtu: int,
    discovery_port: int,
    state_port: int,
    discovery_timeout_s: float,
    include_localhost: bool,
    rediscovery_timeout_s: float,
    rediscovery_interval_s: float,
    skip_discovery_on_return: bool,
) -> RobotResult:
    stage = "connect"
    target_hash: bytes | None = None

    transport = SMPUDPTransportWithPort(mtu=mtu, port=smp_port)
    client = SMPClient(transport=transport, address=robot.ip, timeout_s=smp_timeout_s)

    try:
        await client.connect()

        stage = "read-pre-state"
        pre_images = await _read_image_states(client, smp_timeout_s)

        stage = "upload"
        last_reported = -1
        async for off in client.upload(image_bytes, slot=0, first_timeout_s=40.0, use_sha=True):
            progress = int((off * 100) / len(image_bytes))
            if progress // 10 > last_reported // 10:
                print(f"[{_robot_key(robot)}] upload {progress}% ({off}/{len(image_bytes)})")
                last_reported = progress

        stage = "read-post-state"
        post_images = await _read_image_states(client, smp_timeout_s)
        target_hash = _pick_uploaded_hash(pre_images, post_images)

        stage = "mark-test"
        write_test = await client.request(
            ImageStatesWrite(hash=target_hash, confirm=False), timeout_s=smp_timeout_s
        )
        if error(write_test):
            if _is_test_to_active_denied(write_test):
                print(
                    f"[{_robot_key(robot)}] test mark denied by target policy; "
                    "falling back to permanent mark"
                )
                write_confirm = await client.request(
                    ImageStatesWrite(hash=target_hash, confirm=True), timeout_s=smp_timeout_s
                )
                if error(write_confirm):
                    raise RolloutError(
                        "ImageStatesWrite(test) denied and confirm fallback failed: "
                        f"{write_confirm}"
                    )
            else:
                raise RolloutError(f"ImageStatesWrite(test) failed: {write_test}")

        stage = "reset"
        reset = await client.request(ResetWrite(), timeout_s=smp_timeout_s)
        if error(reset):
            raise RolloutError(f"ResetWrite failed: {reset}")

    finally:
        with suppress(Exception):
            await client.disconnect()

    if skip_discovery_on_return:
        stage = "wait-reconnect"
        returned = DiscoveredRobot(
            ip=robot.ip,
            control_port=robot.control_port,
            serial=robot.serial,
            human_name=robot.human_name,
            firmware_version=robot.firmware_version,
            is_claimed=robot.is_claimed,
            claimed_by_ip=robot.claimed_by_ip,
        )
    else:
        stage = "wait-rediscovery"
        returned = wait_for_robot_return(
            wanted_serial=robot.serial,
            wanted_ip=robot.ip,
            discovery_port=discovery_port,
            state_port=state_port,
            discovery_timeout_s=discovery_timeout_s,
            include_localhost=include_localhost,
            max_wait_s=rediscovery_timeout_s,
            retry_interval_s=rediscovery_interval_s,
        )
        if returned is None:
            return RobotResult(
                robot_key=_robot_key(robot),
                ip=robot.ip,
                serial=robot.serial,
                status="FAILED",
                stage=stage,
                message="Robot did not reappear in discovery window after reset",
            )

    stage = "verify-and-confirm"
    verify_transport = SMPUDPTransportWithPort(mtu=mtu, port=smp_port)
    verify_client = SMPClient(
        transport=verify_transport,
        address=returned.ip,
        timeout_s=smp_timeout_s,
    )

    try:
        connected = await _connect_with_retry(
            verify_client,
            max_wait_s=rediscovery_timeout_s,
            retry_interval_s=rediscovery_interval_s,
        )
        if not connected:
            if skip_discovery_on_return:
                return RobotResult(
                    robot_key=_robot_key(robot),
                    ip=returned.ip,
                    serial=returned.serial,
                    status="FAILED",
                    stage=stage,
                    message="Robot did not become reachable over SMP after reset",
                )
            return RobotResult(
                robot_key=_robot_key(robot),
                ip=returned.ip,
                serial=returned.serial,
                status="FAILED",
                stage=stage,
                message="Rediscovered robot did not become reachable over SMP",
            )

        current_images = await _read_image_states(verify_client, smp_timeout_s)

        active_hashes = {
            bytes(i.hash)
            for i in current_images
            if getattr(i, "active", False) and getattr(i, "hash", None) is not None
        }

        if target_hash is None or target_hash not in active_hashes:
            return RobotResult(
                robot_key=_robot_key(robot),
                ip=returned.ip,
                serial=returned.serial,
                status="FAILED",
                stage=stage,
                message=(
                    "Uploaded image is not active after reboot; possible rollback. "
                    f"states={_state_image_summary(current_images)}"
                ),
            )

        confirm = await verify_client.request(
            ImageStatesWrite(confirm=True), timeout_s=smp_timeout_s
        )
        if error(confirm):
            return RobotResult(
                robot_key=_robot_key(robot),
                ip=returned.ip,
                serial=returned.serial,
                status="FAILED",
                stage=stage,
                message=f"ImageStatesWrite(confirm) failed: {confirm}",
            )

        confirmed_images = await _read_image_states(verify_client, smp_timeout_s)

        is_confirmed = False
        for img in confirmed_images:
            if (
                getattr(img, "active", False)
                and getattr(img, "hash", None) is not None
                and bytes(img.hash) == target_hash
                and bool(getattr(img, "confirmed", False))
            ):
                is_confirmed = True
                break

        if not is_confirmed:
            return RobotResult(
                robot_key=_robot_key(robot),
                ip=returned.ip,
                serial=returned.serial,
                status="FAILED",
                stage=stage,
                message=(
                    "Active image was not confirmed after confirm request. "
                    f"states={_state_image_summary(confirmed_images)}"
                ),
            )

        return RobotResult(
            robot_key=_robot_key(robot),
            ip=returned.ip,
            serial=returned.serial,
            status="OK",
            stage="done",
            message=f"Updated and confirmed hash={target_hash.hex()[:16]}",
        )

    except Exception as exc:
        return RobotResult(
            robot_key=_robot_key(robot),
            ip=returned.ip,
            serial=returned.serial,
            status="FAILED",
            stage=stage,
            message=str(exc),
        )

    finally:
        with suppress(Exception):
            await verify_client.disconnect()


def _run_build() -> None:
    print("[build] Running:", " ".join(DEFAULT_BUILD_CMD))
    proc = subprocess.run(DEFAULT_BUILD_CMD, check=False)
    if proc.returncode != 0:
        raise RolloutError(f"Build failed with exit code {proc.returncode}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Fleet OTA rollout using Bonjour + smpclient", allow_abbrev=False
    )
    parser.add_argument(
        "--image",
        type=Path,
        default=DEFAULT_IMAGE_PATH,
        help=f"Path to signed MCUboot image (default: {DEFAULT_IMAGE_PATH})",
    )
    parser.add_argument(
        "--build",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Build firmware before rollout (default: true)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Only build/discover and print targets; do not perform OTA",
    )
    parser.add_argument("--discovery-port", type=int, default=DEFAULT_DISCOVERY_PORT)
    parser.add_argument("--state-port", type=int, default=DEFAULT_STATE_PORT)
    parser.add_argument(
        "--discovery-timeout",
        type=float,
        default=11.0,
        help="Bonjour response window in seconds",
    )
    parser.add_argument(
        "--discovery-rounds",
        type=int,
        default=DEFAULT_DISCOVERY_ROUNDS,
        help="Number of discovery rounds before deciding no robots are available",
    )
    parser.add_argument(
        "--discovery-round-delay",
        type=float,
        default=DEFAULT_DISCOVERY_ROUND_DELAY_S,
        help="Delay in seconds between discovery rounds",
    )
    parser.add_argument(
        "--include-localhost",
        action="store_true",
        help="Include 127.0.0.1 in discovery targets",
    )
    parser.add_argument("--smp-port", type=int, default=DEFAULT_SMP_PORT)
    parser.add_argument("--smp-timeout", type=float, default=4.0)
    parser.add_argument(
        "--mtu",
        type=int,
        default=1200,
        help="SMP UDP MTU used by smpclient transport",
    )
    parser.add_argument(
        "--rediscovery-timeout",
        type=float,
        default=60.0,
        help="Seconds to wait for robot rediscovery after reboot",
    )
    parser.add_argument(
        "--rediscovery-interval",
        type=float,
        default=3.0,
        help="Seconds between rediscovery attempts",
    )
    parser.add_argument(
        "--limit",
        type=str,
        default="",
        help="Comma-separated serial/IP filter for target robots",
    )
    parser.add_argument(
        "--ip",
        type=str,
        default="",
        help="Target exactly one robot by IP and skip discovery",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    initialize_proto_runtime()

    if args.build:
        _run_build()

    if not args.image.exists():
        raise RolloutError(f"Image not found: {args.image}")

    image_bytes = args.image.read_bytes()
    image_sha256 = hashlib.sha256(image_bytes).hexdigest()
    print(f"[image] {args.image} size={len(image_bytes)} sha256={image_sha256}")

    skip_discovery = bool(args.ip)
    if skip_discovery:
        if args.limit:
            raise RolloutError("--ip cannot be combined with --limit")

        try:
            target_ip = str(ipaddress.ip_address(args.ip))
        except ValueError as exc:
            raise RolloutError(f"Invalid --ip value: {args.ip}") from exc

        robots = [
            DiscoveredRobot(
                ip=target_ip,
                control_port=30010,
                serial=target_ip,
                human_name="target-ip",
                firmware_version="unknown",
            )
        ]
        print(f"[target] Using explicit IP: {target_ip} (discovery skipped)")
    else:
        robots = discover_many_rounds(
            discovery_port=args.discovery_port,
            state_port=args.state_port,
            timeout_s=args.discovery_timeout,
            include_localhost=args.include_localhost,
            rounds=args.discovery_rounds,
            round_delay_s=args.discovery_round_delay,
        )

        limit_set = {part.strip() for part in args.limit.split(",") if part.strip()} or None
        robots = _filter_targets(robots, limit_set)

        if not robots:
            print("[discovery] No robots found")
            return 2

        print(f"[discovery] Found {len(robots)} robot(s)")
        for robot in robots:
            print(
                "  - "
                f"serial={robot.serial or 'unknown'} "
                f"name={robot.human_name or 'unnamed'} "
                f"ip={robot.ip} "
                f"claimed={'yes' if robot.is_claimed else 'no'}"
            )

    if args.dry_run:
        print("[dry-run] Skipping OTA stages")
        return 0

    results: list[RobotResult] = []
    for idx, robot in enumerate(robots, start=1):
        print(f"\n[{idx}/{len(robots)}] Updating {_robot_key(robot)} ({robot.ip})")
        try:
            result = asyncio.run(
                _update_robot(
                    robot=robot,
                    image_bytes=image_bytes,
                    smp_port=args.smp_port,
                    smp_timeout_s=args.smp_timeout,
                    mtu=args.mtu,
                    discovery_port=args.discovery_port,
                    state_port=args.state_port,
                    discovery_timeout_s=args.discovery_timeout,
                    include_localhost=args.include_localhost,
                    rediscovery_timeout_s=args.rediscovery_timeout,
                    rediscovery_interval_s=args.rediscovery_interval,
                    skip_discovery_on_return=skip_discovery,
                )
            )
        except Exception as exc:
            result = RobotResult(
                robot_key=_robot_key(robot),
                ip=robot.ip,
                serial=robot.serial,
                status="FAILED",
                stage="exception",
                message=str(exc),
            )

        results.append(result)
        print(
            f"  -> {result.status} stage={result.stage} "
            f"robot={result.robot_key} msg={result.message}"
        )

    ok_count = sum(1 for result in results if result.status == "OK")
    fail_count = len(results) - ok_count

    print("\n=== Rollout summary ===")
    for result in results:
        print(
            f"{result.status:6} {result.robot_key:24} "
            f"stage={result.stage:16} ip={result.ip} {result.message}"
        )
    print(f"Total={len(results)} OK={ok_count} FAILED={fail_count}")

    return 0 if fail_count == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RolloutError as exc:
        print(f"[fatal] {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
