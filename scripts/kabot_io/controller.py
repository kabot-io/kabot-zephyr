from model import KabotIoModel
from view import KabotIoView


STATUS_PERIODIC_STARTED = "Periodic sending started"
STATUS_PERIODIC_STOPPED = "Periodic sending stopped"
STATUS_INVALID_INPUT = "Invalid input: effort must be float, interval > 0"
STATE_POLL_MS = 20

EFFORT_BY_KEYS = {
    frozenset(): (0.0, 0.0),
    frozenset({"Up"}): (0.5, 0.5),
    frozenset({"Down"}): (-0.5, -0.5),
    frozenset({"Left"}): (-0.5, 0.5),
    frozenset({"Right"}): (0.5, -0.5),
    frozenset({"Up", "Left"}): (0.0, 0.5),
    frozenset({"Up", "Right"}): (0.5, 0.0),
    frozenset({"Down", "Left"}): (0.0, -0.5),
    frozenset({"Down", "Right"}): (-0.5, 0.0),
}


class KabotIoController:
    def __init__(self, model: KabotIoModel, view: KabotIoView):
        self.model = model
        self.view = view
        self.periodic_running = False
        self.periodic_after_id = None
        self.state_poll_after_id = None
        self.active_keys: set[str] = set()

        self.view.on_send_once = self.send_once
        self.view.on_toggle_periodic = self.toggle_periodic
        self.view.on_stop = self.shutdown
        self.view.on_arrow_press = self.handle_arrow_press
        self.view.on_arrow_release = self.handle_arrow_release
        self.view.set_close_callback(self.shutdown)

        self.view.set_state_snapshot(self.model.empty_state_snapshot())
        self._schedule_state_poll()

    def _schedule_state_poll(self) -> None:
        self._poll_state()
        self.state_poll_after_id = self.view.root.after(STATE_POLL_MS, self._schedule_state_poll)

    def _poll_state(self) -> None:
        snapshot = self.model.try_receive_state()
        if snapshot is not None:
            self.view.set_state_snapshot(snapshot)

    def send_once(self) -> None:
        try:
            left, right = self.view.read_effort_inputs()
            self.model.send_control(left, right)
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

            self.model.send_control(left, right)

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
        if self.state_poll_after_id is not None:
            self.view.root.after_cancel(self.state_poll_after_id)
            self.state_poll_after_id = None
        self._stop_periodic()
        self.model.close()
        self.view.root.destroy()
