#!/usr/bin/env python3
"""V237 invariants: only existing R43 QPC compile flags differ from V236."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]

class NoProfilerAB(unittest.TestCase):
    def test_exact_four_reversible_compile_flags(self):
        original=(ROOT/"renderer-core/tools/build_v233_transition_stutter_diagnostic.ps1").read_text()
        candidate=(ROOT/"renderer-core/tools/build_v237_v236_no_profiler.ps1").read_text()
        for option in ("DSRRL_STUTTER_PROFILE","DSRRL_STUTTER_HITCH_TRACE"):
            self.assertEqual(original.count("-D"+option+"=ON"),1)
            self.assertEqual(original.count(option+":BOOL=ON"),1)
            self.assertIn("-D"+option+"=OFF",candidate)
            self.assertIn(option+":BOOL=OFF",candidate)
        self.assertIn("finally",candidate)
        self.assertIn("build_v236_slot_sync.ps1",candidate)
        self.assertIn("V237 failed to restore original V233 builder",candidate)
    def test_native_semantic_source_preserved(self):
        v236=(ROOT/"renderer-core/tools/build_v236_slot_sync.ps1").read_text()
        patch=(ROOT/"renderer-core/tools/apply_v236_slot_sync.py").read_text()
        self.assertIn("pmetal_producer_state.cpp",v236)
        self.assertIn("pmetal_producer_state.cpp",patch)
        self.assertIn("std::lock_guard<std::mutex> lock(slot.mutex);",patch)
        self.assertIn("finally",v236)
    def test_cmake_flags_do_not_disable_renderer(self):
        config=(ROOT/"renderer-core/integrated/CMakeLists.txt").read_text()
        self.assertIn("if(DSRRL_STUTTER_PROFILE)",config)
        self.assertIn("if(DSRRL_STUTTER_HITCH_TRACE)",config)
        self.assertIn("target_compile_definitions(dsrrl_renderer_core_integrated PRIVATE DSRRL_STUTTER_PROFILE)",config)
        self.assertIn("target_compile_definitions(dsrrl_renderer_core_integrated PRIVATE DSRRL_STUTTER_HITCH_TRACE)",config)

if __name__=="__main__":unittest.main(verbosity=2)
