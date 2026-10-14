#!/usr/bin/env python3
"""Negative oracles for V272 Lerp-only stock SpecTex.a postmerge control."""
import unittest
from pathlib import Path

import apply_lerp_stock_alpha_gate as trial


class LerpOnlyStockAlpha(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.stock = trial.TARGET.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_only_r19_lerp_body_changes(self):
        patched = trial.patch(self.stock)
        self.assertEqual(patched.count(trial.TAG), 1)
        start = self.stock.index(trial.START)
        end = self.stock.index(trial.STOP)
        self.assertEqual(self.stock[:start], patched[:start])
        self.assertEqual(self.stock[end:], patched[patched.index(trial.STOP):])
        self.assertIn("words[additive_gate_word + 4u] = 0x00000000u;", patched)
        self.assertIn("words[workflow_mad_word + 4u] = 0x00000000u;", patched)
        self.assertNotIn(trial.OLD_GATE, patched[start:patched.index(trial.STOP)])
        self.assertIn(trial.NEW_GATE, patched)

    def test_idempotent(self):
        patched = trial.patch(self.stock)
        self.assertEqual(trial.patch(patched), patched)

    def test_preimage_drift_fails_closed(self):
        damaged = self.stock.replace(
            trial.OLD_GATE,
            trial.OLD_GATE.replace("0x3f800000u", "0x00000000u"),
            1,
        )
        self.assertNotEqual(damaged, self.stock)
        with self.assertRaises(AssertionError):
            trial.patch(damaged)

    def test_reject_duplicate_operator(self):
        corrupted = self.stock + trial.OLD_GATE
        # An unrelated occurrence after the function does not grant more
        # Lerp operator authority and must be completely untouched.
        patched = trial.patch(corrupted)
        self.assertTrue(patched.endswith(trial.OLD_GATE))

    def test_documented_stock_alpha_postcondition(self):
        patched = trial.patch(self.stock)
        self.assertIn("v272_lerp_stock_coeff_register != *spec_sample_register", patched)
        self.assertIn("state, post_coeff_word, v272_lerp_stock_coeff_word", patched)
        self.assertIn("state, post_coeff_word + 1u,", patched)


if __name__ == "__main__":
    unittest.main(verbosity=2)
