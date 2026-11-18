# Archived runs

Old recordings kept for reference only. Don't use them to derive profiles.
`emgtool` doesn't see this folder, because it only lists folders directly under
`data/`.

## forearm_flexors_faulty_sensor/
Forearm flexor runs from 2026-10-19 (rest, movement, max × 3 trials).

These were recorded with a faulty EMG sensor amplifier. Its output rested near
0 V (ADC ≈ 8) instead of about 1.5 V (ADC ≈ 300), so the lower half of the
signal was clipped off. Because of that:
- rest power reads 0;
- max-hold power is about 60–100 instead of the thousands a working sensor gives;
- `profile` derives factors of 0 from them.

The sensor was replaced on 2026-11-18. Re-collect this location with
`./emg collect forearm_flexors`.
