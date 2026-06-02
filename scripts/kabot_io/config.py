from dataclasses import dataclass


@dataclass(frozen=True)
class AppConfig:
    host: str = "127.0.0.1"
    port: int = 30010
    interval_sec: float = 0.1
    quiet: bool = False
