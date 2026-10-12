#!/usr/bin/env python3
import pathlib
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from audit_v233_reshade_log_timeline import audit

ROOT = HERE.parents[1]

class V233ConservativeLogAudit(unittest.TestCase):
    def test_info_silence_is_not_a_frame_hitch(self):
        fake = (
            "15:01:42:357 [14236] | INFO | [DSRRL V229 GLOSS CUT] probe=140,140\n"
            "15:01:47:667 [  368] | INFO | [DSRRL V210 MATERIAL CUT] raw=1\n"
        )
        result = audit(fake)
        self.assertEqual(result["present_profile_windows"], 0)
        self.assertEqual(result["hitch_frame_samples"], 0)
        self.assertFalse(result["can_attribute_stutter_cause"])
        self.assertEqual(result["largest_log_silence_gaps"][0]["gap_ms"], 5310)
        self.assertFalse(result["largest_log_silence_gaps"][0]["is_frame_hitch"])

    def test_existing_present_and_hitch_markers(self):
        fake = (
            "15:01:42:357 [8120] | INFO | [DSRRL STUTTER R43] window=5s present_frames=100 present_max_ms=70.0\n"
            "15:01:42:357 [8120] | INFO | [DSRRL HITCH FRAME] gap_ms=70.0 dds=calls:2,all_ms:3.2,present_ms:0\n"
        )
        result = audit(fake)
        self.assertTrue(result["can_attribute_stutter_cause"])
        self.assertEqual(result["present_profile_windows"], 1)
        self.assertEqual(result["hitch_frame_samples"], 1)

    def test_diagnostic_off_by_default_and_existing_profiler_contract(self):
        config = (ROOT / "renderer-core/tools/configure_v203_clean_equipment.ps1").read_text()
        profiler = (ROOT / "renderer-core/include/dsrrl/runtime/stutter_profiler.hpp").read_text()
        wrapper = (ROOT / "renderer-core/tools/build_v233_transition_stutter_diagnostic.ps1").read_text()
        for flag in ("DSRRL_STUTTER_PROFILE", "DSRRL_STUTTER_HITCH_TRACE"):
            self.assertIn('" -D'+flag+'=OFF"'.replace('" ','"'), config)
            self.assertIn('" -D'+flag+'=ON"'.replace('" ','"'), wrapper)
        for marker in ("present_previous.exchange", "hitch_top_capacity = 8u",
                       "hitch_index(stage id)", "case stage::sidecar_load"):
            self.assertIn(marker, profiler)
        self.assertIn("finally", wrapper)
        self.assertIn("build_v230_source_audit_live_sidecar.ps1", wrapper)

if __name__ == "__main__":
    unittest.main(verbosity=2)
