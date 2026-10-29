"""Printed reports shared by the subcommands and the menu."""

from pathlib import Path

from .analysis import derive_profile, find_trials, load_run, profile_table_row, trial_metrics


def report_profile(data_dir, location, name=None, out=print):
    """Print per-trial ratios, derived factors, and a PROFILES table row."""
    location_dir = Path(data_dir) / location
    trials = find_trials(location_dir)
    if not trials:
        raise SystemExit(f"No complete trials (rest + movement + max) in {location_dir}")

    metrics = []
    out(f"{'trial':>5} {'rest p99':>9} {'mvc':>9} {'noise x':>8} "
        f"{'cal SNR':>8} {'move SNR':>9} {'peak x':>7}")
    for trial, paths in trials.items():
        _, rest = load_run(paths["rest"])
        _, movement = load_run(paths["movement"])
        _, hold = load_run(paths["max"])
        m = trial_metrics(rest, movement, hold)
        metrics.append(m)
        out(f"{trial:>5} {m['rest_p99']:>9.0f} {m['mvc_power']:>9.0f} "
            f"{m['noise_ratio']:>8.2f} {m['calibration_snr']:>8.1f} "
            f"{m['movement_snr']:>9.1f} {m['hold_peak_ratio']:>7.2f}")

    factors, warnings = derive_profile(metrics)
    out("")
    for factor, value in factors.items():
        out(f"{factor:>15}: {value:.3f}")
    for warning in warnings:
        out(f"WARNING: {warning}")
    out("\nRow for PROFILES in src/emg_profiles.cpp:")
    out(profile_table_row(name or location.replace("_", " ").title(), factors))
