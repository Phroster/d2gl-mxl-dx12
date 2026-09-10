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
    def test_visual_epoch_updates_wrap_gaps_and_old_recordings(self):
        def row(samples, epoch, ms, reason='2'):
            return dict(motion_epoch_active='1', motion_epoch_samples=str(samples), motion_epoch_reason=reason,
                        motion_update_ticks=str(epoch), motion_update_ms=str(ms), level='1')
        frames=[{'frame_id':str(n)} for n in (1,2,3,5,6,7)]
        producers={'1':row(0xffffffff,1000000,0xfffffff0), '2':row(0,1400000,24),
                   '3':row(0,1400000,24), '5':row(1,9999999,64),
                   '6':row(3,9999999,144), '7':row(4,19999999,184,'3')}
        result=diagnostics.epoch_summary(frames,producers,10000000)
        self.assertEqual(result['continuous_updates_checked'],1)
        self.assertEqual(result['maximum_absolute_step_error_ms'],0)
        self.assertEqual(result['observed_update_reasons'],{2:1,3:1})
        self.assertFalse(diagnostics.epoch_summary(frames,{},10000000)['available'])

    def test_comprehensive_context_and_invalid_presentation_are_not_zero(self):
        def frame(n, hr=0, count=1, refresh=1):
            return dict(frame_id=str(n), interval_ms='7', present_probed='1', present_stats_result=str(hr),
                        present_stats_count=str(count), present_refresh=str(refresh), present_probe_ms='.001')
        frames = [frame(1, count=0xffffffff, refresh=0xffffffff), frame(2, count=0, refresh=0),
                  frame(3, hr=0x887a000b), frame(4, count=2, refresh=9), frame(5, count=2, refresh=9), frame(7, count=3, refresh=12)]
        producers = {'1': dict(context_valid='1', level='1', player_mode='2', motion_panels='0', loot_targets='0'),
                     '2': dict(context_valid='1', level='2', player_mode='2', motion_panels='0', loot_targets='1'),
                     '3': dict(context_valid='0', level='0')}
        result = diagnostics.comprehensive_summary([], frames, producers)
        self.assertEqual(len(result['contexts']), 2)
        self.assertEqual(result['presentation']['successful_queries'], 5)
        self.assertEqual(result['presentation']['refresh_steps_for_consecutive_present_ids'], {1: 1})
        self.assertEqual(result['presentation']['repeated_statistics'], 1)
        old = diagnostics.comprehensive_summary([], [{'frame_id': '1'}], {})
        self.assertFalse(old['available'])
        self.assertIsNone(old['presentation']['query_cost_ms'])

    def test_motion_does_not_join_different_areas(self):
        def row(level, x):
            return dict(context_valid='1', level=str(level), motion_player_valid='1', motion_player_id='1',
                        motion_panels='0', motion_player_x=str(x), motion_player_y='0', motion_camera_x='0', motion_camera_y='0')
        result = diagnostics.motion_summary([{'frame_id': '1'}, {'frame_id': '2'}],
                                            {'1': row(1, 65536), '2': row(2, 100000000)}, 10000000)
        self.assertEqual(result['observed_player_frames'], 2)
        self.assertEqual(result['adjacent_player_pairs'], 0)

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

    def test_motion_counters_wrap_stale_reset_and_missing_frames(self):
        def row(samples, updates, **extra):
            return {"motion_valid": "1", "motion_game_type": "3", "motion_samples": str(samples),
                    "motion_client_updates": str(updates), "motion_elapsed_ticks": "700", "motion_clamped_ticks": "600", **extra}
        producers = {"1": row(0xffffffff, 0xffffffff), "2": row(0, 0), "3": row(0, 0),
                     "4": row(1, 1), "6": row(2, 2), "7": row(0, 0),
                     "8": row(1, 1, motion_game_type="0")}
        result = diagnostics.motion_summary([{"frame_id": key} for key in producers], producers, 10000)
        self.assertEqual(result["valid_clamp_frames"], 7)
        self.assertEqual(result["adjacent_counter_pairs"], 3)
        self.assertEqual(result["fresh_clamp_rows"], 2)
        self.assertEqual(result["unchanged_clamp_counter_pairs"], 1)
        self.assertEqual(result["fresh_clamp_elapsed_ms"]["max"], 70)
        self.assertEqual(result["fresh_rows_limited"], 2)
        self.assertEqual(result["client_update_steps"], {0: 1, 1: 2})

    def test_motion_player_identity_panels_signed_camera_and_older_logs(self):
        def row(x, camera, identity="1", panels="0"):
            return {"motion_player_valid": "1", "motion_player_id": identity, "motion_panels": panels,
                    "motion_player_x": str(x), "motion_player_y": "0", "motion_camera_x": str(camera & 0xffffffff), "motion_camera_y": "0"}
        producers = {"1": row(65536, -1), "2": row(131072, 1), "3": row(131072, 1),
                     "4": row(131072, 9, panels="1"), "5": row(131072, 9, identity="2", panels="1")}
        result = diagnostics.motion_summary([{"frame_id": key} for key in producers], producers, 0)
        self.assertEqual(result["adjacent_player_pairs"], 2)
        self.assertEqual(result["unchanged_player_pairs"], 1)
        self.assertEqual(result["player_step_tiles"]["max"], 1)
        self.assertEqual(result["camera_step_pixels"]["max"], 2)
        old = diagnostics.motion_summary([{"frame_id": "1"}], {"1": {}}, 0)
        self.assertFalse(old["available"])
        self.assertIsNone(old["fresh_clamp_elapsed_ms"])

    def test_signed_phase_is_decoded_and_both_limits_counted(self):
        def row(samples, elapsed, clamped):
            return {"motion_valid": "1", "motion_game_type": "3", "motion_samples": str(samples),
                    "motion_client_updates": "2", "motion_elapsed_ticks": str(elapsed & ((1 << 64)-1)),
                    "motion_clamped_ticks": str(clamped & ((1 << 64)-1))}
        producers = {"1": row(1, 0, 0), "2": row(2, -30000, -30000), "3": row(3, -900000, -200000), "4": row(4, 900000, 600000)}
        result = diagnostics.motion_summary([{"frame_id": key} for key in producers], producers, 10000000)
        self.assertEqual(result["negative_phase_rows"], 2)
        self.assertEqual(result["fresh_rows_limited"], 2)
        self.assertEqual(result["fresh_clamp_elapsed_ms"]["median"], -3)


if __name__ == "__main__":
    unittest.main()
