import tkinter as tk
from tkinter import ttk

from model import StateSnapshot


STATE_FIELDS: list[tuple[str, str]] = [
    ("header.stamp", "header_stamp"),
    ("header.frame_id", "header_frame_id"),
    ("effort.header.stamp", "effort_header_stamp"),
    ("effort.header.frame_id", "effort_header_frame_id"),
    ("effort.state.x", "effort_x"),
    ("effort.state.y", "effort_y"),
    ("linear_acceleration.header.stamp", "linear_accel_header_stamp"),
    ("linear_acceleration.header.frame_id", "linear_accel_header_frame_id"),
    ("linear_acceleration.state.x", "linear_accel_x"),
    ("linear_acceleration.state.y", "linear_accel_y"),
    ("linear_acceleration.state.z", "linear_accel_z"),
    ("angular_velocity.header.stamp", "angular_vel_header_stamp"),
    ("angular_velocity.header.frame_id", "angular_vel_header_frame_id"),
    ("angular_velocity.state.x", "angular_vel_x"),
    ("angular_velocity.state.y", "angular_vel_y"),
    ("angular_velocity.state.z", "angular_vel_z"),
    ("magnetic_field.header.stamp", "magnetic_field_header_stamp"),
    ("magnetic_field.header.frame_id", "magnetic_field_header_frame_id"),
    ("magnetic_field.state.x", "magnetic_field_x"),
    ("magnetic_field.state.y", "magnetic_field_y"),
    ("magnetic_field.state.z", "magnetic_field_z"),
    ("distance.header.stamp", "distance_header_stamp"),
    ("distance.header.frame_id", "distance_header_frame_id"),
    ("distance.state", "distance_value"),
]


class KabotIoView:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Kabot IO HMI")
        self.root.geometry("900x760")

        self.left_var = tk.StringVar(value="0.0")
        self.right_var = tk.StringVar(value="0.0")
        self.interval_var = tk.StringVar(value="0.1")
        self.status_var = tk.StringVar(value="Idle")

        self.state_vars: dict[str, tk.StringVar] = {}

        self.on_send_once = None
        self.on_toggle_periodic = None
        self.on_stop = None
        self.on_arrow_press = None
        self.on_arrow_release = None

        self._build_layout()
        self._bind_keyboard()

    def _build_layout(self) -> None:
        outer = ttk.Frame(self.root, padding=12)
        outer.pack(fill="both", expand=True)

        control_frame = ttk.LabelFrame(outer, text="Control", padding=10)
        control_frame.pack(fill="x", padx=2, pady=4)

        ttk.Label(
            control_frame,
            text="Use arrow keys to drive. Keep HMI window focused.",
        ).grid(row=0, column=0, columnspan=8, sticky="w", pady=(0, 8))

        ttk.Label(control_frame, text="Left effort").grid(row=1, column=0, sticky="w")
        ttk.Entry(control_frame, textvariable=self.left_var, width=12).grid(row=1, column=1, padx=6)

        ttk.Label(control_frame, text="Right effort").grid(row=1, column=2, sticky="w")
        ttk.Entry(control_frame, textvariable=self.right_var, width=12).grid(row=1, column=3, padx=6)

        ttk.Label(control_frame, text="Interval [s]").grid(row=1, column=4, sticky="w")
        ttk.Entry(control_frame, textvariable=self.interval_var, width=12).grid(row=1, column=5, padx=6)

        ttk.Button(control_frame, text="Send once", command=self._emit_send_once).grid(
            row=1, column=6, padx=8
        )
        self.periodic_button = ttk.Button(
            control_frame, text="Start periodic", command=self._emit_toggle_periodic
        )
        self.periodic_button.grid(row=1, column=7, padx=8)

        ttk.Label(control_frame, textvariable=self.status_var).grid(
            row=2, column=0, columnspan=8, sticky="w", pady=(8, 0)
        )

        state_frame = ttk.LabelFrame(outer, text="State (read-only)", padding=10)
        state_frame.pack(fill="both", expand=True, padx=2, pady=8)

        canvas = tk.Canvas(state_frame, borderwidth=0, highlightthickness=0)
        scrollbar = ttk.Scrollbar(state_frame, orient="vertical", command=canvas.yview)
        fields_host = ttk.Frame(canvas)

        fields_host.bind(
            "<Configure>",
            lambda _e: canvas.configure(scrollregion=canvas.bbox("all")),
        )

        canvas.create_window((0, 0), window=fields_host, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)

        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        for idx, (field_name, _snapshot_attr) in enumerate(STATE_FIELDS):
            var = tk.StringVar(value="")
            self.state_vars[field_name] = var
            ttk.Label(fields_host, text=field_name, width=34).grid(row=idx, column=0, sticky="w", pady=2)
            entry = ttk.Entry(fields_host, textvariable=var, width=48, state="readonly")
            entry.grid(row=idx, column=1, sticky="we", padx=8, pady=2)

    def _emit_send_once(self) -> None:
        if self.on_send_once is not None:
            self.on_send_once()

    def _emit_toggle_periodic(self) -> None:
        if self.on_toggle_periodic is not None:
            self.on_toggle_periodic()

    def _bind_keyboard(self) -> None:
        self.root.bind("<KeyPress>", self._on_key_press)
        self.root.bind("<KeyRelease>", self._on_key_release)

    def _on_key_press(self, event) -> None:
        if event.keysym.lower() == "q":
            if self.on_stop is not None:
                self.on_stop()
            return

        if self.on_arrow_press is not None:
            self.on_arrow_press(event.keysym)

    def _on_key_release(self, event) -> None:
        if self.on_arrow_release is not None:
            self.on_arrow_release(event.keysym)

    def set_periodic_enabled(self, enabled: bool) -> None:
        self.periodic_button.configure(text="Stop periodic" if enabled else "Start periodic")

    def set_status(self, text: str) -> None:
        self.status_var.set(text)

    def read_control_inputs(self) -> tuple[float, float, float]:
        return float(self.left_var.get()), float(self.right_var.get()), float(self.interval_var.get())

    def read_effort_inputs(self) -> tuple[float, float]:
        return float(self.left_var.get()), float(self.right_var.get())

    def read_interval_input(self) -> float:
        return float(self.interval_var.get())

    def set_control_values(self, left: float, right: float) -> None:
        self.left_var.set(f"{left:.3f}")
        self.right_var.set(f"{right:.3f}")

    def set_state_snapshot(self, snapshot: StateSnapshot) -> None:
        for ui_key, snapshot_attr in STATE_FIELDS:
            self.state_vars[ui_key].set(getattr(snapshot, snapshot_attr))

    def set_close_callback(self, callback) -> None:
        self.root.protocol("WM_DELETE_WINDOW", callback)

    def run(self) -> None:
        self.root.mainloop()
