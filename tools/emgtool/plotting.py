"""Plotting recorded runs."""

import csv
import json
from pathlib import Path

import matplotlib.pyplot as plt


def _load(csv_path):
    csv_path = Path(csv_path)
    meta_path = csv_path.with_suffix(".json")
    meta = json.loads(meta_path.read_text()) if meta_path.exists() else {}
    with open(csv_path, newline="") as f:
        rows = list(csv.DictReader(f))
    return meta, rows


def plot_runs(csv_paths, save_to=None):
    """Plot power (and activation, if any run has it) against time."""
    runs = [_load(p) for p in csv_paths]
    has_activation = any(
        row.get("activation") not in (None, "", "0") for _, rows in runs for row in rows
    )
    fig, axes = plt.subplots(
        2 if has_activation else 1, 1, sharex=True, squeeze=False, figsize=(10, 6)
    )
    power_ax = axes[0][0]
    for path, (meta, rows) in zip(csv_paths, runs):
        label = (f"{meta['location']} {meta['label']} t{meta['trial']}"
                 if meta else Path(path).stem)
        t = [float(r["t_s"]) for r in rows]
        power_ax.plot(t, [float(r["power"]) for r in rows], label=label, linewidth=0.8)
        if has_activation:
            axes[1][0].plot(t, [float(r.get("activation") or 0) for r in rows],
                            label=label, linewidth=0.8)

    power_ax.set_ylabel("power (group average)")
    power_ax.legend(loc="upper right", fontsize="small")
    power_ax.grid(alpha=0.3)
    if has_activation:
        axes[1][0].set_ylabel("activation (%)")
        axes[1][0].set_ylim(0, 105)
        axes[1][0].grid(alpha=0.3)
    axes[-1][0].set_xlabel("time (s)")
    fig.tight_layout()

    if save_to:
        fig.savefig(save_to, dpi=120)
        print(f"Saved plot to {save_to}")
    else:
        plt.show()
