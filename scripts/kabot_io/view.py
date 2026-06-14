import tkinter as tk
from collections import deque
from tkinter import ttk

from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
from model import DiscoveredRobot, StateSnapshot
from state_fields import STATE_FIELDS

PLOT_WINDOW_SEC = 10.0
PLOT_WIDTH = 460
PLOT_HEIGHT = 120
PLOT_BG_COLOR = "#ffffff"
PLOT_BORDER_COLOR = "#cbd5e1"
PLOT_ZERO_LINE_COLOR = "#94a3b8"
PLOT_LABEL_COLOR = "#334155"
PLOT_LINE_ALPHA = 0.5
PLOT_DPI = 100
PLOT_REDRAW_INTERVAL_MS = 33

# Axis color mapping used consistently across plots.
PLOT_COLOR_X = "#ec4899"  # preferred red tone
PLOT_COLOR_Y = "#16a34a"  # saturated green tone
PLOT_COLOR_Z = "#2563eb"  # preferred blue tone

PLOT_DEFINITIONS: tuple[
    tuple[str, str, tuple[str, ...], tuple[str, ...], tuple[float, float]],
    ...,
] = (
    (
        "effort",
        "Effort (Vector2)",
        ("effort_x", "effort_y"),
        (PLOT_COLOR_X, PLOT_COLOR_Y),
        (-1.2, 1.2),
    ),
    (
        "linear_accel",
        "Linear Acceleration (Vector3)",
        ("linear_accel_x", "linear_accel_y", "linear_accel_z"),
        (PLOT_COLOR_X, PLOT_COLOR_Y, PLOT_COLOR_Z),
        (-10.0, 10.0),
    ),
    (
        "angular_vel",
        "Angular Velocity (Vector3)",
        ("angular_vel_x", "angular_vel_y", "angular_vel_z"),
        (PLOT_COLOR_X, PLOT_COLOR_Y, PLOT_COLOR_Z),
        (-1.5, 1.5),
    ),
    (
        "magnetic_field",
        "Magnetic Field (Vector3)",
        ("magnetic_field_x", "magnetic_field_y", "magnetic_field_z"),
        (PLOT_COLOR_X, PLOT_COLOR_Y, PLOT_COLOR_Z),
        (-0.8, 0.8),
    ),
    ("distance", "Distance (Scalar)", ("distance_value",), (PLOT_COLOR_Z,), (0.0, 0.6)),
    (
        "light",
        "Ambient Light (Left/Right)",
        ("light_left_value", "light_right_value"),
        (PLOT_COLOR_Y, PLOT_COLOR_X),
        (0.0, 5000.0),
    ),
    (
        "current_lr",
        "Current (Left/Right)",
        ("current_left_value", "current_right_value"),
        (PLOT_COLOR_X, PLOT_COLOR_Y),
        (0.0, 0.5),
    ),
    (
        "bus_voltage_lr",
        "Bus Voltage (Left/Right)",
        ("bus_voltage_left_value", "bus_voltage_right_value"),
        (PLOT_COLOR_X, PLOT_COLOR_Y),
        (0.0, 6.0),
    ),
    (
        "power_lr",
        "Power (Left/Right)",
        ("power_left_value", "power_right_value"),
        (PLOT_COLOR_X, PLOT_COLOR_Y),
        (0.0, 60.0),
    ),
    (
        "current_supply",
        "Current (Supply)",
        ("current_supply_value",),
        (PLOT_COLOR_Z,),
        (0.0, 0.8),
    ),
    (
        "bus_voltage_supply",
        "Bus Voltage (Supply)",
        ("bus_voltage_supply_value",),
        (PLOT_COLOR_Z,),
        (0.0, 6.0),
    ),
    (
        "power_supply",
        "Power (Supply)",
        ("power_supply_value",),
        (PLOT_COLOR_Z,),
        (0.0, 60.0),
    ),
)


