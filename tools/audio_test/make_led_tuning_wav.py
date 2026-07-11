import os
import time
import wave
import queue
import threading
from datetime import datetime
from pathlib import Path
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

import serial
from serial.tools import list_ports


SAMPLE_RATE = 48000
CHANNELS = 2
SAMPLE_WIDTH_BYTES = 2
FRAME_BYTES = CHANNELS * SAMPLE_WIDTH_BYTES
BYTES_PER_SECOND = SAMPLE_RATE * FRAME_BYTES

START_MARKER = b"PCM1808_RAW16_STEREO_48K_START\n"
READ_CHUNK = 8192


def default_wav_path() -> str:
    music_dir = Path.home() / "Music"
    if not music_dir.exists():
        music_dir = Path.cwd()

    name = datetime.now().strftime("esp32_pcm1808_%Y%m%d_%H%M%S.wav")
    return str(music_dir / name)


class ESP32AudioRecorderGUI:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("ESP32-S3 PCM1808 Audio Recorder")
        self.root.geometry("760x430")

        self.msg_queue = queue.Queue()
        self.stop_event = None
        self.worker_thread = None
        self.port_lookup = {}

        self.port_var = tk.StringVar()
        self.baud_var = tk.StringVar(value="2000000")
        self.duration_var = tk.StringVar(value="10")
        self.file_var = tk.StringVar(value=default_wav_path())
        self.status_var = tk.StringVar(value="Ready")

        self._build_ui()
        self.refresh_ports()

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.after(100, self.poll_messages)

    def _build_ui(self):
        main = ttk.Frame(self.root, padding=12)
        main.pack(fill="both", expand=True)

        main.columnconfigure(1, weight=1)

        ttk.Label(main, text="COM port:").grid(row=0, column=0, sticky="w", pady=4)

        self.port_combo = ttk.Combobox(
            main,
            textvariable=self.port_var,
            state="readonly",
            width=45
        )
        self.port_combo.grid(row=0, column=1, sticky="ew", pady=4)

        self.refresh_button = ttk.Button(
            main,
            text="Refresh",
            command=self.refresh_ports
        )
        self.refresh_button.grid(row=0, column=2, padx=(8, 0), pady=4)

        ttk.Label(main, text="Baud:").grid(row=1, column=0, sticky="w", pady=4)

        self.baud_entry = ttk.Entry(main, textvariable=self.baud_var, width=16)
        self.baud_entry.grid(row=1, column=1, sticky="w", pady=4)

        ttk.Label(main, text="Duration seconds:").grid(row=2, column=0, sticky="w", pady=4)

        self.duration_entry = ttk.Entry(main, textvariable=self.duration_var, width=16)
        self.duration_entry.grid(row=2, column=1, sticky="w", pady=4)

        ttk.Label(
            main,
            text="Use 0 for manual stop."
        ).grid(row=2, column=1, sticky="w", padx=(130, 0), pady=4)

        ttk.Label(main, text="Save WAV as:").grid(row=3, column=0, sticky="w", pady=4)

        self.file_entry = ttk.Entry(main, textvariable=self.file_var)
        self.file_entry.grid(row=3, column=1, sticky="ew", pady=4)

        self.browse_button = ttk.Button(
            main,
            text="Browse",
            command=self.browse_file
        )
        self.browse_button.grid(row=3, column=2, padx=(8, 0), pady=4)

        button_row = ttk.Frame(main)
        button_row.grid(row=4, column=0, columnspan=3, sticky="w", pady=(12, 6))

        self.record_button = ttk.Button(
            button_row,
            text="Record",
            command=self.start_recording
        )
        self.record_button.pack(side="left")

        self.stop_button = ttk.Button(
            button_row,
            text="Stop",
            command=self.stop_recording,
            state="disabled"
        )
        self.stop_button.pack(side="left", padx=(8, 0))

        self.progress = ttk.Progressbar(main, mode="determinate")
        self.progress.grid(row=5, column=0, columnspan=3, sticky="ew", pady=6)

        ttk.Label(main, textvariable=self.status_var).grid(
            row=6,
            column=0,
            columnspan=3,
            sticky="w",
            pady=(2, 8)
        )

        self.log_box = tk.Text(main, height=12, wrap="word")
        self.log_box.grid(row=7, column=0, columnspan=3, sticky="nsew")

        scroll = ttk.Scrollbar(main, orient="vertical", command=self.log_box.yview)
        scroll.grid(row=7, column=3, sticky="ns")
        self.log_box.configure(yscrollcommand=scroll.set)

        main.rowconfigure(7, weight=1)

    def log(self, text: str):
        stamp = datetime.now().strftime("%H:%M:%S")
        self.log_box.insert("end", f"[{stamp}] {text}\n")
        self.log_box.see("end")

    def refresh_ports(self):
        current = self.port_var.get()
        self.port_lookup.clear()

        ports = list(list_ports.comports())
        values = []

        for port in ports:
            label = f"{port.device} - {port.description}"
            values.append(label)
            self.port_lookup[label] = port.device

        self.port_combo["values"] = values

        if current in values:
            self.port_var.set(current)
        elif values:
            self.port_var.set(values[0])
        else:
            self.port_var.set("")

        self.log(f"Found {len(values)} serial port(s).")

    def browse_file(self):
        current = self.file_var.get().strip()
        initial_dir = os.path.dirname(current) if current else str(Path.cwd())
        initial_file = os.path.basename(current) if current else "recording.wav"

        path = filedialog.asksaveasfilename(
            title="Save WAV file",
            initialdir=initial_dir,
            initialfile=initial_file,
            defaultextension=".wav",
            filetypes=[
                ("WAV files", "*.wav"),
                ("All files", "*.*")
            ]
        )

        if path:
            self.file_var.set(path)

    def get_selected_port(self) -> str:
        selected = self.port_var.get().strip()

        if selected in self.port_lookup:
            return self.port_lookup[selected]

        # Allows manually typed values if combobox state is changed later.
        if " - " in selected:
            return selected.split(" - ", 1)[0].strip()

        return selected

    def start_recording(self):
        if self.worker_thread and self.worker_thread.is_alive():
            messagebox.showwarning("Already recording", "A recording is already running.")
            return

        port = self.get_selected_port()
        if not port:
            messagebox.showerror("No COM port", "Select an ESP32 COM port first.")
            return

        try:
            baud = int(self.baud_var.get().strip())
        except ValueError:
            messagebox.showerror("Bad baud", "Baud must be a number, for example 2000000.")
            return

        try:
            duration = float(self.duration_var.get().strip())
        except ValueError:
            messagebox.showerror("Bad duration", "Duration must be a number. Use 0 for manual stop.")
            return

        if duration < 0:
            messagebox.showerror("Bad duration", "Duration cannot be negative.")
            return

        out_path = self.file_var.get().strip()
        if not out_path:
            messagebox.showerror("No file", "Choose a WAV file location first.")
            return

        out_dir = os.path.dirname(os.path.abspath(out_path))
        if not os.path.isdir(out_dir):
            messagebox.showerror("Bad file path", f"This folder does not exist:\n{out_dir}")
            return

        self.stop_event = threading.Event()

        self.record_button.configure(state="disabled")
        self.stop_button.configure(state="normal")
        self.refresh_button.configure(state="disabled")
        self.browse_button.configure(state="disabled")

        if duration > 0:
            self.progress.stop()
            self.progress.configure(mode="determinate", maximum=duration, value=0)
        else:
            self.progress.configure(mode="indeterminate")
            self.progress.start(10)

        self.status_var.set("Opening serial port...")
        self.log(f"Starting recording on {port} -> {out_path}")

        self.worker_thread = threading.Thread(
            target=self.record_worker,
            args=(port, baud, duration, out_path, self.stop_event),
            daemon=True
        )
        self.worker_thread.start()

    def stop_recording(self):
        if self.stop_event:
            self.status_var.set("Stopping...")
            self.log("Stop requested.")
            self.stop_event.set()
            self.stop_button.configure(state="disabled")

    def wait_for_marker(self, ser: serial.Serial, marker: bytes, timeout_s: float, stop_event: threading.Event) -> bytes:
        deadline = time.monotonic() + timeout_s
        buf = bytearray()

        while time.monotonic() < deadline:
            if stop_event.is_set():
                raise RuntimeError("Stopped before recording started.")

            chunk = ser.read(256)

            if chunk:
                buf.extend(chunk)
                idx = bytes(buf).find(marker)

                if idx >= 0:
                    after_marker = bytes(buf[idx + len(marker):])
                    return after_marker

                if len(buf) > 8192:
                    del buf[:-8192]
            else:
                time.sleep(0.01)

        preview = bytes(buf[-500:]).decode("utf-8", errors="replace")
        raise TimeoutError(
            "Did not receive the ESP32 start marker.\n\n"
            "Make sure the ESP32 is running the binary recording sketch, not the tuning sketch.\n\n"
            f"Last received text:\n{preview}"
        )

    def record_worker(self, port: str, baud: int, duration: float, out_path: str, stop_event: threading.Event):
        ser = None
        bytes_done = 0

        target_bytes = None
        if duration > 0:
            target_bytes = int(duration * BYTES_PER_SECOND)
            target_bytes -= target_bytes % FRAME_BYTES

        try:
            self.msg_queue.put(("status", f"Opening {port} at {baud} baud..."))

            ser = serial.Serial(
                port=port,
                baudrate=baud,
                timeout=0.25,
                write_timeout=1
            )

            self.msg_queue.put(("status", "Serial opened. Waiting for ESP32 reset..."))
            time.sleep(2.0)

            ser.reset_input_buffer()
            ser.reset_output_buffer()

            self.msg_queue.put(("status", "Sending start command..."))
            ser.write(b"r")
            ser.flush()

            self.msg_queue.put(("status", "Waiting for ESP32 audio stream..."))
            initial_audio = self.wait_for_marker(
                ser,
                START_MARKER,
                timeout_s=10.0,
                stop_event=stop_event
            )

            self.msg_queue.put(("status", "Recording..."))

            carry = bytearray(initial_audio)
            last_ui_update = time.monotonic()
            last_data_time = time.monotonic()

            with wave.open(out_path, "wb") as wav:
                wav.setnchannels(CHANNELS)
                wav.setsampwidth(SAMPLE_WIDTH_BYTES)
                wav.setframerate(SAMPLE_RATE)

                while not stop_event.is_set():
                    if carry:
                        usable_len = len(carry) - (len(carry) % FRAME_BYTES)

                        if usable_len > 0:
                            data = bytes(carry[:usable_len])
                            del carry[:usable_len]

                            if target_bytes is not None:
                                remaining = target_bytes - bytes_done

                                if remaining <= 0:
                                    break

                                if len(data) > remaining:
                                    data = data[:remaining]
                                    data = data[:len(data) - (len(data) % FRAME_BYTES)]

                            if data:
                                wav.writeframesraw(data)
                                bytes_done += len(data)
                                last_data_time = time.monotonic()

                            if target_bytes is not None and bytes_done >= target_bytes:
                                break

                    if target_bytes is not None:
                        remaining = target_bytes - bytes_done
                        if remaining <= 0:
                            break

                        want = min(READ_CHUNK, remaining + FRAME_BYTES)
                    else:
                        want = READ_CHUNK

                    chunk = ser.read(want)

                    if chunk:
                        carry.extend(chunk)
                    else:
                        if time.monotonic() - last_data_time > 5.0:
                            raise TimeoutError(
                                "No audio bytes received for 5 seconds. "
                                "Check that the ESP32 is still running and the correct sketch is uploaded."
                            )

                    now = time.monotonic()
                    if now - last_ui_update >= 0.2:
                        self.msg_queue.put(("progress", bytes_done, duration))
                        last_ui_update = now

            try:
                if ser and ser.is_open:
                    ser.write(b"q")
                    ser.flush()
            except Exception:
                pass

            self.msg_queue.put(("progress", bytes_done, duration))
            self.msg_queue.put(("done", out_path, bytes_done, stop_event.is_set()))

        except serial.SerialException as e:
            msg = str(e)

            if "Access is denied" in msg or "PermissionError" in msg:
                msg = (
                    f"Could not open {port}: access denied.\n\n"
                    "Close Arduino Serial Monitor, Serial Plotter, PlatformIO monitor, "
                    "PuTTY, other Python scripts, or anything else using this COM port. "
                    "Then unplug/replug the ESP32-S3 and click Refresh."
                )
            else:
                msg = f"Serial error:\n{e}"

            self.msg_queue.put(("error", msg))

        except Exception as e:
            self.msg_queue.put(("error", str(e)))

        finally:
            try:
                if ser and ser.is_open:
                    ser.close()
            except Exception:
                pass

    def poll_messages(self):
        try:
            while True:
                msg = self.msg_queue.get_nowait()
                kind = msg[0]

                if kind == "status":
                    self.status_var.set(msg[1])
                    self.log(msg[1])

                elif kind == "progress":
                    bytes_done = msg[1]
                    duration = msg[2]
                    audio_seconds = bytes_done / BYTES_PER_SECOND

                    if duration > 0:
                        self.progress.configure(mode="determinate", maximum=duration)
                        self.progress["value"] = min(audio_seconds, duration)
                        self.status_var.set(f"Recording: {audio_seconds:.1f} / {duration:.1f} sec")
                    else:
                        self.status_var.set(f"Recording: {audio_seconds:.1f} sec")

                elif kind == "done":
                    out_path = msg[1]
                    bytes_done = msg[2]
                    stopped_by_user = msg[3]
                    audio_seconds = bytes_done / BYTES_PER_SECOND

                    self.progress.stop()

                    if stopped_by_user:
                        self.status_var.set(f"Stopped. Saved {audio_seconds:.1f} sec.")
                        self.log(f"Stopped by user. Saved: {out_path}")
                    else:
                        self.status_var.set(f"Finished. Saved {audio_seconds:.1f} sec.")
                        self.log(f"Finished recording. Saved: {out_path}")

                    self.log(f"Bytes recorded: {bytes_done}")

                    self.record_button.configure(state="normal")
                    self.stop_button.configure(state="disabled")
                    self.refresh_button.configure(state="normal")
                    self.browse_button.configure(state="normal")

                    self.file_var.set(default_wav_path())

                elif kind == "error":
                    self.progress.stop()
                    self.status_var.set("Error")
                    self.log("ERROR: " + msg[1])

                    self.record_button.configure(state="normal")
                    self.stop_button.configure(state="disabled")
                    self.refresh_button.configure(state="normal")
                    self.browse_button.configure(state="normal")

                    messagebox.showerror("Recording error", msg[1])

        except queue.Empty:
            pass

        self.root.after(100, self.poll_messages)

    def on_close(self):
        if self.worker_thread and self.worker_thread.is_alive():
            should_close = messagebox.askyesno(
                "Recording active",
                "A recording is active. Stop recording and close?"
            )

            if not should_close:
                return

            if self.stop_event:
                self.stop_event.set()

            self.root.after(500, self.root.destroy)
        else:
            self.root.destroy()


def main():
    root = tk.Tk()
    app = ESP32AudioRecorderGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
