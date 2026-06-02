import importlib.util
import subprocess
import sys
from pathlib import Path
from types import ModuleType


_PROTO_MODULE: ModuleType | None = None


def _paths() -> tuple[Path, Path, Path]:
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent.parent
    proto_file = repo_root / "app" / "protos" / "state_control_msg.proto"
    generated_dir = script_dir / "_generated"
    module_file = generated_dir / "state_control_msg_pb2.py"
    return proto_file, generated_dir, module_file


def initialize_proto_runtime() -> None:
    global _PROTO_MODULE
    if _PROTO_MODULE is not None:
        return

    proto_file, generated_dir, module_file = _paths()

    if not proto_file.exists():
        raise RuntimeError(f"Proto file not found: {proto_file}")

    generated_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        sys.executable,
        "-m",
        "grpc_tools.protoc",
        f"-I{proto_file.parent}",
        f"--python_out={generated_dir}",
        str(proto_file),
    ]

    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        stderr = proc.stderr.strip()
        stdout = proc.stdout.strip()
        details = stderr or stdout or "unknown protoc error"
        raise RuntimeError(f"Failed to compile proto: {details}")

    if not module_file.exists():
        raise RuntimeError(f"Generated module not found after compile: {module_file}")

    spec = importlib.util.spec_from_file_location("state_control_msg_pb2", module_file)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Failed to load generated module spec: {module_file}")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    _PROTO_MODULE = module


def _proto_module() -> ModuleType:
    initialize_proto_runtime()
    assert _PROTO_MODULE is not None
    return _PROTO_MODULE


def encode_control_effort(left: float, right: float) -> bytes:
    pb2 = _proto_module()
    msg = pb2.Control()
    msg.effort.state.x = left
    msg.effort.state.y = right
    return msg.SerializeToString()


def decode_state_msg(payload: bytes):
    pb2 = _proto_module()
    msg = pb2.State()
    msg.ParseFromString(payload)
    return msg
