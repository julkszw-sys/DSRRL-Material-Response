#!/usr/bin/env python3
"""V273 prebuild source negative oracles, no game or renderer mutation."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parent))
import apply_v273_lerp_stock_mw_control as trial

def verified_fixture() -> str:
    raw = trial.LERP.read_text(encoding="utf-8").replace("\r\n", "\n")
    # The production script is inserted AFTER the V271 overlay. Its exact
    # create-time function is checked by V271's own oracle in the native build.
    # For this prebuild Python-only mutation suite, inject a test marker.
    if "v271_preserves_stock_atmos_glow(" not in raw:
        raw = raw.replace(
            "bool final_postcondition(",
            "// test-only V271 marker: v271_preserves_stock_atmos_glow(\n"
            "bool final_postcondition(",
            1
        )
    return raw

class LerpOnlyContract(unittest.TestCase):
    def test_six_only_and_idempotent(self):
        original = verified_fixture()
        patched = trial.transform_lerp(original)
        self.assertEqual(patched.count(trial.GUARDED_IF), 6)
        self.assertEqual(patched.count(trial.DEFINE), 1)
        self.assertEqual(trial.transform_lerp(patched), patched)
        expected = original.replace(trial.HEADER,
                                    trial.HEADER+"\n"+trial.DEFINE+"\n", 1)
        expected = expected.replace(trial.BASE_IF, trial.GUARDED_IF)
        self.assertEqual(patched, expected)

    def test_guard_drift_fail_open(self):
        original = verified_fixture()
        with self.assertRaises(AssertionError):
            trial.transform_lerp(
                original.replace(trial.BASE_IF, "#if 1", 1))
        with self.assertRaises(AssertionError):
            trial.transform_lerp(
                original.replace("k_ptde_rgba_envspec_chain", "CORRUPTED", 1)
                .replace(trial.BASE_IF, "#if 1", 1))

    def test_only_lerp_registration_changes(self):
        original = trial.INTEGRATED.read_text(encoding="utf-8")
        patched = trial.transform_integrated(original)
        self.assertEqual(trial.transform_integrated(patched), patched)
        self.assertEqual(
            patched.replace(trial.CONTROL_REGISTER, trial.NATIVE_REGISTER),
            original.replace("\r\n", "\n"))
        self.assertEqual(patched.count(trial.LOG_TAG), 1)

    def test_no_stable_source_edit(self):
        from pathlib import Path
        stable = Path("renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp")
        self.assertTrue(stable.is_file(), "V272 stable materializer absent")
        # The controlled patcher cannot reference this path as an output.
        code = Path(trial.__file__).read_text(encoding="utf-8")
        self.assertNotIn(str(stable), code)
        self.assertIn("pmetal_rgba_lerp_materializer.cpp", code)

if __name__ == "__main__":
    unittest.main(verbosity=2)
