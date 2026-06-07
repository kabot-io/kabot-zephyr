#!/usr/bin/env python3
import argparse
import logging

from config import AppConfig
from controller import KabotIoController
from model import KabotIoModel
from proto_codec import initialize_proto_runtime
from view import KabotIoView


def parse_args() -> AppConfig:
    defaults = AppConfig()
    parser = argparse.ArgumentParser(description="Kabot IO HMI (MVC)")
    parser.add_argument(
        "--host",
        default=defaults.host,
        help=f"Target host/IP (default: {defaults.host})",
    )
    parser.add_argument("--port", type=int, default=defaults.port, help="Target UDP port")
    parser.add_argument(
        "--discovery-port",
        type=int,
        default=defaults.discovery_port,
        help=f"Robot discovery UDP port (default: {defaults.discovery_port})",
    )
    parser.add_argument(
        "--discovery-timeout",
        type=float,
        default=defaults.discovery_timeout_sec,
        help=(
            "Discovery response window in seconds after unicast sweep "
            f"(default: {defaults.discovery_timeout_sec})"
        ),
    )
    parser.add_argument(
        "--no-discovery",
        action="store_true",
        help="Disable unicast discovery sweep and use --host/--port directly",
    )
    parser.add_argument(
        "--no-localhost",
        action="store_true",
        help="Exclude 127.0.0.1 from discovery candidates",
    )
    parser.add_argument(
        "--state-bind-host",
        default=defaults.state_bind_host,
        help="Bind address for incoming State UDP (default: 0.0.0.0)",
    )
    parser.add_argument(
        "--state-port",
        type=int,
        default=defaults.state_port,
        help="Listen UDP port for incoming State (default: 30011)",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=defaults.interval_sec,
        help="Default send interval in seconds",
    )
    parser.add_argument("--quiet", action="store_true", help="Reserved for future logging controls")
    args = parser.parse_args()

    return AppConfig(
        host=args.host,
        port=args.port,
        discovery_port=args.discovery_port,
        discovery_timeout_sec=args.discovery_timeout,
        enable_discovery=not args.no_discovery,
        include_localhost=not args.no_localhost,
        state_bind_host=args.state_bind_host,
        state_port=args.state_port,
        interval_sec=args.interval,
        quiet=args.quiet,
    )


def main() -> None:
    config = parse_args()

    logging.basicConfig(
        level=logging.WARNING if config.quiet else logging.INFO,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )

    initialize_proto_runtime()

    model = KabotIoModel(config)

    view = KabotIoView()
    view.left_var.set("0.0")
    view.right_var.set("0.0")
    view.interval_var.set(f"{config.interval_sec:.3f}")
    view.set_status(
        f"Ready. Control target={model.target[0]}:{model.target[1]}, "
        f"State listen={config.state_bind_host}:{config.state_port}. "
        "Run Scan and Claim Selected to choose a robot."
    )
    view.set_active_robot("Active robot: none")

    controller = KabotIoController(model, view)
    if config.enable_discovery:
        controller.scan_robots()
    view.run()


if __name__ == "__main__":
    main()
