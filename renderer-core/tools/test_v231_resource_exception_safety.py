#!/usr/bin/env python3
"""Exact V230 source oracle plus V231 post-generator transform tests."""
from pathlib import Path
import subprocess
import unittest
import apply_v231_resource_exception_safety as patch

ROOT = Path(__file__).resolve().parents[2]
FILE = "renderer-core/src/runtime/material_resource_draw_runtime.cpp"
BASE = "a327fe47b75b0de6a8d82c19c6bee955b5047022"

class V231Safety(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.old = subprocess.check_output(
            ["git", "show", f"{BASE}:{FILE}"], cwd=ROOT
        ).decode("utf-8").replace("\r\n", "\n")
        cls.current = (ROOT / FILE).read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.output = patch.transform(cls.current)

    def test_unmodified_pre_generator_source(self):
        self.assertEqual(self.old, self.current)

    def test_only_two_replacements(self):
        restored = self.output
        for old, new in (
            (patch.RELEASE_OLD, patch.RELEASE_NEW),
            (patch.ADMISSION_OLD, patch.ADMISSION_NEW),
        ):
            self.assertEqual(restored.count(new), 1)
            restored = restored.replace(new, old, 1)
        self.assertEqual(restored, self.old)
        self.assertEqual(patch.transform(self.output), self.output)

    def test_no_allocation_during_device_teardown(self):
        self.assertIn("std::array<companion_set, k_release_batch>", patch.RELEASE_NEW)
        self.assertNotIn("push_back(", patch.RELEASE_NEW)
        self.assertIn("invalidate_all_companion_epochs();", patch.RELEASE_NEW)
        self.assertIn("release_set(dead[n]);", patch.RELEASE_NEW)

    def test_exception_safe_resource_admission(self):
        self.assertIn("} catch (...) {", patch.ADMISSION_NEW)
        self.assertIn("g_quarantined.store(", patch.ADMISSION_NEW)
        self.assertIn("invalidate_all_companion_epochs();", patch.ADMISSION_NEW)
        self.assertIn("release_set(old);", patch.ADMISSION_NEW)
        self.assertIn("release_set(set);", patch.ADMISSION_NEW)
        self.assertIn("had_old = true;\n                g_ambiguous_view_device.emplace(", patch.ADMISSION_NEW)

if __name__ == "__main__":
    unittest.main(verbosity=2)
