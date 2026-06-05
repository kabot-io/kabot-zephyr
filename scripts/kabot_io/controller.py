from collections import deque
import time

from model import KabotIoModel
from state_fields import HEADER_HZ_PAIRS
from view import KabotIoView


STATUS_PERIODIC_STARTED = "Periodic sending started"
STATUS_PERIODIC_STOPPED = "Periodic sending stopped"
STATUS_INVALID_INPUT = "Invalid input: effort must be float, interval > 0"
STATE_POLL_MS = 20
HZ_CLEAR_TIMEOUT_MS = 2000
HZ_MOVING_AVERAGE_SAMPLES = 5

EFFORT_BY_KEYS = {
    frozenset(): (0.0, 0.0),
    frozenset({"Up"}): (1.0, 1.0),
    frozenset({"Down"}): (-1.0, -1.0),
    frozenset({"Left"}): (-1.0, 1.0),
    frozenset({"Right"}): (1.0, -1.0),
    frozenset({"Up", "Left"}): (0.0, 1.0),
    frozenset({"Up", "Right"}): (1.0, 0.0),
    frozenset({"Down", "Left"}): (0.0, -1.0),
    frozenset({"Down", "Right"}): (-1.0, 0.0),
}

effort_scale = (1.0, 1.0)

class KabotIoController:
    def __init__(self, model: KabotIoModel, view: KabotIoView):
        self.model = model
        self.view = view
        self.periodic_running = False
        self.periodic_after_id = None
        self.state_poll_after_id = None
        self.active_keys: set[str] = set()
        self._current_snapshot = self.model.empty_state_snapshot()
        self._last_header_stamps: dict[str, int] = {}
        self._last_header_hz: dict[str, str] = {}
        self._hz_clear_after_ids: dict[str, str] = {}
        self._header_hz_samples: dict[str, deque[float]] = {}

        self.view.on_send_once = self.send_once
        self.view.on_toggle_periodic = self.toggle_periodic
        self.view.on_toggle_plots = self.toggle_plots
        self.view.on_stop = self.shutdown
        self.view.on_arrow_press = self.handle_arrow_press
        self.view.on_arrow_release = self.handle_arrow_release
        self.view.set_close_callback(self.shutdown)

        self.view.set_state_snapshot(self._current_snapshot)
        self._schedule_state_poll()

    def _schedule_state_poll(self) -> None:
        self._poll_state()
        self.state_poll_after_id = self.view.root.after(STATE_POLL_MS, self._schedule_state_poll)

    def _poll_state(self) -> None:
        snapshot = self.model.try_receive_state()
        if snapshot is not None:
            self._current_snapshot = snapshot
            self._populate_header_hz(self._current_snapshot)
            sample_time_sec = self._sample_time_sec(self._current_snapshot)
            if self.view.plots_enabled():
                self.view.add_plot_sample(self._current_snapshot, sample_time_sec)
            self.view.set_state_snapshot(self._current_snapshot)

    def toggle_plots(self) -> None:
        if not self.view.plots_enabled():
            self.view.clear_plots()

    @staticmethod
    def _sample_time_sec(snapshot) -> float:
        try:
            stamp_ms = int(getattr(snapshot, "header_stamp", ""))
            if stamp_ms > 0:
                return stamp_ms / 1000.0
        except (TypeError, ValueError):
            pass
        return time.monotonic()

    def _cancel_hz_clear_timer(self, stamp_attr: str) -> None:
        after_id = self._hz_clear_after_ids.pop(stamp_attr, None)
        if after_id is not None:
            self.view.root.after_cancel(after_id)

    def _clear_hz_value(self, stamp_attr: str, hz_attr: str) -> None:
        self._hz_clear_after_ids.pop(stamp_attr, None)
        self._last_header_hz[stamp_attr] = ""
        setattr(self._current_snapshot, hz_attr, "")
        self.view.set_state_snapshot(self._current_snapshot)

    def _refresh_hz_clear_timer(self, stamp_attr: str, hz_attr: str) -> None:
        self._cancel_hz_clear_timer(stamp_attr)
        after_id = self.view.root.after(
            HZ_CLEAR_TIMEOUT_MS,
            lambda sa=stamp_attr, ha=hz_attr: self._clear_hz_value(sa, ha),
        )
        self._hz_clear_after_ids[stamp_attr] = after_id

    def _populate_header_hz(self, snapshot) -> None:
        for stamp_attr, hz_attr in HEADER_HZ_PAIRS:
            stamp_text = getattr(snapshot, stamp_attr)
            hz_value = self._last_header_hz.get(stamp_attr, "")

            try:
                current_stamp = int(stamp_text)
            except (TypeError, ValueError):
                self._last_header_stamps.pop(stamp_attr, None)
                self._cancel_hz_clear_timer(stamp_attr)
                self._header_hz_samples.pop(stamp_attr, None)
                self._last_header_hz[stamp_attr] = ""
                setattr(snapshot, hz_attr, "")
                continue

            previous_stamp = self._last_header_stamps.get(stamp_attr)

            if previous_stamp is None:
                self._last_header_stamps[stamp_attr] = current_stamp
                hz_value = ""
            elif current_stamp == previous_stamp:
                pass
            else:
                delta_ms = current_stamp - previous_stamp
                if delta_ms > 0:
                    hz_samples = self._header_hz_samples.get(stamp_attr)
                    if hz_samples is None:
                        hz_samples = deque(maxlen=HZ_MOVING_AVERAGE_SAMPLES)
                        self._header_hz_samples[stamp_attr] = hz_samples

                    hz_samples.append(1000.0 / float(delta_ms))
                    avg_hz = sum(hz_samples) / float(len(hz_samples))
                    hz_value = f"{avg_hz:.2f}"
                    self._refresh_hz_clear_timer(stamp_attr, hz_attr)
                else:
                    hz_value = ""
                    self._cancel_hz_clear_timer(stamp_attr)
                    self._header_hz_samples.pop(stamp_attr, None)
                self._last_header_stamps[stamp_attr] = current_stamp

            self._last_header_hz[stamp_attr] = hz_value

            setattr(snapshot, hz_attr, hz_value)

    def send_once(self) -> None:
        try:
            left, right = self.view.read_effort_inputs()
            self.model.send_control(left*effort_scale[0], right*effort_scale[1])
            self.view.set_status(
                f"Sent #{self.model.sent_count} -> left={left:.3f}, right={right:.3f}, "
                f"target={self.model.target[0]}:{self.model.target[1]}"
            )
        except ValueError:
            self.view.set_status("Invalid numeric input in control fields")
        except OSError as exc:
            self.view.set_status(f"UDP send failed: {exc}")

    def handle_arrow_press(self, key: str) -> None:
        if key not in ("Up", "Down", "Left", "Right"):
            return

        if key in self.active_keys:
            return

        self.active_keys.add(key)
        self._apply_active_keys()

    def handle_arrow_release(self, key: str) -> None:
        if key not in self.active_keys:
            return

        self.active_keys.remove(key)
        self._apply_active_keys()

    def _apply_active_keys(self) -> None:
        left, right = self._effort_for_active_keys(self.active_keys)
        self.view.set_control_values(left, right)

    @staticmethod
    def _effort_for_active_keys(active_keys: set[str]) -> tuple[float, float]:
        return EFFORT_BY_KEYS.get(frozenset(active_keys), (0.0, 0.0))

    def toggle_periodic(self) -> None:
        if self.periodic_running:
            self._stop_periodic()
            return

        self.periodic_running = True
        self.view.set_periodic_enabled(True)
        self.view.set_status(STATUS_PERIODIC_STARTED)
        self._tick_periodic()

    def _tick_periodic(self) -> None:
        if not self.periodic_running:
            return

        try:
            interval_sec = self.view.read_interval_input()
            if interval_sec <= 0.0:
                raise ValueError("interval must be > 0")

            left, right = self._effort_for_active_keys(self.active_keys)
            self.view.set_control_values(left, right)

            self.model.send_control(left * effort_scale[0], right * effort_scale[1])

            delay_ms = max(1, int(interval_sec * 1000.0))
            self.periodic_after_id = self.view.root.after(delay_ms, self._tick_periodic)
        except ValueError:
            self.view.set_status(STATUS_INVALID_INPUT)
            self._stop_periodic()
        except OSError as exc:
            self.view.set_status(f"UDP send failed: {exc}")
            self._stop_periodic()

    def _stop_periodic(self) -> None:
        self.periodic_running = False
        if self.periodic_after_id is not None:
            self.view.root.after_cancel(self.periodic_after_id)
            self.periodic_after_id = None
        self.view.set_periodic_enabled(False)
        self.view.set_status(STATUS_PERIODIC_STOPPED)

    def shutdown(self) -> None:
        for stamp_attr in list(self._hz_clear_after_ids.keys()):
            self._cancel_hz_clear_timer(stamp_attr)
        if self.state_poll_after_id is not None:
            self.view.root.after_cancel(self.state_poll_after_id)
            self.state_poll_after_id = None
        self._stop_periodic()
        self.model.close()
        self.view.root.destroy()
