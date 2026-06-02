import socket
from dataclasses import dataclass

from config import AppConfig
from proto_codec import encode_control_effort


@dataclass
class StateSnapshot:
    header_stamp: str = ""
    header_frame_id: str = ""

    effort_header_stamp: str = ""
    effort_header_frame_id: str = ""
    effort_x: str = ""
    effort_y: str = ""

    linear_accel_header_stamp: str = ""
    linear_accel_header_frame_id: str = ""
    linear_accel_x: str = ""
    linear_accel_y: str = ""
    linear_accel_z: str = ""

    angular_vel_header_stamp: str = ""
    angular_vel_header_frame_id: str = ""
    angular_vel_x: str = ""
    angular_vel_y: str = ""
    angular_vel_z: str = ""

    magnetic_field_header_stamp: str = ""
    magnetic_field_header_frame_id: str = ""
    magnetic_field_x: str = ""
    magnetic_field_y: str = ""
    magnetic_field_z: str = ""

    distance_header_stamp: str = ""
    distance_header_frame_id: str = ""
    distance_value: str = ""


class KabotIoModel:
    def __init__(self, config: AppConfig):
        self.config = config
        self.target = (config.host, config.port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sent_count = 0

    def send_control(self, left: float, right: float) -> None:
        payload = encode_control_effort(left, right)
        self.sock.sendto(payload, self.target)
        self.sent_count += 1

    def send_stop_sequence(self) -> None:
        payload = encode_control_effort(0.0, 0.0)
        for _ in range(3):
            self.sock.sendto(payload, self.target)

    def close(self) -> None:
        self.send_stop_sequence()
        self.sock.close()

    @staticmethod
    def empty_state_snapshot() -> StateSnapshot:
        return StateSnapshot()
