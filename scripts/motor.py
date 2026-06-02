#!/usr/bin/env python3
"""MVC Architecture for Motor Control: Easily swappable UI."""

import socket
import struct
import threading
import time
from typing import Callable, Optional

import typer
import tkinter as tk
import numpy as np

app = typer.Typer(help="Send EffortMsg protobuf datagrams over UDP via MVC architecture.")


# ==========================================
# MODEL (Data, Network, & Hardware Logic)
# ==========================================
class MotorModel:
    """Handles the state of the motors and network communication."""
    
    def __init__(self, host: str, port: int, interval: float, quiet: bool):
        self.target = (host, port)
        self.interval = interval
        self.quiet = quiet
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        
        # Thread-safe state
        self._lock = threading.Lock()
        self.left = 0.0
        self.right = 0.0
        
        self.sent_count = 0
        self.is_running = False

    def set_effort(self, left: float, right: float):
        """Update the target effort safely across threads."""
        with self._lock:
            self.left = left
            self.right = right

    def get_effort(self) -> tuple[float, float]:
        with self._lock:
            return self.left, self.right

    def _encode_effort_msg(self, left: float, right: float) -> bytes:
        """Manually pack the Protobuf wire format."""
        return b"\x0d" + struct.pack("<f", left) + b"\x15" + struct.pack("<f", right)

    def send_current_state(self):
        """Encode and send the current effort to the UDP target."""
        left, right = self.get_effort()
        payload = self._encode_effort_msg(left, right)
        self.sock.sendto(payload, self.target)
        self.sent_count += 1
        
        if not self.quiet:
            print(f"sent #{self.sent_count:04d} | left={left:5.2f} right={right:5.2f}", end="\r")

    def send_stop_sequence(self):
        """Fire 3 guaranteed stop packets."""
        payload = self._encode_effort_msg(0.0, 0.0)
        for _ in range(3):
            self.sock.sendto(payload, self.target)
            time.sleep(0.01)

    def close(self):
        """Safely shut down the socket and stop motors."""
        print("\nShutting down motors...")
        self.send_stop_sequence()
        self.sock.close()


# ==========================================
# VIEW (User Interface)
# ==========================================
class TkinterView:
    """A dumb UI component. Captures input and fires generic callbacks."""
    
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Motor Control")
        self.root.geometry("250x100")
        
        self.label = tk.Label(self.root, text="Window must be focused\nto drive motors. Q to quit.", pady=20)
        self.label.pack()

        # Callbacks to be assigned by the Controller
        self.on_press_cb: Optional[Callable[[str], None]] = None
        self.on_release_cb: Optional[Callable[[str], None]] = None
        self.on_quit_cb: Optional[Callable[[], None]] = None

        # Bind UI events
        self.root.bind('<KeyPress>', self._handle_press)
        self.root.bind('<KeyRelease>', self._handle_release)
        self.root.protocol("WM_DELETE_WINDOW", self._handle_quit)

    def _handle_press(self, event):
        key = event.keysym
        if key.lower() == 'q':
            self._handle_quit()
        elif self.on_press_cb:
            self.on_press_cb(key)

    def _handle_release(self, event):
        if self.on_release_cb:
            self.on_release_cb(event.keysym)

    def _handle_quit(self):
        if self.on_quit_cb:
            self.on_quit_cb()
        self.root.destroy()

    def start(self):
        """Start the blocking UI loop."""
        self.root.mainloop()


