"""Serial connection to the EMG firmware and parsing of what it prints.

The firmware prints two kinds of lines at 115200 baud:
  * telemetry, ">name:value", once per 10 ms group (e.g. >power:, >activation:)
  * plain text: help, prompts, calibration results, stats summaries
"""

import re
import time

import serial
from serial.tools import list_ports

BAUD = 115200
GROUP_SECONDS = 0.01  # firmware prints one telemetry group every 10 ms
BOOT_TIMEOUT_SECONDS = 5.0


def detect_ports():
    """Return the connected Arduino-like serial ports (possibly none)."""
    return [
        p.device
        for p in list_ports.comports()
        if any(tag in p.device for tag in ("usbmodem", "usbserial", "ttyACM", "ttyUSB"))
        or (p.description and "Arduino" in p.description)
    ]


def find_port():
    """Return the only connected Arduino-like serial port, or exit with a hint."""
    candidates = detect_ports()
    if not candidates:
        raise SystemExit("No Arduino found. Plug it in or pass --port.")
    if len(candidates) > 1:
        raise SystemExit("Several ports found, pass --port: " + ", ".join(candidates))
    return candidates[0]


def parse_telemetry(line):
    """Return (name, value) for a ">name:value" line, else None."""
    if not line.startswith(">") or ":" not in line:
        return None
    name, _, value = line[1:].partition(":")
    try:
        return name, float(value)
    except ValueError:
        return None


def parse_location_list(lines):
    """Read the firmware's location list from its boot help text.

    The board prints lines like "  1 Forearm flexors" (stripped to
    "1 Forearm flexors"). Returns [(id, name), ...].
    """
    locations = []
    for line in lines:
        match = re.fullmatch(r"(\d) ([A-Za-z].*)", line)
        if match:
            locations.append((int(match.group(1)), match.group(2)))
    return locations


class RowBuilder:
    """Groups telemetry fields into one row per firmware group.

    The firmware prints each group's fields in a fixed order, so a field
    name repeating means the previous group is complete.
    """

    def __init__(self):
        self.fields = {}

    def add(self, name, value):
        """Add one field; return the finished previous row, if any."""
        finished = None
        if name in self.fields:
            finished = self.fields
            self.fields = {}
        self.fields[name] = value
        return finished


class Link:
    """An open connection to the board."""

    def __init__(self, port=None):
        self.port = port or find_port()
        # opening the port resets the Uno
        self.ser = serial.Serial(self.port, BAUD, timeout=0.1)

    def close(self):
        self.ser.close()

    def read_line(self):
        """Return the next line (stripped), or None if nothing arrived in 100 ms."""
        raw = self.ser.readline()
        if not raw:
            return None
        return raw.decode("ascii", errors="replace").strip()

    def reset_input(self):
        """Discard everything the board sent while we weren't reading."""
        self.ser.reset_input_buffer()

    def send(self, command):
        self.ser.write(command.encode("ascii"))

    def wait_for_boot(self, echo=print):
        """Wait until the board has reset and started streaming.

        Boot text (help, saved calibration) is passed to `echo` and returned
        as a list of lines.
        """
        boot_lines = []
        deadline = time.monotonic() + BOOT_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            line = self.read_line()
            if line is None:
                continue
            if parse_telemetry(line):
                return boot_lines
            if line:
                echo(line)
                boot_lines.append(line)
        raise SystemExit(f"No data from {self.port}. Is the EMG firmware uploaded?")
