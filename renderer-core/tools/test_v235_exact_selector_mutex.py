#!/usr/bin/env python3
"""Source contracts for a single-function R43 exact-key synchronization ablation."""
import pathlib
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
from apply_v235_exact_selector_mutex import (
    PATH, OLD_SIGNATURE, OLD_CONSUMER, NEW_SIGNATURE, NEW_CONSUMER, audit
)

ROOT = HERE.parents[1]
class V235ExactSource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.stock = (ROOT/PATH).read_text(encoding="utf-8")

    def test_production_source_remains_original(self):
        self.assertIn(OLD_SIGNATURE,self.stock)
        self.assertIn(OLD_CONSUMER,self.stock)
        self.assertNotIn(NEW_SIGNATURE,self.stock)
        self.assertNotIn(NEW_CONSUMER,self.stock)
        self.assertEqual(audit(self.stock,False),self.stock)

    def test_narrow_two_site_change_and_idempotence(self):
        after = audit(self.stock,True)
        self.assertEqual(after,self.stock.replace(OLD_SIGNATURE,NEW_SIGNATURE)
                                           .replace(OLD_CONSUMER,NEW_CONSUMER))
        self.assertEqual(audit(after,True),after)
        self.assertEqual(audit(after,False),after)

    def test_producer_trylock_and_old_payload_fallback_preserved(self):
        after = audit(self.stock,True)
        self.assertIn("} else if (!lock.try_lock())",after)
        self.assertIn("if (hook_producer_cache_lookup(\n            selector_identity,",after)
        self.assertIn("if (!hook_producer_cache_lookup(\n            query,\n            cached,\n            nullptr,\n            true))",after)
        self.assertIn("g_hook_source_mutex",after)
        self.assertIn("same_hook_source_payload(",after)
        self.assertIn("same_hook_selector_identity(",after)
        self.assertIn("void clear_hook_source() noexcept",after)

    def test_broadening_fails_closed(self):
        with self.assertRaises(ValueError):
            audit(self.stock.replace(OLD_CONSUMER,OLD_CONSUMER + OLD_CONSUMER),True)
        with self.assertRaises(ValueError):
            audit(self.stock.replace(OLD_SIGNATURE,OLD_SIGNATURE + OLD_SIGNATURE),True)

    def test_single_addon_wrapper_recovers_source(self):
        wrapper = (HERE/"build_v235_exact_selector_mutex.ps1").read_text()
        self.assertIn("finally",wrapper)
        self.assertIn("V235 failed to restore generated renderer source",wrapper)
        self.assertIn("build_v233_transition_stutter_diagnostic.ps1",wrapper)
        self.assertIn("verify_v203_clean_equipment.ps1",wrapper)
        self.assertIn("DSRRL_V235_EXACT_SELECTOR_MUTEX_DIAGNOSTIC.addon64",wrapper)

if __name__ == "__main__":
    unittest.main(verbosity=2)
