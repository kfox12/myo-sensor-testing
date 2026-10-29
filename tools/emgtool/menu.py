"""Interactive menu: run emgtool with no subcommand (or ./emg from the repo root).

Lists every action with a one-line description, marks the ones that can't
run right now (no board, no recordings), and asks for whatever an action
needs instead of taking command-line arguments.
"""

from collections import namedtuple
from pathlib import Path

import serial

from . import analysis, calibration, plotting, recording
from .reports import report_profile
from .serial_link import Link, detect_ports, parse_location_list

TASKS = [label for label, _, _ in recording.PROTOCOL]
DEFAULT_SECONDS = {label: seconds for label, _, seconds in recording.PROTOCOL}

# requirement -> reason shown when it isn't met
REQUIREMENTS = {
    "board": "needs board",
    "data": "no recordings yet",
    "trials": "needs a complete collect",
}

Action = namedtuple("Action", "section name description requirement handler")


# --- input helpers ---

def ask(prompt, default=None, convert=str):
    """Ask for a value; Enter accepts `default`. Re-asks on bad input."""
    suffix = f" [{default}]" if default is not None else ""
    while True:
        text = input(f"{prompt}{suffix}: ").strip()
        if not text:
            if default is not None:
                return default
            continue
        try:
            return convert(text)
        except ValueError:
            print("  Not valid, try again.")


def choose(title, options):
    """Pick from a numbered list. `options` are labels or (label, value) pairs."""
    options = [o if isinstance(o, tuple) else (o, o) for o in options]
    print(title)
    for number, (label, _) in enumerate(options, 1):
        print(f"  {number}  {label}")
    while True:
        text = input("Choose: ").strip()
        if text.isdigit() and 1 <= int(text) <= len(options):
            return options[int(text) - 1][1]
        print(f"  Enter a number from 1 to {len(options)}.")


# --- session state ---

class Session:
    """What the menu knows about: the data folder and the (lazily opened) board."""

    def __init__(self, data_dir, port=None):
        self.data_dir = Path(data_dir)
        self.port = port
        self.link = None
        self.board_locations = []  # [(firmware id, name)] from the boot text

    def ports(self):
        return [self.port] if self.port else detect_ports()

    def get_link(self):
        """Open the board on first use and keep it open (opening resets it)."""
        if self.link is None:
            ports = self.ports()
            if not ports:
                raise SystemExit("No board found. Plug it in and try again.")
            port = ports[0] if len(ports) == 1 else choose("Which port?", ports)
            print(f"Connecting to {port}, waiting for the board to reset...")
            self.link = Link(port)
            boot_lines = self.link.wait_for_boot(echo=lambda line: None)
            self.board_locations = parse_location_list(boot_lines)
        return self.link

    def close(self):
        if self.link is not None:
            try:
                self.link.close()
            except serial.SerialException:
                pass
            self.link = None

    def data_locations(self):
        """Location folders that hold at least one recording."""
        if not self.data_dir.is_dir():
            return []
        return sorted(
            d.name for d in self.data_dir.iterdir()
            if d.is_dir() and d.name != "calibrations" and any(d.glob("*.csv"))
        )

    def trial_locations(self):
        """Locations with at least one complete rest + movement + max trial."""
        return [loc for loc in self.data_locations()
                if analysis.find_trials(self.data_dir / loc)]

    def unmet(self, requirement):
        """Reason `requirement` isn't met right now, or None if it is."""
        met = {
            "board": lambda: self.link is not None or bool(self.ports()),
            "data": lambda: bool(self.data_locations()),
            "trials": lambda: bool(self.trial_locations()),
        }[requirement]()
        return None if met else REQUIREMENTS[requirement]


def choose_location(session, existing=None):
    """Ask which body part. With `existing`, only offer those data folders."""
    if existing is not None:
        return choose("Which body part?", existing)
    names = session.data_locations()
    options = [(f"{name}   (existing data)", name) for name in names]
    options += [(name, name) for _, name in session.board_locations
                if recording.slug(name) not in names]
    options.append(("new name...", None))
    choice = choose("Which body part?", options)
    if choice is None:
        choice = ask("Name (e.g. forearm flexors)")
    return recording.slug(choice)


