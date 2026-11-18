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

Other programs for the same board, each its own environment:

| Command | Program |
|---|---|
| `pio run -e debug -t upload` | `src/debug_main.cpp`: hardware check that streams `>power:`, `>adcMin:`, `>adcMax:` and drives the servo (pin 9) from a fixed threshold; no calibration or commands |
| `pio run -e simple -t upload` | `simple.ino`: the original day-1 sketch |

Upload the plain `pio run -t upload` again to get the full firmware back.

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

## 4. Understanding calibration
Calibration (`c`, or **calibrate** in `./emg`) records you relaxed for 8 s,
then squeezing and holding twice for 3 s. It ends with a summary like:
```
rest p99: 13, max contraction: 4473, SNR: 344.1
clipped groups: 65 of 1300
```
The terms are explained in the glossary below. A run passes only if all three
checks pass:

| Failure message | Check | What to do |
|---|---|---|
| `signal clipped` | More than 1% of groups clipped (see below) | Squeeze firmly but not all-out; if it clips while relaxed, check the hardware |
| `too many artifacts` | More than 10% of groups were thrown out as bumps | Hold still, tape the cable down, retry |
| `signal too weak vs rest` | SNR below the location profile's minimum | Reposition the electrodes over the muscle belly, check skin contact |

### What clipping is
The sensor outputs a resting voltage of about 1.5 V (ADC ≈ 300). Muscle
activity makes that voltage swing rapidly above and below the resting level,
and bigger contractions make bigger swings. The sensor's output can only go
from 0 to about 3 V. When a swing would go past that range, the reading
stops at the limit, and the tip of the wave is cut off flat, or
"clipped". A microphone distorts the same way when you shout into it.

A clipped signal reads weaker than it really is and has lost its true shape,
so calibration can't trust it. The firmware counts a group as clipped if any
of its readings hit a rail (0 or 1023). With this sensor, clipping happens at
0, because the swings have only about 300 counts of room below the resting
level. A few clipped peaks during a hard squeeze are harmless and allowed (up
to 1% of groups). More than that fails the run.

- **Clipping while squeezing:** the contraction is too strong for the sensor's
  range. Calibration doesn't need an all-out effort; a firm, comfortable
  squeeze works because the on/off thresholds are set as a fraction of it.
- **Clipping while relaxed:** the resting level is wrong. Upload the debug
  program (`pio run -e debug -t upload`) and check that `adcMin`/`adcMax` sit
  near 300 at rest. A resting level near 0 means a power, wiring, or sensor
  fault.

### Glossary
| Term | Meaning |
|---|---|
| **EMG** | Electromyography: the electrical activity of muscles, measured here through electrodes on the skin. |
| **ADC** | Analog-to-digital converter: turns the voltage on pin A0 into a number from 0 to 1023 (0–5 V, about 4.9 mV per step). |
| **Rail** | Either end of the ADC's range (0 or 1023). A signal stuck at a rail is clipped. |
| **Sample** | One ADC reading. The firmware takes 1,000 per second (1 kHz). |
| **Group** | 10 samples (10 ms). The firmware reports one value per group. |
| **Power** | How strong the muscle activity is: the filtered signal, squared and averaged over a group. Near 0 at rest, thousands on a strong squeeze. |
| **Activation** | Power as a percentage (0–100) of your calibrated max contraction. |
| **p99** | 99th percentile: 99% of groups were at or below this value. *Rest p99* is the top of your normal resting noise, ignoring rare spikes. |
| **Max contraction (MVC)** | Maximum voluntary contraction: the average power during the calibration holds. It is the 100% reference for activation. |
| **SNR** | Signal-to-noise ratio: max contraction ÷ rest p99, so how many times stronger your squeeze is than your resting noise. Higher is better. |
| **Artifact** | A spike that doesn't come from the muscle, e.g. the sensor or cable being bumped. Groups above the artifact limit are zeroed. |
| **Notch / band-pass filter** | The notch filter removes 60 Hz mains hum; the band-pass keeps only the frequencies where muscle signals live. |
| **Location profile** | Per-body-location factors (e.g. forearm flexors) that turn your rest and max readings into thresholds. |
| **EEPROM** | The Uno's small memory that survives power-off; a passed calibration is saved there. |

## Layout
- `src/`, `include/`: firmware (envelope filtering, stats, calibration, profiles, EEPROM storage)
- `lib/EMGFilters/`: bundled EMG filter library
- `tools/`: `emgtool` host tool
- `data/`: recorded runs and calibrations
- `docs/calibration-plan.md`: design and roadmap
