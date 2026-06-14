import errno
import ipaddress
import json
import logging
import socket
import subprocess
import time
from collections.abc import Iterable
from dataclasses import dataclass

from config import AppConfig
from proto_codec import (
    decode_bonjour_response,
    decode_state_msg,
    encode_bonjour,
    encode_control_effort,
)

LOGGER = logging.getLogger(__name__)

MIN_DISCOVERY_RESPONSE_WINDOW_SEC = 0.8
LOCALHOST_GRACE_WINDOW_SEC = 0.12


@dataclass
class DiscoveredRobot:
    ip: str
    control_port: int
    serial: str
    human_name: str
    firmware_version: str
    is_claimed: bool = False
    claimed_by_ip: str = ""


@dataclass(frozen=True)
class InterfaceSubnet:
    ifname: str
    local_ip: str
    prefixlen: int
    host_count: int


@dataclass
class StateSnapshot:
    # The *_hz fields are UI-derived values computed in the controller.
    header_stamp: str = ""
    header_frame_id: str = ""
    header_hz: str = ""

    effort_header_stamp: str = ""
    effort_header_frame_id: str = ""
    effort_header_hz: str = ""
    effort_x: str = ""
    effort_y: str = ""

    linear_accel_header_stamp: str = ""
    linear_accel_header_frame_id: str = ""
    linear_accel_header_hz: str = ""
    linear_accel_x: str = ""
    linear_accel_y: str = ""
    linear_accel_z: str = ""

    angular_vel_header_stamp: str = ""
    angular_vel_header_frame_id: str = ""
    angular_vel_header_hz: str = ""
    angular_vel_x: str = ""
    angular_vel_y: str = ""
    angular_vel_z: str = ""

    magnetic_field_header_stamp: str = ""
    magnetic_field_header_frame_id: str = ""
    magnetic_field_header_hz: str = ""
    magnetic_field_x: str = ""
    magnetic_field_y: str = ""
    magnetic_field_z: str = ""

    distance_header_stamp: str = ""
    distance_header_frame_id: str = ""
    distance_header_hz: str = ""
    distance_value: str = ""

    light_left_header_stamp: str = ""
    light_left_header_frame_id: str = ""
    light_left_header_hz: str = ""
    light_left_value: str = ""

    light_right_header_stamp: str = ""
    light_right_header_frame_id: str = ""
    light_right_header_hz: str = ""
    light_right_value: str = ""

    current_left_header_stamp: str = ""
    current_left_header_frame_id: str = ""
    current_left_header_hz: str = ""
    current_left_value: str = ""

    bus_voltage_left_header_stamp: str = ""
    bus_voltage_left_header_frame_id: str = ""
    bus_voltage_left_header_hz: str = ""
    bus_voltage_left_value: str = ""

    power_left_header_stamp: str = ""
    power_left_header_frame_id: str = ""
    power_left_header_hz: str = ""
    power_left_value: str = ""

    current_right_header_stamp: str = ""
    current_right_header_frame_id: str = ""
    current_right_header_hz: str = ""
    current_right_value: str = ""

    bus_voltage_right_header_stamp: str = ""
    bus_voltage_right_header_frame_id: str = ""
    bus_voltage_right_header_hz: str = ""
    bus_voltage_right_value: str = ""

    power_right_header_stamp: str = ""
    power_right_header_frame_id: str = ""
    power_right_header_hz: str = ""
    power_right_value: str = ""

    current_supply_header_stamp: str = ""
    current_supply_header_frame_id: str = ""
    current_supply_header_hz: str = ""
    current_supply_value: str = ""

    bus_voltage_supply_header_stamp: str = ""
    bus_voltage_supply_header_frame_id: str = ""
    bus_voltage_supply_header_hz: str = ""
    bus_voltage_supply_value: str = ""

    power_supply_header_stamp: str = ""
    power_supply_header_frame_id: str = ""
    power_supply_header_hz: str = ""
    power_supply_value: str = ""