class KabotIoView:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Kabot IO HMI")
        self.root.geometry("1480x820")

        self.left_var = tk.StringVar(value="0.0")
        self.right_var = tk.StringVar(value="0.0")
        self.interval_var = tk.StringVar(value="0.1")
        self.status_var = tk.StringVar(value="Idle")
        self.plots_enabled_var = tk.BooleanVar(value=True)
        self.active_robot_var = tk.StringVar(value="No claimed robot")

        self.state_vars: dict[str, tk.StringVar] = {}
        self.state_hz_vars: dict[str, tk.StringVar] = {}
        self.plot_figures: dict[str, Figure] = {}
        self.plot_axes = {}
        self.plot_widgets: dict[str, FigureCanvasTkAgg] = {}
        self.plot_lines = {}
        self.plot_zero_lines = {}
        self.plot_range_text = {}
        self.plot_series: dict[str, deque[tuple[float, tuple[float, ...]]]] = {
            plot_id: deque() for plot_id, _title, _attrs, _colors, _ylim in PLOT_DEFINITIONS
        }
        self._last_plot_time_sec: float | None = None
        self._latest_plot_time_sec: float | None = None
        self._plot_redraw_after_id = None

        self.on_send_once = None
        self.on_toggle_periodic = None
        self.on_toggle_plots = None
        self.on_scan = None
        self.on_claim_selected = None
        self.on_unclaim = None
        self.on_stop = None
        self.on_arrow_press = None
        self.on_arrow_release = None

        self.robot_tree: ttk.Treeview | None = None

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
        ).grid(row=0, column=0, columnspan=9, sticky="w", pady=(0, 8))

        ttk.Label(control_frame, text="Left effort").grid(row=1, column=0, sticky="w")
        ttk.Entry(control_frame, textvariable=self.left_var, width=12).grid(row=1, column=1, padx=6)

        ttk.Label(control_frame, text="Right effort").grid(row=1, column=2, sticky="w")
        ttk.Entry(control_frame, textvariable=self.right_var, width=12).grid(
            row=1, column=3, padx=6
        )

        ttk.Label(control_frame, text="Interval [s]").grid(row=1, column=4, sticky="w")
        ttk.Entry(control_frame, textvariable=self.interval_var, width=12).grid(
            row=1, column=5, padx=6
        )

        ttk.Button(control_frame, text="Send once", command=self._emit_send_once).grid(
            row=1, column=6, padx=8
        )
        self.periodic_button = ttk.Button(
            control_frame, text="Start periodic", command=self._emit_toggle_periodic
        )
        self.periodic_button.grid(row=1, column=7, padx=8)

        ttk.Button(control_frame, text="Scan", command=self._emit_scan).grid(
            row=1, column=8, padx=8
        )

        ttk.Button(control_frame, text="Claim Selected", command=self._emit_claim_selected).grid(
            row=1, column=9, padx=8
        )

        ttk.Button(control_frame, text="Unclaim", command=self._emit_unclaim).grid(
            row=1, column=10, padx=8
        )

        ttk.Checkbutton(
            control_frame,
            text="Enable plots",
            variable=self.plots_enabled_var,
            command=self._emit_toggle_plots,
        ).grid(row=2, column=6, columnspan=2, sticky="e", padx=8, pady=(8, 0))

        ttk.Label(control_frame, textvariable=self.status_var).grid(
            row=2, column=0, columnspan=6, sticky="w", pady=(8, 0)
        )
        ttk.Label(control_frame, textvariable=self.active_robot_var).grid(
            row=2, column=6, columnspan=4, sticky="e", pady=(8, 0)
        )

        robot_frame = ttk.LabelFrame(outer, text="Discovered Robots", padding=8)
        robot_frame.pack(fill="x", padx=2, pady=(4, 8))

        columns = (
            "ip",
            "control_port",
            "serial",
            "human_name",
            "firmware_version",
            "is_claimed",
            "claimed_by_ip",
        )
        self.robot_tree = ttk.Treeview(robot_frame, columns=columns, show="headings", height=6)
        self.robot_tree.heading("ip", text="IP")
        self.robot_tree.heading("control_port", text="Control Port")
        self.robot_tree.heading("serial", text="Serial")
        self.robot_tree.heading("human_name", text="Human Name")
        self.robot_tree.heading("firmware_version", text="Firmware")
        self.robot_tree.heading("is_claimed", text="Claimed")
        self.robot_tree.heading("claimed_by_ip", text="Claimed By")

        self.robot_tree.column("ip", width=150, anchor="w")
        self.robot_tree.column("control_port", width=110, anchor="center")
        self.robot_tree.column("serial", width=180, anchor="w")
        self.robot_tree.column("human_name", width=180, anchor="w")
        self.robot_tree.column("firmware_version", width=120, anchor="w")
        self.robot_tree.column("is_claimed", width=90, anchor="center")
        self.robot_tree.column("claimed_by_ip", width=140, anchor="w")

        robot_scroll = ttk.Scrollbar(robot_frame, orient="vertical", command=self.robot_tree.yview)
        self.robot_tree.configure(yscrollcommand=robot_scroll.set)

        self.robot_tree.pack(side="left", fill="x", expand=True)
        robot_scroll.pack(side="right", fill="y")

        content = ttk.Frame(outer)
        content.pack(fill="both", expand=True, padx=2, pady=8)

        state_frame = ttk.LabelFrame(content, text="State (read-only)", padding=10)
        state_frame.grid(row=0, column=0, sticky="nsew", padx=(0, 8))

        plots_frame = ttk.LabelFrame(content, text="Rolling Plots", padding=10)
        plots_frame.grid(row=0, column=1, sticky="ns")

        content.grid_columnconfigure(0, weight=1)
        content.grid_columnconfigure(1, weight=0)
        content.grid_rowconfigure(0, weight=1)

        state_canvas = tk.Canvas(state_frame, borderwidth=0, highlightthickness=0)
        scrollbar = ttk.Scrollbar(state_frame, orient="vertical", command=state_canvas.yview)
        fields_host = ttk.Frame(state_canvas)

        fields_host.bind(
            "<Configure>",
            lambda _e: state_canvas.configure(scrollregion=state_canvas.bbox("all")),
        )

        state_canvas.create_window((0, 0), window=fields_host, anchor="nw")
        state_canvas.configure(yscrollcommand=scrollbar.set)

        state_canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        plots_canvas = tk.Canvas(
            plots_frame, borderwidth=0, highlightthickness=0, width=PLOT_WIDTH + 24
        )
        plots_scrollbar = ttk.Scrollbar(plots_frame, orient="vertical", command=plots_canvas.yview)
        plots_host = ttk.Frame(plots_canvas)

        plots_host.bind(
            "<Configure>",
            lambda _e: plots_canvas.configure(scrollregion=plots_canvas.bbox("all")),
        )

        plots_canvas.create_window((0, 0), window=plots_host, anchor="nw")
        plots_canvas.configure(yscrollcommand=plots_scrollbar.set)

        plots_canvas.pack(side="left", fill="y", expand=False)
        plots_scrollbar.pack(side="right", fill="y")

        ttk.Label(fields_host, text="Hz", width=8).grid(row=0, column=2, sticky="w", padx=(8, 0))

        for idx, (field_name, _snapshot_attr, _hz_attr) in enumerate(STATE_FIELDS):
            var = tk.StringVar(value="")
            self.state_vars[field_name] = var
            hz_var = tk.StringVar(value="")
            self.state_hz_vars[field_name] = hz_var

            ttk.Label(fields_host, text=field_name, width=34).grid(
                row=idx + 1, column=0, sticky="w", pady=2
            )
            entry = ttk.Entry(fields_host, textvariable=var, width=48, state="readonly")
            entry.grid(row=idx + 1, column=1, sticky="we", padx=8, pady=2)

            hz_entry = ttk.Entry(fields_host, textvariable=hz_var, width=8, state="readonly")
            hz_entry.grid(row=idx + 1, column=2, sticky="w", padx=(8, 0), pady=2)

        for row, (plot_id, title, _attrs, _colors, y_limits) in enumerate(PLOT_DEFINITIONS):
            ttk.Label(plots_host, text=title).grid(row=row * 2, column=0, sticky="w", pady=(0, 2))

            figure = Figure(
                figsize=(PLOT_WIDTH / PLOT_DPI, PLOT_HEIGHT / PLOT_DPI),
                dpi=PLOT_DPI,
                facecolor=PLOT_BG_COLOR,
            )
            axis = figure.add_subplot(111)
            axis.set_facecolor(PLOT_BG_COLOR)
            for spine in axis.spines.values():
                spine.set_color(PLOT_BORDER_COLOR)
            axis.tick_params(colors=PLOT_LABEL_COLOR, labelsize=7)
            axis.grid(True, color="#e2e8f0", linewidth=0.8, alpha=0.8)
            axis.tick_params(axis="x", labelbottom=False)
            axis.set_xlim(0.0, PLOT_WINDOW_SEC)
            axis.set_ylim(y_limits[0], y_limits[1])

            lines = []
            for color in _colors:
                (line,) = axis.plot([], [], color=color, linewidth=1.8, alpha=PLOT_LINE_ALPHA)
                lines.append(line)

            zero_line = axis.axhline(
                0.0, color=PLOT_ZERO_LINE_COLOR, linewidth=0.9, linestyle=(0, (2, 2))
            )
            zero_line.set_visible(False)

            range_text = axis.text(
                0.99,
                0.96,
                "",
                transform=axis.transAxes,
                ha="right",
                va="top",
                color=PLOT_LABEL_COLOR,
                fontsize=8,
            )

            fig_canvas = FigureCanvasTkAgg(figure, master=plots_host)
            widget = fig_canvas.get_tk_widget()
            widget.configure(highlightthickness=1, highlightbackground=PLOT_BORDER_COLOR)
            widget.grid(row=row * 2 + 1, column=0, sticky="w", pady=(0, 8))

            self.plot_figures[plot_id] = figure
            self.plot_axes[plot_id] = axis
            self.plot_widgets[plot_id] = fig_canvas
            self.plot_lines[plot_id] = lines
            self.plot_zero_lines[plot_id] = zero_line
            self.plot_range_text[plot_id] = range_text

    def _emit_send_once(self) -> None:
        if self.on_send_once is not None:
            self.on_send_once()

    def _emit_toggle_periodic(self) -> None:
        if self.on_toggle_periodic is not None:
            self.on_toggle_periodic()

    def _emit_toggle_plots(self) -> None:
        if self.on_toggle_plots is not None:
            self.on_toggle_plots()

    def _emit_scan(self) -> None:
        if self.on_scan is not None:
            self.on_scan()

    def _emit_claim_selected(self) -> None:
        if self.on_claim_selected is not None:
            self.on_claim_selected()

    def _emit_unclaim(self) -> None:
        if self.on_unclaim is not None:
            self.on_unclaim()

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

    def plots_enabled(self) -> bool:
        return bool(self.plots_enabled_var.get())

    def read_control_inputs(self) -> tuple[float, float, float]:
        return (
            float(self.left_var.get()),
            float(self.right_var.get()),
            float(self.interval_var.get()),
        )

    def read_effort_inputs(self) -> tuple[float, float]:
        return float(self.left_var.get()), float(self.right_var.get())

    def read_interval_input(self) -> float:
        return float(self.interval_var.get())

    def set_control_values(self, left: float, right: float) -> None:
        self.left_var.set(f"{left:.3f}")
        self.right_var.set(f"{right:.3f}")

    def set_state_snapshot(self, snapshot: StateSnapshot) -> None:
        for ui_key, snapshot_attr, hz_attr in STATE_FIELDS:
            self.state_vars[ui_key].set(getattr(snapshot, snapshot_attr))
            self.state_hz_vars[ui_key].set(getattr(snapshot, hz_attr) if hz_attr else "")

    def set_discovered_robots(self, robots: list[DiscoveredRobot]) -> None:
        if self.robot_tree is None:
            return

        for row_id in self.robot_tree.get_children():
            self.robot_tree.delete(row_id)

        for idx, robot in enumerate(robots):
            self.robot_tree.insert(
                "",
                "end",
                iid=str(idx),
                values=(
                    robot.ip,
                    str(robot.control_port),
                    robot.serial or "",
                    robot.human_name or "",
                    robot.firmware_version or "",
                    "yes" if robot.is_claimed else "no",
                    robot.claimed_by_ip or "",
                ),
            )

    def selected_robot_index(self) -> int | None:
        if self.robot_tree is None:
            return None

        selection = self.robot_tree.selection()
        if not selection:
            return None

        try:
            return int(selection[0])
        except ValueError:
            return None

    def set_active_robot(self, text: str) -> None:
        self.active_robot_var.set(text)

    @staticmethod
    def _to_float_or_none(raw: str) -> float | None:
        try:
            return float(raw)
        except (TypeError, ValueError):
            return None

    def add_plot_sample(self, snapshot: StateSnapshot, sample_time_sec: float) -> None:
        if not self.plots_enabled():
            return

        # Reset history when producer time goes backwards (e.g. firmware restart) to
        # avoid drawing long diagonal connections between different time epochs.
        if self._last_plot_time_sec is not None and sample_time_sec <= self._last_plot_time_sec:
            for series in self.plot_series.values():
                series.clear()

        self._last_plot_time_sec = sample_time_sec

        for plot_id, _title, attrs, _colors, _ylim in PLOT_DEFINITIONS:
            values: list[float] = []
            for attr in attrs:
                parsed = self._to_float_or_none(getattr(snapshot, attr, ""))
                if parsed is None:
                    values = []
                    break
                values.append(parsed)

            if not values:
                continue

            series = self.plot_series[plot_id]
            series.append((sample_time_sec, tuple(values)))
            self._trim_plot_series(series, sample_time_sec)

        self._latest_plot_time_sec = sample_time_sec
        self._schedule_plot_redraw()

    def _schedule_plot_redraw(self) -> None:
        if self._plot_redraw_after_id is not None:
            return
        self._plot_redraw_after_id = self.root.after(
            PLOT_REDRAW_INTERVAL_MS, self._flush_plot_redraw
        )

    def _flush_plot_redraw(self) -> None:
        self._plot_redraw_after_id = None
        if self._latest_plot_time_sec is None:
            return
        self._redraw_plots(self._latest_plot_time_sec)

    def _trim_plot_series(
        self, series: deque[tuple[float, tuple[float, ...]]], latest_time_sec: float
    ) -> None:
        min_time = latest_time_sec - PLOT_WINDOW_SEC
        while series and series[0][0] < min_time:
            series.popleft()

    def _redraw_plots(self, latest_time_sec: float) -> None:
        for plot_id, _title, attrs, colors, y_limits in PLOT_DEFINITIONS:
            self._draw_single_plot(plot_id, len(attrs), colors, y_limits, latest_time_sec)

    def _draw_single_plot(
        self,
        plot_id: str,
        component_count: int,
        colors: tuple[str, ...],
        y_limits: tuple[float, float],
        latest_time_sec: float,
    ) -> None:
        axis = self.plot_axes[plot_id]
        figure_canvas = self.plot_widgets[plot_id]
        lines = self.plot_lines[plot_id]
        zero_line = self.plot_zero_lines[plot_id]
        range_text = self.plot_range_text[plot_id]
        series = self.plot_series[plot_id]

        if not series:
            for line in lines:
                line.set_data([], [])
            zero_line.set_visible(False)
            range_text.set_text("")
            figure_canvas.draw_idle()
            return

        min_time = latest_time_sec - PLOT_WINDOW_SEC
        visible = [(ts, vals) for ts, vals in series if ts >= min_time]
        if not visible:
            for line in lines:
                line.set_data([], [])
            zero_line.set_visible(False)
            range_text.set_text("")
            figure_canvas.draw_idle()
            return

        x_vals = [ts for ts, _vals in visible]

        y_min, y_max = y_limits

        has_zero = y_min < 0.0 < y_max
        zero_line.set_visible(has_zero)
        if has_zero:
            zero_line.set_ydata([0.0, 0.0])

        for comp_idx in range(component_count):
            y_vals = [vals[comp_idx] for _ts, vals in visible]
            lines[comp_idx].set_color(colors[comp_idx])
            lines[comp_idx].set_data(x_vals, y_vals)

        axis.set_xlim(min_time, max(min_time + 0.001, latest_time_sec))
        axis.set_ylim(y_min, y_max)
        range_text.set_text(f"{y_min:.2f} .. {y_max:.2f}")

        figure_canvas.draw_idle()

    def clear_plots(self) -> None:
        self._last_plot_time_sec = None
        self._latest_plot_time_sec = None

        if self._plot_redraw_after_id is not None:
            self.root.after_cancel(self._plot_redraw_after_id)
            self._plot_redraw_after_id = None

        for series in self.plot_series.values():
            series.clear()

        for plot_id, _title, attrs, colors, y_limits in PLOT_DEFINITIONS:
            axis = self.plot_axes[plot_id]
            axis.set_xlim(0.0, PLOT_WINDOW_SEC)
            axis.set_ylim(y_limits[0], y_limits[1])
            self._draw_single_plot(plot_id, len(attrs), colors, y_limits, 0.0)

    def set_close_callback(self, callback) -> None:
        self.root.protocol("WM_DELETE_WINDOW", callback)

    def run(self) -> None:
        self.root.mainloop()
