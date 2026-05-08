from __future__ import annotations

import argparse
import math
import queue
import struct
import threading
import time
import tkinter as tk
from dataclasses import dataclass
from tkinter import messagebox, ttk
from zlib import crc32

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None


MAGIC = b"CGTH"
HEADER_LEN = 28
VERSION = 1
FRAME_TYPE_FLOAT32 = 1
WIDTH = 32
HEIGHT = 24
PIXEL_COUNT = WIDTH * HEIGHT
PAYLOAD_LEN = PIXEL_COUNT * 4
DEFAULT_BAUD = 115200
VIEWER_VERSION = "V1"


@dataclass
class ThermalFrame:
    sequence: int
    timestamp_ms: int
    pixels: tuple[float, ...]


class ThermalFrameParser:
    def __init__(self) -> None:
        self._buffer = bytearray()
        self.frames = 0
        self.crc_errors = 0
        self.sync_drops = 0

    @property
    def buffered_bytes(self) -> int:
        return len(self._buffer)

    def feed(self, data: bytes) -> list[ThermalFrame]:
        self._buffer.extend(data)
        frames: list[ThermalFrame] = []

        while True:
            magic_index = self._buffer.find(MAGIC)
            if magic_index < 0:
                if len(self._buffer) > len(MAGIC):
                    self.sync_drops += len(self._buffer) - len(MAGIC) + 1
                    del self._buffer[: len(self._buffer) - len(MAGIC) + 1]
                break

            if magic_index > 0:
                self.sync_drops += magic_index
                del self._buffer[:magic_index]

            if len(self._buffer) < HEADER_LEN:
                break

            version = self._buffer[4]
            frame_type = self._buffer[5]
            header_len = struct.unpack_from("<H", self._buffer, 6)[0]
            payload_len = struct.unpack_from("<I", self._buffer, 20)[0]
            total_len = header_len + payload_len

            if header_len != HEADER_LEN or payload_len != PAYLOAD_LEN:
                self.sync_drops += 1
                del self._buffer[0]
                continue

            if len(self._buffer) < total_len:
                break

            payload = bytes(self._buffer[header_len:total_len])
            expected_crc = struct.unpack_from("<I", self._buffer, 24)[0]
            actual_crc = crc32(payload) & 0xFFFFFFFF
            if expected_crc != actual_crc:
                self.crc_errors += 1
                del self._buffer[0]
                continue

            if version == VERSION and frame_type == FRAME_TYPE_FLOAT32:
                width = struct.unpack_from("<H", self._buffer, 16)[0]
                height = struct.unpack_from("<H", self._buffer, 18)[0]
                if width == WIDTH and height == HEIGHT:
                    sequence = struct.unpack_from("<I", self._buffer, 8)[0]
                    timestamp_ms = struct.unpack_from("<I", self._buffer, 12)[0]
                    pixels = struct.unpack("<" + "f" * PIXEL_COUNT, payload)
                    frames.append(ThermalFrame(sequence, timestamp_ms, pixels))
                    self.frames += 1

            del self._buffer[:total_len]

        return frames


class SerialReader(threading.Thread):
    def __init__(self, port: str, baud: int, output_queue: queue.Queue, stop_event: threading.Event) -> None:
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.output_queue = output_queue
        self.stop_event = stop_event
        self.parser = ThermalFrameParser()
        self.bytes_read = 0

    def run(self) -> None:
        try:
            if serial is None:
                raise RuntimeError("Install dependencies first: pip install -r requirements.txt")
            with serial.Serial(self.port, self.baud, timeout=0.2, exclusive=True) as ser:
                while not self.stop_event.is_set():
                    data = ser.read(4096)
                    if not data:
                        continue
                    self.bytes_read += len(data)
                    for frame in self.parser.feed(data):
                        self.output_queue.put(("frame", frame, self.parser.frames, self.parser.crc_errors, self.parser.sync_drops))
                    self.output_queue.put(("stats", self.bytes_read, self.parser.frames, self.parser.crc_errors, self.parser.sync_drops, self.parser.buffered_bytes))
        except Exception as exc:
            self.output_queue.put(("error", str(exc)))