class KabotIoModel:
    def __init__(self, config: AppConfig):
        self.config = config
        self.target = (config.host, config.port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        self.state_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.state_sock.bind((config.state_bind_host, config.state_port))
        self.state_sock.setblocking(False)

        self.sent_count = 0
        self.claimed_robot: DiscoveredRobot | None = None

    @staticmethod
    def _linux_ipv4_interfaces() -> list[InterfaceSubnet]:
        cmd = ["ip", "-j", "-4", "addr", "show", "up"]
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=5.0)
        except FileNotFoundError:
            LOGGER.warning("Discovery interface query failed: 'ip' command not found")
            return []
        except subprocess.TimeoutExpired:
            LOGGER.warning("Discovery interface query timed out")
            return []
        if proc.returncode != 0:
            LOGGER.warning("Discovery interface query failed: rc=%d", proc.returncode)
            return []

        try:
            data = json.loads(proc.stdout)
        except json.JSONDecodeError:
            LOGGER.warning("Discovery interface query produced invalid JSON")
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
                host_count = KabotIoModel._host_count_for_prefix(int(prefixlen))
                subnets.append(
                    InterfaceSubnet(
                        ifname=ifname,
                        local_ip=local,
                        prefixlen=int(prefixlen),
                        host_count=host_count,
                    )
                )

        return subnets

    @staticmethod
    def _host_count_for_prefix(prefixlen: int) -> int:
        if prefixlen >= 32:
            return 1
        if prefixlen == 31:
            return 2
        return max(0, (1 << (32 - prefixlen)) - 2)

    @staticmethod
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

    def _iter_discovery_candidates(self) -> Iterable[tuple[str, InterfaceSubnet | None]]:
        seen: set[str] = set()

        if self.config.include_localhost:
            seen.add("127.0.0.1")
            yield ("127.0.0.1", None)

        subnets = sorted(
            self._linux_ipv4_interfaces(),
            key=lambda item: (item.host_count, item.ifname, item.local_ip),
        )

        for subnet in subnets:
            LOGGER.info(
                "Discovery scanning subnet if=%s cidr=%s/%d hosts=%d",
                subnet.ifname,
                subnet.local_ip,
                subnet.prefixlen,
                subnet.host_count,
            )

            scanned = 0
            for host in self._iter_hosts_for_cidr(subnet.local_ip, subnet.prefixlen):
                if host in seen:
                    continue
                seen.add(host)
                scanned += 1
                if (scanned % 1024) == 0:
                    LOGGER.info(
                        "Discovery progress if=%s cidr=%s/%d scanned=%d/%d",
                        subnet.ifname,
                        subnet.local_ip,
                        subnet.prefixlen,
                        scanned,
                        subnet.host_count,
                    )
                yield (host, subnet)

            LOGGER.info(
                "Discovery finished subnet if=%s cidr=%s/%d scanned=%d",
                subnet.ifname,
                subnet.local_ip,
                subnet.prefixlen,
                scanned,
            )

    def _try_receive_discovery_response(self, discover_sock: socket.socket) -> DiscoveredRobot | None:
        try:
            response, addr = discover_sock.recvfrom(2048)
        except (BlockingIOError, TimeoutError, OSError):
            return None

        try:
            msg = decode_bonjour_response(response)
        except Exception:
            return None

        control_port = int(msg.control_port) if msg.control_port > 0 else self.config.port
        discovered = DiscoveredRobot(
            ip=addr[0],
            control_port=control_port,
            serial=msg.serial,
            human_name=msg.human_name,
            firmware_version=msg.firmware_version,
            is_claimed=bool(getattr(msg, "is_claimed", False)),
            claimed_by_ip=str(getattr(msg, "claimed_by_ip", "")),
        )

        return discovered

    @staticmethod
    def _robot_key(robot: DiscoveredRobot) -> tuple[str, str]:
        serial_key = (robot.serial or "").strip()
        if serial_key:
            return ("serial", serial_key)
        return ("endpoint", f"{robot.ip}:{robot.control_port}")

    def discover_many(self) -> list[DiscoveredRobot]:
        if not self.config.enable_discovery:
            return []

        payload = encode_bonjour(self.config.state_port, claim=False)
        response_window_sec = max(MIN_DISCOVERY_RESPONSE_WINDOW_SEC, self.config.discovery_timeout_sec)
        deadline = time.monotonic() + response_window_sec
        LOGGER.info(
            "Discovery started: port=%d timeout=%.3fs (effective=%.3fs) include_localhost=%s",
            self.config.discovery_port,
            self.config.discovery_timeout_sec,
            response_window_sec,
            self.config.include_localhost,
        )

        found: dict[tuple[str, str], DiscoveredRobot] = {}

        def collect_available(sock: socket.socket) -> None:
            while True:
                discovered = self._try_receive_discovery_response(sock)
                if discovered is None:
                    return
                key = self._robot_key(discovered)
                found[key] = discovered
                LOGGER.info(
                    "Discovery found robot serial=%s name=%s endpoint=%s:%d",
                    discovered.serial or "unknown",
                    discovered.human_name or "unnamed",
                    discovered.ip,
                    discovered.control_port,
                )

        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as discover_sock:
            discover_sock.bind(("0.0.0.0", 0))
            discover_sock.setblocking(False)

            sent_count = 0
            for host, subnet in self._iter_discovery_candidates():
                if time.monotonic() >= deadline:
                    LOGGER.info("Discovery timed out during sweep after %d sends", sent_count)
                    break

                try:
                    discover_sock.sendto(payload, (host, self.config.discovery_port))
                    sent_count += 1

                except OSError as exc:
                    LOGGER.warning("Discovery send failed for %s: %s", host, exc)
                    if exc.errno in (errno.ENOBUFS, errno.EAGAIN, errno.EWOULDBLOCK):
                        time.sleep(0.001)
                    continue

                if subnet is None:
                    LOGGER.info("Discovery sent localhost probe to %s", host)
                    localhost_deadline = min(deadline, time.monotonic() + LOCALHOST_GRACE_WINDOW_SEC)
                    while time.monotonic() < localhost_deadline:
                        collect_available(discover_sock)
                        time.sleep(0.01)

                collect_available(discover_sock)

            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break

                discover_sock.settimeout(remaining)
                discovered = self._try_receive_discovery_response(discover_sock)
                if discovered is not None:
                    key = self._robot_key(discovered)
                    found[key] = discovered
                    LOGGER.info(
                        "Discovery found robot serial=%s name=%s endpoint=%s:%d",
                        discovered.serial or "unknown",
                        discovered.human_name or "unnamed",
                        discovered.ip,
                        discovered.control_port,
                    )

        robots = list(found.values())
        robots.sort(key=lambda item: (item.serial or "", item.human_name or "", item.ip, item.control_port))
        LOGGER.info("Discovery completed with %d unique robot(s)", len(robots))
        return robots

    def claim_robot(self, robot: DiscoveredRobot) -> DiscoveredRobot | None:
        payload = encode_bonjour(self.config.state_port, claim=True, release=False)
        timeout = max(0.2, self.config.discovery_timeout_sec)

        LOGGER.info("Claim started for robot endpoint=%s:%d", robot.ip, self.config.discovery_port)

        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as claim_sock:
            claim_sock.bind(("0.0.0.0", 0))
            claim_sock.settimeout(timeout)

            try:
                claim_sock.sendto(payload, (robot.ip, self.config.discovery_port))
            except OSError as exc:
                LOGGER.warning("Claim send failed for %s: %s", robot.ip, exc)
                return None

            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                try:
                    response, addr = claim_sock.recvfrom(2048)
                except TimeoutError:
                    return None
                except OSError:
                    return None

                if addr[0] != robot.ip:
                    continue

                try:
                    msg = decode_bonjour_response(response)
                except Exception:
                    continue

                control_port = int(msg.control_port) if msg.control_port > 0 else robot.control_port
                claimed = DiscoveredRobot(
                    ip=addr[0],
                    control_port=control_port,
                    serial=msg.serial,
                    human_name=msg.human_name,
                    firmware_version=msg.firmware_version,
                    is_claimed=bool(getattr(msg, "is_claimed", False)),
                    claimed_by_ip=str(getattr(msg, "claimed_by_ip", "")),
                )

                self.target = (claimed.ip, claimed.control_port)
                self.claimed_robot = claimed
                LOGGER.info(
                    "Claim success serial=%s name=%s target=%s:%d",
                    claimed.serial or "unknown",
                    claimed.human_name or "unnamed",
                    claimed.ip,
                    claimed.control_port,
                )
                return claimed

        return None

    def release_robot_stream(self, robot: DiscoveredRobot) -> bool:
        payload = encode_bonjour(self.config.state_port, claim=False, release=True)
        timeout = max(0.2, self.config.discovery_timeout_sec)

        LOGGER.info("Release started for robot endpoint=%s:%d", robot.ip, self.config.discovery_port)

        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as release_sock:
            release_sock.bind(("0.0.0.0", 0))
            release_sock.settimeout(timeout)

            try:
                release_sock.sendto(payload, (robot.ip, self.config.discovery_port))
            except OSError as exc:
                LOGGER.warning("Release send failed for %s: %s", robot.ip, exc)
                return False

            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                try:
                    response, addr = release_sock.recvfrom(2048)
                except TimeoutError:
                    return False
                except OSError:
                    return False

                if addr[0] != robot.ip:
                    continue

                try:
                    _ = decode_bonjour_response(response)
                except Exception:
                    continue

                if self.claimed_robot and self.claimed_robot.ip == robot.ip:
                    self.claimed_robot = None

                LOGGER.info("Release success for robot endpoint=%s", robot.ip)
                return True

        return False

    def discover_and_bind(self) -> DiscoveredRobot | None:
        robots = self.discover_many()
        if not robots:
            return None
        return self.claim_robot(robots[0])

    def send_control(self, left: float, right: float) -> None:
        payload = encode_control_effort(left, right)
        self.sock.sendto(payload, self.target)
        self.sent_count += 1

    def send_stop_sequence(self) -> None:
        payload = encode_control_effort(0.0, 0.0)
        for _ in range(3):
            self.sock.sendto(payload, self.target)

    def close(self) -> None:
        try:
            self.send_stop_sequence()
        finally:
            self.sock.close()
            self.state_sock.close()

    def try_receive_state(self) -> StateSnapshot | None:
        try:
            payload, _addr = self.state_sock.recvfrom(2048)
        except (BlockingIOError, OSError):
            return None

        try:
            state = decode_state_msg(payload)
        except Exception:
            return None

        return StateSnapshot(
            header_stamp=str(state.header.stamp),
            header_frame_id=state.header.frame_id,
            effort_header_stamp=str(state.effort.header.stamp),
            effort_header_frame_id=state.effort.header.frame_id,
            effort_x=f"{state.effort.state.x:.3f}",
            effort_y=f"{state.effort.state.y:.3f}",
            linear_accel_header_stamp=str(state.linear_acceleration.header.stamp),
            linear_accel_header_frame_id=state.linear_acceleration.header.frame_id,
            linear_accel_x=f"{state.linear_acceleration.state.x:.3f}",
            linear_accel_y=f"{state.linear_acceleration.state.y:.3f}",
            linear_accel_z=f"{state.linear_acceleration.state.z:.3f}",
            angular_vel_header_stamp=str(state.angular_velocity.header.stamp),
            angular_vel_header_frame_id=state.angular_velocity.header.frame_id,
            angular_vel_x=f"{state.angular_velocity.state.x:.3f}",
            angular_vel_y=f"{state.angular_velocity.state.y:.3f}",
            angular_vel_z=f"{state.angular_velocity.state.z:.3f}",
            magnetic_field_header_stamp=str(state.magnetic_field.header.stamp),
            magnetic_field_header_frame_id=state.magnetic_field.header.frame_id,
            magnetic_field_x=f"{state.magnetic_field.state.x:.3f}",
            magnetic_field_y=f"{state.magnetic_field.state.y:.3f}",
            magnetic_field_z=f"{state.magnetic_field.state.z:.3f}",
            distance_header_stamp=str(state.distance.header.stamp),
            distance_header_frame_id=state.distance.header.frame_id,
            distance_value=f"{state.distance.state:.3f}",
            light_left_header_stamp=str(state.light_left.header.stamp),
            light_left_header_frame_id=state.light_left.header.frame_id,
            light_left_value=f"{state.light_left.state:.3f}",
            light_right_header_stamp=str(state.light_right.header.stamp),
            light_right_header_frame_id=state.light_right.header.frame_id,
            light_right_value=f"{state.light_right.state:.3f}",
            current_left_header_stamp=str(state.current_left.header.stamp),
            current_left_header_frame_id=state.current_left.header.frame_id,
            current_left_value=f"{state.current_left.state:.3f}",
            bus_voltage_left_header_stamp=str(state.bus_voltage_left.header.stamp),
            bus_voltage_left_header_frame_id=state.bus_voltage_left.header.frame_id,
            bus_voltage_left_value=f"{state.bus_voltage_left.state:.3f}",
            power_left_header_stamp=str(state.power_left.header.stamp),
            power_left_header_frame_id=state.power_left.header.frame_id,
            power_left_value=f"{state.power_left.state:.3f}",
            current_right_header_stamp=str(state.current_right.header.stamp),
            current_right_header_frame_id=state.current_right.header.frame_id,
            current_right_value=f"{state.current_right.state:.3f}",
            bus_voltage_right_header_stamp=str(state.bus_voltage_right.header.stamp),
            bus_voltage_right_header_frame_id=state.bus_voltage_right.header.frame_id,
            bus_voltage_right_value=f"{state.bus_voltage_right.state:.3f}",
            power_right_header_stamp=str(state.power_right.header.stamp),
            power_right_header_frame_id=state.power_right.header.frame_id,
            power_right_value=f"{state.power_right.state:.3f}",
            current_supply_header_stamp=str(state.current_supply.header.stamp),
            current_supply_header_frame_id=state.current_supply.header.frame_id,
            current_supply_value=f"{state.current_supply.state:.3f}",
            bus_voltage_supply_header_stamp=str(state.bus_voltage_supply.header.stamp),
            bus_voltage_supply_header_frame_id=state.bus_voltage_supply.header.frame_id,
            bus_voltage_supply_value=f"{state.bus_voltage_supply.state:.3f}",
            power_supply_header_stamp=str(state.power_supply.header.stamp),
            power_supply_header_frame_id=state.power_supply.header.frame_id,
            power_supply_value=f"{state.power_supply.state:.3f}",
        )

    @staticmethod
    def empty_state_snapshot() -> StateSnapshot:
        return StateSnapshot()
