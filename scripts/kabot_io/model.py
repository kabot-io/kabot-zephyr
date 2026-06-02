import socket
from dataclasses import dataclass

from config import AppConfig
from proto_codec import decode_state_msg, encode_control_effort


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

        self.state_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.state_sock.bind((config.state_bind_host, config.state_port))
        self.state_sock.setblocking(False)

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
        )

    @staticmethod
    def empty_state_snapshot() -> StateSnapshot:
        return StateSnapshot()
