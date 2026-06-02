from collections import deque
import tkinter as tk
from tkinter import ttk

from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

from model import StateSnapshot
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

PLOT_DEFINITIONS: tuple[tuple[str, str, tuple[str, ...], tuple[str, ...]], ...] = (
    ("effort", "Effort (Vector2)", ("effort_x", "effort_y"), ("#3b82f6", "#f59e0b")),
    (
        "linear_accel",
        "Linear Acceleration (Vector3)",
        ("linear_accel_x", "linear_accel_y", "linear_accel_z"),
        ("#ef4444", "#3b82f6", "#10b981"),
    ),
    (
        "angular_vel",
        "Angular Velocity (Vector3)",
        ("angular_vel_x", "angular_vel_y", "angular_vel_z"),
        ("#8b5cf6", "#06b6d4", "#f97316"),
    ),
    (
        "magnetic_field",
        "Magnetic Field (Vector3)",
        ("magnetic_field_x", "magnetic_field_y", "magnetic_field_z"),
        ("#ec4899", "#0ea5e9", "#84cc16"),
    ),
    ("distance", "Distance (Scalar)", ("distance_value",), ("#2563eb",)),
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

        self.state_vars: dict[str, tk.StringVar] = {}
        self.state_hz_vars: dict[str, tk.StringVar] = {}
        self.plot_figures: dict[str, Figure] = {}
        self.plot_axes = {}
        self.plot_widgets: dict[str, FigureCanvasTkAgg] = {}
        self.plot_series: dict[str, deque[tuple[float, tuple[float, ...]]]] = {
            plot_id: deque() for plot_id, _title, _attrs, _colors in PLOT_DEFINITIONS
        }
        self._last_plot_time_sec: float | None = None

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

        content = ttk.Frame(outer)
        content.pack(fill="both", expand=True, padx=2, pady=8)

        state_frame = ttk.LabelFrame(content, text="State (read-only)", padding=10)
        state_frame.grid(row=0, column=0, sticky="nsew", padx=(0, 8))

        plots_frame = ttk.LabelFrame(content, text="Rolling Plots", padding=10)
        plots_frame.grid(row=0, column=1, sticky="ns")

        content.grid_columnconfigure(0, weight=1)
        content.grid_columnconfigure(1, weight=0)
        content.grid_rowconfigure(0, weight=1)

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

        ttk.Label(fields_host, text="Hz", width=8).grid(row=0, column=2, sticky="w", padx=(8, 0))

        for idx, (field_name, _snapshot_attr, _hz_attr) in enumerate(STATE_FIELDS):
            var = tk.StringVar(value="")
            self.state_vars[field_name] = var
            hz_var = tk.StringVar(value="")
            self.state_hz_vars[field_name] = hz_var

            ttk.Label(fields_host, text=field_name, width=34).grid(row=idx + 1, column=0, sticky="w", pady=2)
            entry = ttk.Entry(fields_host, textvariable=var, width=48, state="readonly")
            entry.grid(row=idx + 1, column=1, sticky="we", padx=8, pady=2)

            hz_entry = ttk.Entry(fields_host, textvariable=hz_var, width=8, state="readonly")
            hz_entry.grid(row=idx + 1, column=2, sticky="w", padx=(8, 0), pady=2)

        for row, (plot_id, title, _attrs, _colors) in enumerate(PLOT_DEFINITIONS):
            ttk.Label(plots_frame, text=title).grid(row=row * 2, column=0, sticky="w", pady=(0, 2))

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

            canvas = FigureCanvasTkAgg(figure, master=plots_frame)
            widget = canvas.get_tk_widget()
            widget.configure(highlightthickness=1, highlightbackground=PLOT_BORDER_COLOR)
            widget.grid(row=row * 2 + 1, column=0, sticky="w", pady=(0, 8))

            self.plot_figures[plot_id] = figure
            self.plot_axes[plot_id] = axis
            self.plot_widgets[plot_id] = canvas

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
        for ui_key, snapshot_attr, hz_attr in STATE_FIELDS:
            self.state_vars[ui_key].set(getattr(snapshot, snapshot_attr))
            self.state_hz_vars[ui_key].set(getattr(snapshot, hz_attr) if hz_attr else "")

    @staticmethod
    def _to_float_or_none(raw: str) -> float | None:
        try:
            return float(raw)
        except (TypeError, ValueError):
            return None

    def add_plot_sample(self, snapshot: StateSnapshot, sample_time_sec: float) -> None:
        # Reset history when producer time goes backwards (e.g. firmware restart) to
        # avoid drawing long diagonal connections between different time epochs.
        if self._last_plot_time_sec is not None and sample_time_sec <= self._last_plot_time_sec:
            for series in self.plot_series.values():
                series.clear()

        self._last_plot_time_sec = sample_time_sec

        for plot_id, _title, attrs, _colors in PLOT_DEFINITIONS:
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

        self._redraw_plots(sample_time_sec)

    def _trim_plot_series(self, series: deque[tuple[float, tuple[float, ...]]], latest_time_sec: float) -> None:
        min_time = latest_time_sec - PLOT_WINDOW_SEC
        while series and series[0][0] < min_time:
            series.popleft()

    def _redraw_plots(self, latest_time_sec: float) -> None:
        for plot_id, _title, attrs, colors in PLOT_DEFINITIONS:
            self._draw_single_plot(plot_id, len(attrs), colors, latest_time_sec)

    def _draw_single_plot(
        self,
        plot_id: str,
        component_count: int,
        colors: tuple[str, ...],
        latest_time_sec: float,
    ) -> None:
        axis = self.plot_axes[plot_id]
        figure_canvas = self.plot_widgets[plot_id]
        series = self.plot_series[plot_id]

        axis.cla()
        axis.set_facecolor(PLOT_BG_COLOR)
        for spine in axis.spines.values():
            spine.set_color(PLOT_BORDER_COLOR)
        axis.tick_params(colors=PLOT_LABEL_COLOR, labelsize=7)
        axis.grid(True, color="#e2e8f0", linewidth=0.8, alpha=0.8)

        if not series:
            figure_canvas.draw_idle()
            return

        min_time = latest_time_sec - PLOT_WINDOW_SEC
        visible = [(ts, vals) for ts, vals in series if ts >= min_time]
        if not visible:
            figure_canvas.draw_idle()
            return

        visible.sort(key=lambda item: item[0])

        flat_values = [vals[i] for _ts, vals in visible for i in range(component_count)]
        y_min = min(flat_values)
        y_max = max(flat_values)
        if y_min == y_max:
            y_min -= 1.0
            y_max += 1.0

        if y_min < 0.0 < y_max:
            axis.axhline(0.0, color=PLOT_ZERO_LINE_COLOR, linewidth=0.9, linestyle=(0, (2, 2)))

        for comp_idx in range(component_count):
            x_vals = [ts for ts, _vals in visible]
            y_vals = [vals[comp_idx] for _ts, vals in visible]
            if len(x_vals) >= 2:
                axis.plot(x_vals, y_vals, color=colors[comp_idx], linewidth=1.8, alpha=PLOT_LINE_ALPHA)

        axis.set_xlim(min_time, latest_time_sec)
        axis.set_ylim(y_min, y_max)
        axis.tick_params(axis="x", labelbottom=False)
        axis.text(
            0.99,
            0.96,
            f"{y_min:.2f} .. {y_max:.2f}",
            transform=axis.transAxes,
            ha="right",
            va="top",
            color=PLOT_LABEL_COLOR,
            fontsize=8,
        )

        figure_canvas.draw_idle()

    def set_close_callback(self, callback) -> None:
        self.root.protocol("WM_DELETE_WINDOW", callback)

    def run(self) -> None:
        self.root.mainloop()
