"""Unit tests for the parts of emgtool that don't need a board.

Run from the repo root:  tools/.venv/bin/python -m unittest discover -s tools/tests -t tools
"""

import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import numpy as np

from emgtool.__main__ import main
from emgtool.analysis import derive_profile, find_trials, load_run, profile_table_row, trial_metrics
from emgtool.calibration import parse_block, parse_measurement
from emgtool.recording import save_run
from emgtool import menu
from emgtool.serial_link import RowBuilder, parse_location_list, parse_telemetry


class TelemetryTest(unittest.TestCase):
    def test_parse(self):
        self.assertEqual(parse_telemetry(">power:1234"), ("power", 1234.0))
        self.assertIsNone(parse_telemetry("Relax (3 s)"))
        self.assertIsNone(parse_telemetry(">power:abc"))

    def test_rows_split_on_repeated_field(self):
        builder = RowBuilder()
        self.assertIsNone(builder.add("power", 1))
        self.assertIsNone(builder.add("activation", 2))
        self.assertEqual(builder.add("power", 3), {"power": 1, "activation": 2})


class CalibrationParseTest(unittest.TestCase):
    def test_block(self):
        block = parse_block([
            "location:        1 Forearm flexors",
            "status:          calibrated",
            "noise threshold: 626",
            "max contraction: 8003",
            "next 'c' uses:   2 Biceps",
        ])
        self.assertEqual(block["location"], "1 Forearm flexors")
        self.assertEqual(block["noise_threshold"], 626)
        self.assertEqual(block["max_contraction"], 8003)
        self.assertEqual(block["next_c_uses"], "2 Biceps")

    def test_measurement(self):
        self.assertEqual(
            parse_measurement("rest p99: 447, max contraction: 8003, SNR: 17.9"),
            {"rest_p99": 447, "max_contraction": 8003, "snr": 17.9},
        )


def fake_trial(rng, rest_level=100, move_level=2000, hold_level=8000):
    rest = rng.normal(rest_level, rest_level * 0.2, 1000).clip(0)
    movement = np.where(np.arange(1000) % 100 < 50, move_level, rest_level).astype(float)
    hold = np.concatenate([np.linspace(0, hold_level, 50),
                           rng.normal(hold_level, hold_level * 0.1, 450)])
    return rest, movement, hold


class ProfileTest(unittest.TestCase):
    def test_derived_factors_are_sensible(self):
        rng = np.random.default_rng(1)
        metrics = [trial_metrics(*fake_trial(rng)) for _ in range(3)]
        factors, warnings = derive_profile(metrics)
        self.assertEqual(warnings, [])
        # noise threshold clears rest spikes but stays well below max contraction
        self.assertGreater(factors["noiseFactor"], 1.0)
        self.assertLess(factors["noiseFactor"], 3.0)
        # on threshold lies between the noise floor and typical movement
        rest_p99 = metrics[0]["rest_p99"]
        mvc = metrics[0]["mvc_power"]
        on_power = factors["onFraction"] * mvc
        self.assertGreater(on_power, factors["noiseFactor"] * rest_p99)
        self.assertLess(on_power, 2000)
        self.assertAlmostEqual(factors["offFraction"], factors["onFraction"] / 2)
        # the lead-in ramp is skipped, so mvc is close to the hold level
        self.assertAlmostEqual(mvc, 8000, delta=200)
        self.assertGreater(factors["artifactFactor"], 1.0)
        self.assertIn('"Test",', profile_table_row("Test", factors))

    def test_weak_signal_warns(self):
        rest = np.zeros(1000)
        movement = np.full(1000, 20.0)
        hold = np.full(500, 29.0)
        _, warnings = derive_profile([trial_metrics(rest, movement, hold)])
        self.assertTrue(any("rest p99 is 0" in w for w in warnings))


class StorageTest(unittest.TestCase):
    def test_save_and_find_trials(self):
        rng = np.random.default_rng(2)
        with tempfile.TemporaryDirectory() as tmp:
            for trial in (1, 2):
                for label, data in zip(("rest", "movement", "max"), fake_trial(rng)):
                    rows = [{"power": float(v), "activation": 0, "host_time_s": i * 0.01}
                            for i, v in enumerate(data)]
                    save_run(rows, tmp, "Forearm Flexors", label, trial, "fake")
            # an incomplete trial is ignored
            save_run([{"power": 1.0, "host_time_s": 0}], tmp, "Forearm Flexors", "rest", 3, "fake")

            trials = find_trials(Path(tmp) / "forearm_flexors")
            self.assertEqual(sorted(trials), [1, 2])
            meta, power = load_run(trials[1]["max"])
            self.assertEqual(meta["label"], "max")
            self.assertEqual(len(power), 500)
            header = trials[1]["rest"].read_text().splitlines()[0]
            self.assertEqual(header, "group,t_s,host_time_s,activation,power")
            self.assertEqual(json.loads(trials[1]["rest"].with_suffix(".json").read_text())["trial"], 1)


