# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("diagnostics", Path(__file__).parents[1] / "tools/analyze-diagnostics.py")
diagnostics = importlib.util.module_from_spec(spec)
spec.loader.exec_module(diagnostics)


class LootAnalysis(unittest.TestCase):
    def test_frame_join_states_and_startup_filter(self):
        frames = [{"frame_id": str(i), "interval_ms": str(ms), "slow": str(int(ms > 10))}
                  for i, ms in [(1, 7), (2, 8), (3, 20), (4, 12), (5, 9)]]
        producers = {
            "3": {"loot_enabled": "1", "loot_targets": "60", "loot_selection_samples": "2",
                  "loot_pickup_sampled_ms": "0.8", "loot_selection_calls": "128", "loot_labels_ms": "2.0",
                  "loot_names_ms": "0.5", "loot_cache_uploads": "124"},
            "1": {"loot_enabled": "0", "loot_targets": "0"},
            "2": {"loot_enabled": "1", "loot_targets": "0"},
            "4": {"producer_build_ms": "4.0"},  # Older log is unknown, not effects off.
        }
        with tempfile.TemporaryDirectory() as folder:
            Path(folder, "lootfilterconf.json").write_text(json.dumps([
                {"name": "Current", "active": True, "rules": [{}, {}]},
                {"name": "Old", "active": False, "rules": [{}]}]))
            result = diagnostics.loot_work_summary(folder, frames, producers)
        self.assertEqual(result["frames_without_loot_metrics"], 2)
        self.assertEqual(result["states"]["effects_off"]["frames"], 1)
        self.assertEqual(result["states"]["effects_on_no_loot"]["frame_interval_ms"]["max"], 8)
        loot = result["states"]["effects_on_loot_visible"]
        self.assertEqual(loot["slow_frames"], 1)
        self.assertEqual(loot["counts_total"]["loot_cache_uploads"], 124)
        self.assertEqual(loot["mean_timed_pickup_sample_ms"], .4)
        self.assertEqual(loot["timings_ms"]["loot_labels_ms"]["max"], 2)
        self.assertEqual(result["saved_filter_at_launch"]["active_profiles"], [{"name": "Current", "rules": 2}])

    def test_old_or_absent_recording_is_not_zero_work(self):
        with tempfile.TemporaryDirectory() as folder:
            result = diagnostics.loot_work_summary(folder, [], {})
        self.assertEqual(result["states"], {})
        self.assertIsNone(result["saved_filter_at_launch"])


if __name__ == "__main__":
    unittest.main()
