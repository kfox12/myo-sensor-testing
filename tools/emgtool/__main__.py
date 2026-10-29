"""Command-line entry point: python -m emgtool <command> ..."""

import argparse
import json
import sys
from pathlib import Path

DEFAULT_DATA_DIR = Path(__file__).resolve().parents[2] / "data"


def _open_link(args):
    from .serial_link import Link

    link = Link(args.port)
    print(f"Connected to {link.port}, waiting for the board to reset...")
    link.wait_for_boot()
    return link


def cmd_ports(args):
    from serial.tools import list_ports

    for p in list_ports.comports():
        print(f"{p.device}  {p.description}")


def cmd_record(args):
    from .recording import record

    link = _open_link(args)
    try:
        record(link, args.data_dir, args.location, args.label, args.trial, args.seconds)
    finally:
        link.close()


def cmd_collect(args):
    from .recording import PROTOCOL, collect

    durations = {label: seconds for label, _, seconds in PROTOCOL}
    overrides = {"rest": args.rest, "movement": args.movement, "max": args.max}
    # only the flags that were given replace the defaults
    durations.update({label: s for label, s in overrides.items() if s is not None})
    link = _open_link(args)
    try:
        collect(link, args.data_dir, args.location, args.trials, durations)
    finally:
        link.close()


def cmd_calibrate(args):
    from .calibration import calibrate

    link = _open_link(args)
    try:
        result = calibrate(link, args.data_dir, args.location_id)
    finally:
        link.close()
    sys.exit(0 if result["saved"] else 1)


def cmd_info(args):
    from .calibration import info

    link = _open_link(args)
    try:
        result = info(link, echo=lambda line: None)
    finally:
        link.close()
    print(json.dumps(result, indent=2))


def cmd_plot(args):
    from .plotting import plot_runs

    plot_runs(args.files, args.save)


def cmd_profile(args):
    from .reports import report_profile

    report_profile(args.data_dir, args.location, args.name)


def main(argv=None):
    parser = argparse.ArgumentParser(prog="emgtool", description=__doc__)
    parser.add_argument("--port", help="serial port (default: auto-detect)")
    parser.add_argument("--data-dir", type=Path, default=DEFAULT_DATA_DIR,
                        help=f"where runs are saved (default: {DEFAULT_DATA_DIR})")
    # no subcommand opens the interactive menu
    sub = parser.add_subparsers(dest="command")

    sub.add_parser("ports", help="list serial ports").set_defaults(func=cmd_ports)

    p = sub.add_parser("record", help="record one labeled run to CSV")
    p.add_argument("location", help="electrode location name, e.g. forearm_flexors")
    p.add_argument("label", help="what the subject is doing, e.g. rest")
    p.add_argument("--trial", type=int, default=1)
    p.add_argument("--seconds", type=float, default=10)
    p.set_defaults(func=cmd_record)

    p = sub.add_parser("collect",
                       help="guided developer protocol: rest / movement / max x trials")
    p.add_argument("location", help="electrode location name, e.g. forearm_flexors")
    p.add_argument("--trials", type=int, default=3)
    p.add_argument("--rest", type=float, help="rest seconds (default 10)")
    p.add_argument("--movement", type=float, help="movement seconds (default 10)")
    p.add_argument("--max", type=float, help="max-hold seconds (default 5)")
    p.set_defaults(func=cmd_collect)

    p = sub.add_parser("calibrate", help="run the board's calibration and save the result")
    p.add_argument("--location-id", type=int, choices=range(10),
                   help="firmware location profile to calibrate (digit)")
    p.set_defaults(func=cmd_calibrate)

    sub.add_parser("info", help="print the board's active calibration as JSON"
                   ).set_defaults(func=cmd_info)

    p = sub.add_parser("plot", help="plot recorded runs")
    p.add_argument("files", nargs="+", type=Path, help="CSV files from record/collect")
    p.add_argument("--save", type=Path, help="write a PNG instead of opening a window")
    p.set_defaults(func=cmd_plot)

    p = sub.add_parser("profile", help="compute a location profile from collected trials")
    p.add_argument("location", help="folder name under the data dir, e.g. forearm_flexors")
    p.add_argument("--name", help="display name for the firmware table row")
    p.set_defaults(func=cmd_profile)

    args = parser.parse_args(argv)
    if args.command is None:
        from .menu import run

        run(args.data_dir, args.port)
    else:
        args.func(args)


if __name__ == "__main__":
    main()
