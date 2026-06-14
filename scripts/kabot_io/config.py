from dataclasses import dataclass


@dataclass(frozen=True)
class AppConfig:
    host: str = "127.0.0.1"
    port: int = 30010
    discovery_port: int = 30012
    discovery_timeout_sec: float = 3.0
    enable_discovery: bool = True
    include_localhost: bool = True
    state_bind_host: str = "0.0.0.0"
    state_port: int = 30011
    interval_sec: float = 0.1
    quiet: bool = False
