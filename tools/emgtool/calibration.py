"""Driving the firmware's calibration ('c') and info ('i') commands."""

import json
import re
import time
from datetime import datetime
from pathlib import Path

from .serial_link import parse_telemetry

CALIBRATION_TIMEOUT_SECONDS = 40  # the run itself takes about 19 s
BLOCK_START = "----- Calibration -----"
BLOCK_END = "-----------------------"


def parse_block(lines):
    """Parse the firmware's calibration block ("key: value" lines) into a dict."""
    result = {}
    for line in lines:
        key, sep, value = line.partition(":")
        if not sep:
            continue
        key = key.strip().replace(" ", "_").replace("'", "")
        value = value.strip()
        result[key] = int(value) if re.fullmatch(r"-?\d+", value) else value
    return result


def parse_measurement(line):
    """Parse "rest p99: 447, max contraction: 8003, SNR: 17.9"."""
    match = re.match(r"rest p99: (\d+), max contraction: (\d+), SNR: ([\d.]+)", line)
    if not match:
        return None
    return {
        "rest_p99": int(match.group(1)),
        "max_contraction": int(match.group(2)),
        "snr": float(match.group(3)),
    }


def _read_text_until(link, done, timeout, echo):
    """Echo text lines until done(line) is true; return the lines read."""
    lines = []
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = link.read_line()
        if not line or parse_telemetry(line):
            continue
        echo(line)
        lines.append(line)
        if done(line):
            return lines
    raise SystemExit("Timed out waiting for the board.")


def _read_block(link, echo, timeout=5):
    lines = _read_text_until(link, lambda l: l == BLOCK_START, timeout, echo)
    lines = _read_text_until(link, lambda l: l == BLOCK_END, timeout, echo)
    return parse_block(lines)


def info(link, echo=print):
    link.send("i")
    return _read_block(link, echo)


def calibrate(link, data_dir, location_id=None, echo=print):
    """Run a calibration on the board and save its result as JSON."""
    if location_id is not None:
        link.send(str(location_id))
        _read_text_until(
            link,
            lambda l: l.startswith("Selected location") or l == "Unknown location.",
            5,
            echo,
        )
    link.send("c")
    lines = _read_text_until(
        link,
        lambda l: l == "Calibration saved." or l == "Keeping previous calibration.",
        CALIBRATION_TIMEOUT_SECONDS,
        echo,
    )
    measurement = next(filter(None, map(parse_measurement, lines)), None)
    saved = lines[-1] == "Calibration saved."
    failure = next((l for l in lines if l.startswith("Calibration failed")), None)

    result = {
        "recorded_at": datetime.now().isoformat(timespec="seconds"),
        "port": link.port,
        "saved": saved,
        "failure": failure,
        "measurement": measurement,
        "calibration": _read_block(link, echo) if saved else None,
    }
    folder = Path(data_dir) / "calibrations"
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / f"calibration_{datetime.now():%Y%m%d-%H%M%S}.json"
    path.write_text(json.dumps(result, indent=2) + "\n")
    echo(f"Saved result to {path}")
    return result
