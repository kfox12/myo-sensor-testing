"""Recording the telemetry stream to labeled CSV files.

Each run is saved as two files in <data dir>/<location>/:
  <location>_<label>_t<trial>_<timestamp>.csv   one row per 10 ms group
  <same name>.json                              run metadata
"""

import csv
import json
import time
from datetime import datetime
from pathlib import Path

from .serial_link import GROUP_SECONDS, RowBuilder, parse_telemetry

GROUPS_PER_SECOND = round(1 / GROUP_SECONDS)

# Developer data collection (roadmap step 1): each trial runs these tasks
# in order. (label, prompt, default seconds)
PROTOCOL = [
    ("rest", "Relax completely", 10),
    ("movement", "Repeatedly contract and relax, about once per second", 10),
    ("max", "Squeeze as hard as comfortable and hold", 5),
]
PREP_SECONDS = 3


def slug(text):
    return "".join(c if c.isalnum() else "_" for c in text.strip().lower()).strip("_")


def drain(link, seconds, echo=print):
    """Keep reading (and discarding telemetry) for `seconds`, echoing text lines."""
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        line = link.read_line()
        if line and not parse_telemetry(line):
            echo(line)


def countdown(link, prompt, seconds, echo=print):
    """Show a get-ready countdown while keeping the serial buffer drained."""
    echo(f"Next: {prompt}")
    for remaining in range(seconds, 0, -1):
        echo(f"  starting in {remaining}")
        drain(link, 1, echo)


def record_rows(link, seconds, echo=print):
    """Collect `seconds` worth of telemetry groups, printing a countdown."""
    target = round(seconds * GROUPS_PER_SECOND)
    builder = RowBuilder()
    rows = []
    start = time.monotonic()
    # the first row may be partial (we joined mid-group), so drop it
    skip_first = True
    while len(rows) < target:
        line = link.read_line()
        if line is None:
            continue
        parsed = parse_telemetry(line)
        if not parsed:
            if line:
                echo(line)
            continue
        finished = builder.add(*parsed)
        if finished is None:
            continue
        if skip_first:
            skip_first = False
            continue
        finished["host_time_s"] = round(time.monotonic() - start, 3)
        rows.append(finished)
        remaining_groups = target - len(rows)
        if remaining_groups and remaining_groups % GROUPS_PER_SECOND == 0:
            echo(f"  {remaining_groups // GROUPS_PER_SECOND}")
    return rows


def save_run(rows, data_dir, location, label, trial, port):
    """Write rows + metadata; return the CSV path."""
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    folder = Path(data_dir) / slug(location)
    folder.mkdir(parents=True, exist_ok=True)
    base = f"{slug(location)}_{slug(label)}_t{trial}_{stamp}"
    csv_path = folder / f"{base}.csv"

    fields = sorted({name for row in rows for name in row} - {"host_time_s"})
    with open(csv_path, "w", newline="") as f:
        writer = csv.writer(f)
        # t_s comes from the firmware's exact 10 ms group clock; host_time_s
        # is when the PC received it (jittery, for spotting dropouts)
        writer.writerow(["group", "t_s", "host_time_s"] + fields)
        for i, row in enumerate(rows):
            writer.writerow(
                [i, round(i * GROUP_SECONDS, 2), row["host_time_s"]]
                + [_format(row.get(name)) for name in fields]
            )

    meta = {
        "location": location,
        "label": label,
        "trial": trial,
        "groups": len(rows),
        "seconds": round(len(rows) * GROUP_SECONDS, 2),
        "recorded_at": datetime.now().isoformat(timespec="seconds"),
        "port": port,
    }
    csv_path.with_suffix(".json").write_text(json.dumps(meta, indent=2) + "\n")
    return csv_path


def _format(value):
    if value is None:
        return ""
    return int(value) if float(value).is_integer() else value


def record(link, data_dir, location, label, trial, seconds, echo=print):
    countdown(link, f"record '{label}' for {seconds} s", PREP_SECONDS, echo)
    echo("Go!")
    rows = record_rows(link, seconds, echo)
    path = save_run(rows, data_dir, location, label, trial, link.port)
    echo(f"Saved {len(rows)} groups to {path}")
    return path


def collect(link, data_dir, location, trials, durations, echo=print):
    """Run the full developer protocol: every task, `trials` times."""
    for trial in range(1, trials + 1):
        echo(f"=== {location}: trial {trial} of {trials}")
        for label, prompt, _ in PROTOCOL:
            seconds = durations[label]
            countdown(link, f"{prompt} ({seconds} s)", PREP_SECONDS, echo)
            echo(f"{prompt}!")
            rows = record_rows(link, seconds, echo)
            path = save_run(rows, data_dir, location, label, trial, link.port)
            echo(f"Saved {path.name}")
    echo("Done. Run 'profile' to compute this location's factors.")