class ThermalViewer(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title(f"MLX90640 Thermal Viewer {VIEWER_VERSION}")
        self.resizable(False, False)

        self.frame_queue: queue.Queue = queue.Queue()
        self.reader: SerialReader | None = None
        self.stop_event: threading.Event | None = None
        self.last_frame_time = 0.0
        self.fps = 0.0

        self.port_var = tk.StringVar()
        self.baud_var = tk.StringVar(value=str(DEFAULT_BAUD))
        self.status_var = tk.StringVar(value="Disconnected")
        self.stats_var = tk.StringVar(value="Min --  Max --  Avg --  FPS --")
        self.error_var = tk.StringVar(value="Bytes 0  Frames 0  CRC 0  Sync 0")

        self.cell_size = 20
        self.canvas = tk.Canvas(self, width=WIDTH * self.cell_size, height=HEIGHT * self.cell_size, bg="black", highlightthickness=0)
        self.rects: list[int] = []

        self._build_ui()
        self._refresh_ports()
        self.after(50, self._process_queue)

    def _build_ui(self) -> None:
        controls = ttk.Frame(self, padding=8)
        controls.grid(row=0, column=0, sticky="ew")

        ttk.Label(controls, text="Port").grid(row=0, column=0, padx=(0, 6))
        self.port_combo = ttk.Combobox(controls, textvariable=self.port_var, width=18, state="readonly")
        self.port_combo.grid(row=0, column=1, padx=(0, 6))

        ttk.Label(controls, text="Baud").grid(row=0, column=2, padx=(0, 6))
        self.baud_combo = ttk.Combobox(controls, textvariable=self.baud_var, width=10, state="readonly")
        self.baud_combo["values"] = ("115200", "9600", "230400", "460800", "921600")
        self.baud_combo.grid(row=0, column=3, padx=(0, 6))

        ttk.Button(controls, text="Refresh", command=self._refresh_ports).grid(row=0, column=4, padx=(0, 6))
        self.connect_button = ttk.Button(controls, text="Connect", command=self._toggle_connection)
        self.connect_button.grid(row=0, column=5)

        self.canvas.grid(row=1, column=0, padx=8, pady=(0, 8))

        footer = ttk.Frame(self, padding=(8, 0, 8, 8))
        footer.grid(row=2, column=0, sticky="ew")
        ttk.Label(footer, textvariable=self.status_var).grid(row=0, column=0, sticky="w")
        ttk.Label(footer, textvariable=self.stats_var).grid(row=0, column=1, padx=16)
        ttk.Label(footer, textvariable=self.error_var).grid(row=0, column=2, sticky="e")

        for y in range(HEIGHT):
            for x in range(WIDTH):
                x0 = x * self.cell_size
                y0 = y * self.cell_size
                self.rects.append(self.canvas.create_rectangle(x0, y0, x0 + self.cell_size, y0 + self.cell_size, outline="", fill="#000000"))

    def _refresh_ports(self) -> None:
        if list_ports is None:
            self.port_combo["values"] = []
            self.status_var.set("Install dependencies first: pip install -r requirements.txt")
            return

        ports = [port.device for port in list_ports.comports()]
        self.port_combo["values"] = ports
        if ports and self.port_var.get() not in ports:
            self.port_var.set(ports[0])

    def _toggle_connection(self) -> None:
        if self.reader is not None:
            self._disconnect()
            return

        port = self.port_var.get()
        if not port:
            self.status_var.set("No serial port selected")
            return
        try:
            baud = int(self.baud_var.get())
        except ValueError:
            self.status_var.set("Invalid baud rate")
            return

        self.stop_event = threading.Event()
        self.reader = SerialReader(port, baud, self.frame_queue, self.stop_event)
        self.reader.start()
        self.connect_button.configure(text="Disconnect")
        self.status_var.set(f"Connected {port} @ {baud}")

    def _disconnect(self) -> None:
        if self.stop_event is not None:
            self.stop_event.set()
        self.reader = None
        self.stop_event = None
        self.connect_button.configure(text="Connect")
        self.status_var.set("Disconnected")

    def _process_queue(self) -> None:
        try:
            while True:
                item = self.frame_queue.get_nowait()
                if item[0] == "frame":
                    _, frame, frames, crc_errors, sync_drops = item
                    self._draw_frame(frame)
                elif item[0] == "stats":
                    _, bytes_read, frames, crc_errors, sync_drops, buffered = item
                    self.error_var.set(f"Bytes {bytes_read}  Frames {frames}  CRC {crc_errors}  Sync {sync_drops}  Buf {buffered}")
                elif item[0] == "error":
                    self.status_var.set(f"Serial error: {item[1]}")
                    messagebox.showerror("Serial error", str(item[1]))
                    self._disconnect()
        except queue.Empty:
            pass

        self.after(50, self._process_queue)

    def _draw_frame(self, frame: ThermalFrame) -> None:
        now = time.monotonic()
        if self.last_frame_time > 0:
            dt = now - self.last_frame_time
            if dt > 0:
                self.fps = 1.0 / dt
        self.last_frame_time = now

        valid_pixels = [p for p in frame.pixels if math.isfinite(p)]
        if not valid_pixels:
            return

        min_temp = min(valid_pixels)
        max_temp = max(valid_pixels)
        avg_temp = sum(valid_pixels) / len(valid_pixels)
        span = max(max_temp - min_temp, 0.001)

        for index, value in enumerate(frame.pixels):
            t = 0.0 if not math.isfinite(value) else (value - min_temp) / span
            self.canvas.itemconfigure(self.rects[index], fill=self._palette(t))

        self.stats_var.set(f"Min {min_temp:.1f} C  Max {max_temp:.1f} C  Avg {avg_temp:.1f} C  FPS {self.fps:.1f}")
        self.status_var.set(f"Seq {frame.sequence}  Sensor {frame.timestamp_ms} ms")

    @staticmethod
    def _palette(t: float) -> str:
        t = min(1.0, max(0.0, t))
        stops = (
            (0.0, (0, 0, 32)),
            (0.20, (0, 60, 160)),
            (0.42, (0, 180, 180)),
            (0.62, (90, 210, 60)),
            (0.80, (250, 180, 20)),
            (1.0, (255, 255, 245)),
        )
        for i in range(len(stops) - 1):
            left_t, left_rgb = stops[i]
            right_t, right_rgb = stops[i + 1]
            if t <= right_t:
                local = (t - left_t) / (right_t - left_t)
                rgb = tuple(int(left_rgb[c] + (right_rgb[c] - left_rgb[c]) * local) for c in range(3))
                return f"#{rgb[0]:02x}{rgb[1]:02x}{rgb[2]:02x}"
        return "#ffffff"


def run_self_test() -> None:
    parser = ThermalFrameParser()
    pixels = tuple(float(i % WIDTH) for i in range(PIXEL_COUNT))
    payload = struct.pack("<" + "f" * PIXEL_COUNT, *pixels)
    header = bytearray(HEADER_LEN)
    header[0:4] = MAGIC
    header[4] = VERSION
    header[5] = FRAME_TYPE_FLOAT32
    struct.pack_into("<H", header, 6, HEADER_LEN)
    struct.pack_into("<I", header, 8, 123)
    struct.pack_into("<I", header, 12, 456)
    struct.pack_into("<H", header, 16, WIDTH)
    struct.pack_into("<H", header, 18, HEIGHT)
    struct.pack_into("<I", header, 20, PAYLOAD_LEN)
    struct.pack_into("<I", header, 24, crc32(payload) & 0xFFFFFFFF)

    frames = parser.feed(b"noise" + bytes(header) + payload)
    assert len(frames) == 1
    assert frames[0].sequence == 123
    assert frames[0].timestamp_ms == 456
    assert len(frames[0].pixels) == PIXEL_COUNT
    print("self-test ok")


def main() -> None:
    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("--self-test", action="store_true")
    args = arg_parser.parse_args()

    if args.self_test:
        run_self_test()
        return

    if serial is None:
        raise SystemExit("Install dependencies first: pip install -r requirements.txt")

    app = ThermalViewer()
    app.mainloop()


if __name__ == "__main__":
    main()