# ==========================================
# CONTROLLER (Business Logic & Glue)
# ==========================================
class MotorController:
    """Connects the View's inputs to the Model's logic."""
    
    def __init__(self, model: MotorModel, view: TkinterView):
        self.model = model
        self.view = view
        self.active_keys = set()
        
        # Connect View -> Controller
        self.view.on_press_cb = self.handle_key_press
        self.view.on_release_cb = self.handle_key_release
        self.view.on_quit_cb = self.stop

        # Network thread (allows UI to run on main thread)
        self.network_thread = threading.Thread(target=self._network_loop, daemon=True)

    def handle_key_press(self, key: str):
        if key in ('Up', 'Down', 'Left', 'Right') and key not in self.active_keys:
            self.active_keys.add(key)
            self._update_model_effort()

    def handle_key_release(self, key: str):
        if key in self.active_keys:
            self.active_keys.remove(key)
            self._update_model_effort()

    def _update_model_effort(self):
        """Translate UI state into motor effort and update the Model."""
        
        # 1. MACIERZ KLAWISZY (dtype=object, używamy zbiorów)
        key_matrix = np.array([
            [{"Up", "Left"},   {"Up"},   {"Up", "Right"}],
            [{"Left"},         set(),    {"Right"}      ],  # set() oznacza brak wciśniętych klawiszy (środek)
            [{"Down", "Left"}, {"Down"}, {"Down", "Right"}]
        ], dtype=object)

        # 2. MACIERZ SILNIKÓW (z poprzedniego kroku)
        effort_matrix = np.array([
            [[ 0.0,  1.0], [ 1.0,  1.0], [ 1.0,  0.0]],
            [[-1.0,  1.0], [ 0.0,  0.0], [ 1.0, -1.0]],
            [[ 0.0, -1.0], [-1.0, -1.0], [-1.0,  0.0]]
        ])

        # scale the matrix by left, right multipliers
        effort_matrix[:, :, 0] *= 0.5  # left multiplier (could be dynamic)
        effort_matrix[:, :, 1] *= 0.5  # right multiplier (could be dynamic)

        # 3. FUNKCJA MAPUJĄCA KLAWISZE NA WSPÓŁRZĘDNE
        def get_coordinates_from_keys(pressed_keys_set):
            """
            Szuka w key_matrix zbioru odpowiadającego wciśniętym klawiszom 
            i zwraca jego współrzędne (x, y).
            """
            for (y, x), required_keys in np.ndenumerate(key_matrix):
                if pressed_keys_set == required_keys:
                    return x, y
                    
            # Fallback: Jeśli wciśnięto dziwną kombinację (np. Up i Down naraz),
            # zwracamy bezpieczny środek (1, 1), czyli stop.
            return 1, 1


        currently_pressed = self.active_keys

        # 1. Znajdź pozycję w macierzy na podstawie klawiszy
        x, y = get_coordinates_from_keys(currently_pressed)
        print(f"Rozpoznane współrzędne dla {currently_pressed}: X={x}, Y={y}")

        # 2. Pobierz moc silników dla tych współrzędnych
        left_motor, right_motor = effort_matrix[y, x]
        print(f"Wysyłam do robota: Lewy={left_motor}, Prawy={right_motor}")

        self.model.set_effort(left_motor, right_motor)

    def _network_loop(self):
        """Background loop pushing data to the network at the target interval."""
        while self.model.is_running:
            self.model.send_current_state()
            time.sleep(self.model.interval)

    def start(self):
        if not self.model.quiet:
            print(f"Connecting to {self.model.target[0]}:{self.model.target[1]}...")
            
        self.model.is_running = True
        self.network_thread.start()
        
        # This blocks until the window is closed
        self.view.start() 

    def stop(self):
        """Cleanly shut down the background threads and model."""
        self.model.is_running = False
        self.model.close()


# ==========================================
# CLI ENTRY POINT
# ==========================================
@app.command()
def main(
    host: str = typer.Option(..., "--host", help="Target host/IP (e.g. 192.168.1.50)"),
    port: int = typer.Option(30010, "--port", help="Target UDP port"),
    left: Optional[float] = typer.Option(None, "--left", help="Static left effort in [-1.0, 1.0]"),
    right: Optional[float] = typer.Option(None, "--right", help="Static right effort in [-1.0, 1.0]"),
    arrows: bool = typer.Option(False, "--arrows", help="Launch interactive MVC window"),
    count: int = typer.Option(1, "--count", help="Static mode packets to send (0 means forever)"),
    interval: float = typer.Option(0.1, "--interval", help="Seconds between packets"),
    quiet: bool = typer.Option(False, "--quiet", help="Do not print logs")
):
    model = MotorModel(host, port, interval, quiet)

    if arrows:
        # Assemble and run the MVC components
        view = TkinterView()
        controller = MotorController(model, view)
        controller.start()
        
    else:
        # Static command mode (using just the Model)
        if left is None or right is None:
            print("Error: --left and --right are required unless --arrows is used.")
            raise typer.Exit(code=1)

        model.set_effort(left, right)
        try:
            while count == 0 or model.sent_count < count:
                model.send_current_state()
                time.sleep(interval)
        except KeyboardInterrupt:
            pass
        finally:
            model.close()


if __name__ == "__main__":
    app()