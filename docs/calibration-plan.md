# Per-user EMG calibration — roadmap + first step

## Context
Today every threshold is a compile-time constant tuned on one person
(`threshold = 0` in `src/emg_envelope.cpp`, `ARTIFACT_LIMIT 30000L` in
`include/emg_envelope.h`). EMG amplitude varies several-fold between people and
between sessions (skin impedance, fat, electrode placement), so these won't
transfer. Goal: developers characterize sensor locations once (as *relative*
factors), then each user runs a short guided calibration that turns those factors
into personal absolute thresholds, normalizes output to % of their max contraction,
and saves the result on the board.

Key principle: **developer data = ratios and sanity ranges; user calibration =
absolute numbers.** Band-pass (20–150 Hz) and 60 Hz notch stay fixed — they're
physics, not physiology.

## Roadmap
1. **Developer data collection** (manual, uses existing `s`/`r` stats): per
   location, rest / repeated-movement / sustained-max runs × 3 trials. Derive per
   location: threshold factor (× rest p99), expected SNR (movement p90 ÷ rest p99),
   artifact factor (× max-contraction power).
2. **Firmware calibration mode + location profiles + EEPROM** ← *this plan's
   implementation step (below).*
3. **Python host tool**: log the serial stream to CSV with labels, drive
   calibration from the PC, plot runs, and compute developer profiles from the
   collected trials (replaces manual spreadsheet work from step 1).
4. **Activation detection**: on/off with hysteresis on the normalized signal →
   a control output (e.g. servo/motor module).
5. **Multi-movement prediction**: multiple channels + labeled per-user training
   data collected by an extended calibration routine. Prerequisites found in code:
   - `lib/EMGFilters/src/EMGFilters.cpp:166-168` keeps the filter state in
     file-`static` `LPF`/`HPF`/`AHF`, so two `EMGFilters` objects would share
     state. Multi-channel needs that state moved into the class.
   - Filter cost is ~520 µs/sample, so 2 channels at 1 kHz won't fit on the Uno;
     either 500 Hz per channel or a faster board.
   - Fix the missing comma at `EMGFilters.cpp:47` before ever using 50 Hz/500 Hz.

## Step 2 — implementation

### New files
- **`include/emg_profiles.h` / `src/emg_profiles.cpp`**: `const` table of
  `LocationProfile { name, noiseFactor, onFraction, offFraction, minSnr,
  artifactFactor }` (placeholder values, e.g. 1.4 / 0.15 / 0.08 / 3.0 / 3.0, until
  step-1 data exists) plus a `GENERIC` default entry.
- **`include/emg_calibration.h` / `src/emg_calibration.cpp`**: a non-blocking
  state machine fed one finished group at a time (same pattern as
  `emgStatsAdd()`), so 1 kHz sampling never stops. Phases: settle 2 s → rest 8 s
  ("relax completely") → max contraction 2 × 3 s holds with 3 s rest between
  ("squeeze as hard as comfortable and hold") → compute → validate → save.
  - Compute: `noiseThreshold = noiseFactor × rest p99`; `mvcPower` = mean of
    the hold phases; `on/offThreshold = on/offFraction × mvcPower`;
    `artifactLimit = artifactFactor × mvcPower`.
  - Validate (fail = print reason, keep previous calibration): clipped groups > 0
    → "lower sensor gain"; SNR `mvcPower / rest p99` < `minSnr` → "reposition
    electrodes"; too many artifact groups → "hold still / tape cables, retry".
- **`include/emg_storage.h` / `src/emg_storage.cpp`**: save/load
  `UserCalibration { magic, version, locationId, noiseThreshold, mvcPower,
  onThreshold, offThreshold, artifactLimit, checksum }` to EEPROM
  (`<EEPROM.h>`, `EEPROM.put/get`). Load at boot; on bad magic/version/checksum
  fall back to the selected profile's defaults.

### Changes to existing files
- **`src/emg_stats.cpp` / `include/emg_stats.h`**: add `EmgStatsSummary
  emgStatsGetSummary()` (counts, min/max/mean/std, p10/p50/p90/p99, clipped,
  artifact) and have `emgStatsPrint()` use it. Calibration reuses the stats module
  per phase (`emgStatsReset()` at phase start, summary at phase end) instead of
  duplicating the histogram/percentile code. Replace its use of `ARTIFACT_LIMIT`
  with the runtime getter.
- **`src/emg_envelope.cpp` / `include/emg_envelope.h`**: make the per-sample
  `threshold` and `ARTIFACT_LIMIT` runtime values with setters
  (`emgEnvelopeSetNoiseThreshold()`, `emgEnvelopeSetArtifactLimit()`, plus getters).
  Move noise gating from per-sample power to the **group average** so it matches
  the group-level stats the threshold is derived from (behavior change; per-sample
  threshold is 0 today, so default output is unchanged). Add
  `EmgEnvelope.activation` = power × 100 / mvcPower, clamped 0–100 (0 if
  uncalibrated).
- **`src/main.cpp`**: new serial commands — `c` start calibration, `0`–`9`
  select location profile, `i` print current calibration; existing `s`/`r` keep
  working. Print `>activation:` alongside `>power:`. Skip `emgStatsAdd()` while
  calibration is running (it owns the stats module then).

### Notes
- RAM: currently ~706 B of 2048; new state is small (<100 B). Keep strings in
  `F()`.
- Calibration prompts are short lines; only the final summary is long, printed
  once after sampling for the phase is done.

## Verification
1. `pio run` builds cleanly; check RAM/flash usage.
2. Host-side sanity test (like the earlier histogram test) of the threshold
   computation and EEPROM struct checksum with fake summaries.
3. On hardware: send `c`, follow prompts, confirm printed thresholds look sane
   relative to the `s` stats from manual runs; `>activation:` reads ~0 at rest and
   approaches 100 on hard contraction.
4. Power-cycle the board → `i` shows the same calibration (EEPROM load works).
5. Failure paths: lift an electrode during calibration (should fail SNR check and
   keep the old values); relax during the "squeeze" phase (low SNR → reject).
