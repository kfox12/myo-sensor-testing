# emgtool — host-side tools for the EMG firmware

Records the board's serial stream to labeled CSV, drives calibration from
the PC, plots runs, and computes location profiles for
`src/emg_profiles.cpp` (roadmap steps 1 and 3 in `docs/calibration-plan.md`).

## Setup (once)
```
python3 -m venv tools/.venv
tools/.venv/bin/pip install -r tools/requirements.txt
```

## Use
From the repo root, run:
```
./emg
```
This opens a menu. Each action has a one-line description, and actions that
can't run right now are marked with the reason (e.g. `(needs board)`).
Whatever an action needs, such as the body part or the number of trials, it
asks for. Ctrl-C cancels the current action and returns to the menu.

| Section | Action | What it does |
|---|---|---|
| User (each person / session) | calibrate | the board's ~19 s calibration; result saved on the board and in `data/calibrations/` |
| | info | show the board's saved calibration |
| Developer (once per electrode location) | collect | guided rest 10 s / repeated movement 10 s / max hold 5 s, per trial |
| | record | one labeled run |
| | plot | graph a location's rest, movement, max, or all runs |
| | profile | per-trial ratios, derived factors, and a ready-to-paste `PROFILES` row |

Close the PlatformIO serial monitor first, because only one program can use
the port. The board resets when the menu first connects; after that the
connection stays open until you quit.

### Scripting
The same actions are available as subcommands, e.g.
`./emg collect forearm_flexors --trials 3`, `./emg plot data/forearm_flexors/*_max_*.csv`,
`./emg profile forearm_flexors`. Run `./emg --help` for the full list.

## Data layout
Runs go to `data/` at the repo root (change with `--data-dir`):
```
data/<location>/<location>_<label>_t<trial>_<timestamp>.csv   group, t_s, host_time_s, activation, power
data/<location>/<same name>.json                               location, label, trial, duration, time, port
data/calibrations/calibration_<timestamp>.json                 calibrate results
```
`t_s` comes from the firmware's 10 ms group clock. `host_time_s` is when
the PC received the group, which is useful for spotting dropped data.

## Developer workflow (roadmap step 1)
1. Place the electrodes, run `./emg`, choose **collect**, pick the body part,
   and follow the prompts.
2. Use **plot** to check the runs: a flat rest, a clear on/off pattern in
   the movement runs, and a steady hold.
3. Use **profile** and paste the printed row into `PROFILES` in
   `src/emg_profiles.cpp`.

How `profile` picks each factor (see `emgtool/analysis.py`):
- **noiseFactor**: 1.1 × the largest (rest max ÷ rest p99) across trials,
  so the noise threshold clears every rest spike.
- **minSnr**: 0.5 × the smallest (max-hold mean ÷ rest p99) across trials.
  This is the same ratio the firmware checks, so a user calibration that is
  half as clean as the worst developer trial is still accepted.
- **artifactFactor**: 1.5 × the largest (hold peak ÷ hold mean).
- **onFraction**: the geometric midpoint between the noise threshold and the
  movement p90, as a fraction of max contraction (median across trials).
  **offFraction** is half of that.

These margins are starting points. Adjust them once real data shows how they behave.
Percentiles here are exact, while the firmware's come from a coarse
histogram (±17%), so expect small differences.

## Tests
```
tools/.venv/bin/python -m unittest discover -s tools/tests -t tools
```
