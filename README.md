# Myoelectric_intro

Arduino Uno firmware that reads an EMG (muscle) sensor, filters it, and
reports a per-user calibrated activation level. A Python host tool (`emgtool`)
records, plots, and calibrates from the PC.

## What you need
- Arduino Uno with an analog EMG sensor on pin **A0**
- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- Python 3 (for the host tool only)

## 1. Build and upload the firmware
From the repo root:
```
pio run -t upload
```
Or use the PlatformIO toolbar in VS Code (Upload). The board is an Uno at
115200 baud (see `platformio.ini`).

## 2. Watch the output
```
pio device monitor
```
The board prints a stream of `>power:` and `>activation:` values (the format
the Teleplot extension plots) and a help menu. Serial commands:

| Key | Action |
|---|---|
| `s` / `r` | print / reset statistics |
| `c` | run the ~19 s user calibration (saved to EEPROM) |
| `i` / `x` | show / erase the saved calibration |
| digit | select the electrode location profile |

Only one program can use the serial port at a time, so close the monitor
before using the host tool.

## 3. Host tool (optional)
Set up once:
```
python3 -m venv tools/.venv
tools/.venv/bin/pip install -r tools/requirements.txt
```
Then run the menu from the repo root:
```
./emg
```
It can calibrate the board, collect labeled runs into `data/`, plot them, and
compute location profiles. See [tools/README.md](tools/README.md) for details.

Tests:
```
tools/.venv/bin/python -m unittest discover -s tools/tests -t tools
```

## Layout
- `src/`, `include/`: firmware (envelope filtering, stats, calibration, profiles, EEPROM storage)
- `lib/EMGFilters/`: bundled EMG filter library
- `tools/`: `emgtool` host tool
- `data/`: recorded runs and calibrations
- `docs/calibration-plan.md`: design and roadmap