# --- actions ---

def do_calibrate(session):
    link = session.get_link()
    if session.board_locations:
        options = [(f"{name}", lid) for lid, name in session.board_locations]
        location_id = choose("Which firmware location profile?", options)
    else:
        location_id = ask("Firmware location number", 0, int)
    print("Follow the prompts below.")
    link.reset_input()
    calibration.calibrate(link, session.data_dir, location_id)


def do_info(session):
    link = session.get_link()
    link.reset_input()
    calibration.info(link)


def do_collect(session):
    link = session.get_link()
    location = choose_location(session)
    trials = ask("Trials", 3, int)
    link.reset_input()
    recording.collect(link, session.data_dir, location, trials, DEFAULT_SECONDS)


def do_record(session):
    link = session.get_link()
    location = choose_location(session)
    label = choose("What will the subject do?", TASKS + ["other..."])
    if label == "other...":
        label = ask("Label")
    trial = ask("Trial number", 1, int)
    seconds = ask("Seconds", DEFAULT_SECONDS.get(label, 10), float)
    link.reset_input()
    recording.record(link, session.data_dir, location, label, trial, seconds)


def do_plot(session):
    location = choose_location(session, existing=session.data_locations())
    task = choose("Which runs?", TASKS + ["all"])
    pattern = "*.csv" if task == "all" else f"*_{task}_t*.csv"
    files = sorted((session.data_dir / location).glob(pattern))
    if not files:
        print(f"No {task} runs for {location}.")
        return
    print(f"Plotting {len(files)} runs; close the window to return to the menu.")
    plotting.plot_runs(files)


def do_profile(session):
    location = choose_location(session, existing=session.trial_locations())
    report_profile(session.data_dir, location)


ACTIONS = [
    Action("User (each person / session)", "calibrate",
           "fit thresholds to this user (~19 s)", "board", do_calibrate),
    Action("User (each person / session)", "info",
           "show the board's saved calibration", "board", do_info),
    Action("Developer (once per electrode location)", "collect",
           "guided rest / movement / max trials", "board", do_collect),
    Action("Developer (once per electrode location)", "record",
           "one labeled run", "board", do_record),
    Action("Developer (once per electrode location)", "plot",
           "graph recorded runs", "data", do_plot),
    Action("Developer (once per electrode location)", "profile",
           "compute a location's profile factors", "trials", do_profile),
]


# --- menu loop ---

def show_menu(session):
    """Print the menu; return {number: action} for the actions that can run."""
    board = session.link.port if session.link else (
        ", ".join(session.ports()) or "not connected")
    locations = session.data_locations()
    data = f"{len(locations)} location(s) ({', '.join(locations)})" if locations else "none"
    print(f"\nEMG tool   board: {board}   data: {data}")

    enabled = {}
    section = None
    for number, action in enumerate(ACTIONS, 1):
        if action.section != section:
            section = action.section
            print(f"\n{section}")
        reason = session.unmet(action.requirement)
        if reason:
            print(f"  -  {action.name:<10} {action.description}   ({reason})")
        else:
            print(f"  {number}  {action.name:<10} {action.description}")
            enabled[str(number)] = action
    print("\n  q  quit")
    return enabled


def run_action(session, action):
    try:
        action.handler(session)
    except KeyboardInterrupt:
        print("\nCancelled.")
    except SystemExit as error:
        # the shared code reports problems (timeouts, missing data) this way
        if error.code not in (None, 0):
            print(error.code)
    except (serial.SerialException, OSError) as error:
        print(f"Board disconnected ({error}).")
        session.close()


def run(data_dir, port=None):
    session = Session(data_dir, port)
    try:
        while True:
            enabled = show_menu(session)
            choice = input("Choose: ").strip().lower()
            if choice in ("q", "quit"):
                break
            action = enabled.get(choice)
            if action is None:
                print("Not available. Pick a numbered action or q.")
                continue
            run_action(session, action)
    except (KeyboardInterrupt, EOFError):
        print()
    finally:
        session.close()
