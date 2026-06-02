from dataclasses import dataclass


@dataclass(frozen=True)
class AppConfig:
    host: str = "127.0.0.1"
    port: int = 30010
    state_bind_host: str = "0.0.0.0"
    state_port: int = 30011
    interval_sec: float = 0.1
    quiet: bool = False
