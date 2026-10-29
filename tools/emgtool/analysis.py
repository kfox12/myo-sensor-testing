"""Loading recorded runs and deriving location profiles from them.

Pure functions (no serial, no plotting) so they can be unit tested.

Profile derivation mirrors what the firmware does with a profile during
calibration (src/emg_calibration.cpp):
    noiseThreshold = noiseFactor    x rest p99
    on / off       = on/offFraction x mvcPower
    artifactLimit  = artifactFactor x mvcPower
    reject if mvcPower / rest p99 < minSnr
so each factor is chosen from the developer trials to make those
formulas land in sensible places. The margins below are starting
heuristics; adjust them once real data shows how they behave.
"""

import csv
import json
import math
from collections import defaultdict
from pathlib import Path

import numpy as np

# same as the firmware: skip the reaction/ramp at the start of a hold
HOLD_LEAD_IN_SECONDS = 0.5
GROUP_SECONDS = 0.01

NOISE_MARGIN = 1.1     # noise threshold sits 10% above the worst rest max
SNR_ACCEPT_SHARE = 0.5 # accept user calibrations half as clean as the worst dev trial
ARTIFACT_MARGIN = 1.5  # artifact limit sits 50% above the strongest hold peak
OFF_SHARE_OF_ON = 0.5  # hysteresis: off threshold is half the on threshold


def load_run(csv_path):
    """Return (metadata dict, power array) for one recorded run."""
    csv_path = Path(csv_path)
    meta = json.loads(csv_path.with_suffix(".json").read_text())
    with open(csv_path, newline="") as f:
        power = np.array([float(row["power"]) for row in csv.DictReader(f)])
    return meta, power


def summarize(power):
    """Basic statistics of a power array (empty array -> zeros)."""
    if len(power) == 0:
        return {k: 0.0 for k in ("groups", "mean", "p10", "p50", "p90", "p99", "max")}
    return {
        "groups": len(power),
        "mean": float(np.mean(power)),
        "p10": float(np.percentile(power, 10)),
        "p50": float(np.percentile(power, 50)),
        "p90": float(np.percentile(power, 90)),
        "p99": float(np.percentile(power, 99)),
        "max": float(np.max(power)),
    }


def trial_metrics(rest, movement, hold):
    """Ratios from one trial's rest / movement / max-hold power arrays."""
    lead_in = round(HOLD_LEAD_IN_SECONDS / GROUP_SECONDS)
    rest_s = summarize(rest)
    move_s = summarize(movement)
    hold_s = summarize(hold[lead_in:])

    # like the firmware, a perfectly quiet rest still gets a floor of 1
    rest_p99 = max(rest_s["p99"], 1.0)
    mvc = hold_s["mean"]
    return {
        "rest_p99": rest_s["p99"],
        "mvc_power": mvc,
        "noise_ratio": rest_s["max"] / rest_p99,       # how far rest spikes above its p99
        "calibration_snr": mvc / rest_p99,             # what the firmware's SNR check sees
        "movement_snr": move_s["p90"] / rest_p99,      # expected SNR during real use
        "movement_fraction": move_s["p90"] / mvc if mvc else 0.0,
        "hold_peak_ratio": hold_s["max"] / mvc if mvc else 0.0,
    }


def derive_profile(trials):
    """Profile factors from a list of trial_metrics() dicts.

    Returns (factors dict, warnings list).
    """
    warnings = []
    if not trials:
        raise ValueError("no complete trials")
    if any(t["rest_p99"] < 1 for t in trials):
        warnings.append("rest p99 is 0 in some trials: the signal may be too weak "
                        "to separate from quantization; ratios are unreliable")
    if any(t["mvc_power"] <= 0 for t in trials):
        raise ValueError("a max-hold run has zero mean power")

    noise_factor = NOISE_MARGIN * max(t["noise_ratio"] for t in trials)
    min_snr = SNR_ACCEPT_SHARE * min(t["calibration_snr"] for t in trials)
    artifact_factor = ARTIFACT_MARGIN * max(t["hold_peak_ratio"] for t in trials)

    # on threshold: geometric midpoint between the noise threshold and
    # typical movement (both as a fraction of max contraction)
    on_values = []
    for t in trials:
        noise_fraction = noise_factor * max(t["rest_p99"], 1.0) / t["mvc_power"]
        if t["movement_fraction"] <= noise_fraction:
            warnings.append(f"movement p90 does not clear the noise threshold "
                            f"(trial with rest p99 {t['rest_p99']:.0f})")
            continue
        on_values.append(math.sqrt(noise_fraction * t["movement_fraction"]))
    on_fraction = float(np.median(on_values)) if on_values else 0.0
    off_fraction = OFF_SHARE_OF_ON * on_fraction

    if min_snr < 2:
        warnings.append(f"min SNR {min_snr:.1f} is very low; calibration would "
                        "accept almost anything")

    factors = {
        "noiseFactor": noise_factor,
        "onFraction": on_fraction,
        "offFraction": off_fraction,
        "minSnr": min_snr,
        "artifactFactor": artifact_factor,
    }
    return factors, warnings


def find_trials(location_dir):
    """Group a location folder's runs into complete trials.

    Returns {trial number: {label: csv path}}, keeping the newest run for
    each (trial, label) and only trials that have rest, movement and max.
    """
    found = defaultdict(dict)
    for meta_path in sorted(Path(location_dir).glob("*.json")):
        meta = json.loads(meta_path.read_text())
        # sorted by timestamped name, so later runs overwrite earlier ones
        found[meta["trial"]][meta["label"]] = meta_path.with_suffix(".csv")
    return {
        trial: labels
        for trial, labels in sorted(found.items())
        if {"rest", "movement", "max"} <= labels.keys()
    }


def profile_table_row(name, factors):
    """Format a row for the PROFILES table in src/emg_profiles.cpp."""
    quoted = f'"{name}",'
    return (f"    {{{quoted:<21}{factors['noiseFactor']:.2f}f, "
            f"{factors['onFraction']:.3f}f, {factors['offFraction']:.3f}f, "
            f"{factors['minSnr']:.1f}f, {factors['artifactFactor']:.2f}f}},")