class CollectArgsTest(unittest.TestCase):
    def durations_for(self, argv):
        link = mock.Mock()
        with mock.patch("emgtool.__main__._open_link", return_value=link), \
             mock.patch("emgtool.recording.collect") as collect:
            main(argv)
        return collect.call_args.args[4]

    def test_defaults_when_no_flags(self):
        self.assertEqual(self.durations_for(["collect", "biceps"]),
                         {"rest": 10, "movement": 10, "max": 5})

    def test_one_flag_overrides_only_its_task(self):
        self.assertEqual(self.durations_for(["collect", "biceps", "--rest", "4"]),
                         {"rest": 4.0, "movement": 10, "max": 5})


def write_complete_trial(data_dir, location, trial, rng):
    for label, data in zip(("rest", "movement", "max"), fake_trial(rng)):
        rows = [{"power": float(v), "host_time_s": 0} for v in data]
        save_run(rows, data_dir, location, label, trial, "fake")


class BootParseTest(unittest.TestCase):
    def test_location_list(self):
        boot = [
            "Commands: 's' stats, 'r' reset stats, 'c' calibrate,",
            "digit = select location:",
            "0 Generic",
            "1 Forearm flexors",
            "----- Calibration -----",
            "location:        1 Forearm flexors",
        ]
        self.assertEqual(parse_location_list(boot), [(0, "Generic"), (1, "Forearm flexors")])


class MenuInputTest(unittest.TestCase):
    def test_choose_reasks_until_valid(self):
        with mock.patch("builtins.input", side_effect=["9", "x", "2"]), \
             mock.patch("builtins.print"):
            self.assertEqual(menu.choose("Pick", ["a", ("B label", "b")]), "b")

    def test_ask_default_and_conversion(self):
        with mock.patch("builtins.input", side_effect=["", "abc", "7"]), \
             mock.patch("builtins.print"):
            self.assertEqual(menu.ask("Trials", 3, int), 3)
            self.assertEqual(menu.ask("Trials", 3, int), 7)


class MenuSessionTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.data = Path(self.tmp.name)
        patcher = mock.patch("emgtool.menu.detect_ports", return_value=[])
        patcher.start()
        self.addCleanup(patcher.stop)
        self.addCleanup(self.tmp.cleanup)

    def test_availability(self):
        session = menu.Session(self.data)
        self.assertEqual(session.unmet("board"), "needs board")
        self.assertEqual(session.unmet("data"), "no recordings yet")
        save_run([{"power": 1.0, "host_time_s": 0}], self.data, "biceps", "rest", 1, "fake")
        self.assertIsNone(session.unmet("data"))
        self.assertEqual(session.unmet("trials"), "needs a complete collect")
        write_complete_trial(self.data, "biceps", 1, np.random.default_rng(3))
        self.assertIsNone(session.unmet("trials"))
        with mock.patch("emgtool.menu.detect_ports", return_value=["/dev/fake"]):
            self.assertIsNone(session.unmet("board"))

    def test_board_actions_not_selectable_without_board(self):
        with mock.patch("builtins.print"):
            enabled = menu.show_menu(menu.Session(self.data))
        self.assertEqual(enabled, {})

    def test_plot_session(self):
        rng = np.random.default_rng(4)
        write_complete_trial(self.data, "biceps", 1, rng)
        write_complete_trial(self.data, "biceps", 2, rng)
        # menu: 5 = plot, location 1 (biceps), task 3 (max), then quit
        with mock.patch("builtins.input", side_effect=["5", "1", "3", "q"]), \
             mock.patch("builtins.print"), \
             mock.patch("emgtool.plotting.plot_runs") as plot_runs:
            menu.run(self.data)
        files = plot_runs.call_args.args[0]
        self.assertEqual(len(files), 2)
        self.assertTrue(all("_max_t" in f.name for f in files))

    def test_errors_return_to_menu(self):
        write_complete_trial(self.data, "biceps", 1, np.random.default_rng(5))
        # 6 = profile; make it fail, then quit: the menu must survive
        with mock.patch("builtins.input", side_effect=["6", "1", "q"]), \
             mock.patch("builtins.print"), \
             mock.patch("emgtool.menu.report_profile", side_effect=SystemExit("boom")):
            menu.run(self.data)


if __name__ == "__main__":
    unittest.main()
