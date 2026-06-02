#!/usr/bin/env python3
import argparse

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
        "--interval",
        type=float,
        default=defaults.interval_sec,
        help="Default send interval in seconds",
    )
    parser.add_argument("--quiet", action="store_true", help="Reserved for future logging controls")
    args = parser.parse_args()

    return AppConfig(host=args.host, port=args.port, interval_sec=args.interval, quiet=args.quiet)


def main() -> None:
    config = parse_args()

    initialize_proto_runtime()

    model = KabotIoModel(config)
    view = KabotIoView()
    view.left_var.set("0.0")
    view.right_var.set("0.0")
    view.interval_var.set(f"{config.interval_sec:.3f}")
    view.set_status(f"Ready. Target: {config.host}:{config.port}")

    KabotIoController(model, view)
    view.run()


if __name__ == "__main__":
    main()
