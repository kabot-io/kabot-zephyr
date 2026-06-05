import socket
from dataclasses import dataclass

from config import AppConfig
from proto_codec import decode_state_msg, encode_control_effort


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
