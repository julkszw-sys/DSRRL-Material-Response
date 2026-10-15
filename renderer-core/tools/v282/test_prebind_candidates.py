#!/usr/bin/env python3
"""V282 non-runtime regression gates: no texture-only material authorization."""
import json
import tempfile
import unittest
from pathlib import Path
from compile_prebind_candidates import compile_index, inventory_index

ROOT = Path(__file__).resolve().parents[3]

class V282PrebindSourceTest(unittest.TestCase):
    def test_full_repository_corpus(self):
        plan = compile_index(ROOT, None)
        self.assertEqual(plan["counts"]["spec_name_records"], 769)
        self.assertEqual(plan["counts"]["diffuse_pairs"], 528)
        self.assertEqual(plan["counts"]["normal_triples"], 557)
        self.assertFalse(plan["activation_authorized"])
        self.assertFalse(plan["inputs"]["october10_inventory_supplied"])
        self.assertGreaterEqual(plan["counts"]["ambiguous_spec_name_routes"], 3)
        self.assertGreater(plan["counts"]["mtd_donors"], 30)

    def test_real_shared_spec_requires_full_tuple(self):
        plan = compile_index(ROOT, None)
        candidate = plan["texture_candidate_index"]["5e4fbb58eb8dc682"]
        self.assertEqual(candidate["logical_name"], "bd_m_body_m_s")
        self.assertEqual(len(candidate["diffuse"]), 3)
        self.assertEqual(len(candidate["normal"]), 3)
        self.assertFalse(plan["activation_authorized"])

    def test_inventory_validates_presence_not_activation(self):
        with tempfile.TemporaryDirectory() as d:
            manifest = Path(d) / "manifest.json"
            manifest.write_text(json.dumps({"entries":[
                {"path":"Specular/BD_M_body_M_s.dds", "sha256":"a"*64},
                {"path":"Diffuse/BD_M_body_M.dds", "sha256":"b"*64}
            ]}), encoding="utf-8")
            plan = compile_index(ROOT, manifest)
            self.assertEqual(plan["counts"]["inventory_entries"], 2)
            self.assertEqual(plan["counts"]["inventory_spec_matches"], 1)
            self.assertTrue(plan["texture_candidate_index"]["5e4fbb58eb8dc682"]["inventory_present"])
            self.assertFalse(plan["activation_authorized"])

    def test_inventory_refuses_missing_sha_or_bad_directory(self):
        with tempfile.TemporaryDirectory() as d:
            manifest = Path(d) / "manifest.json"
            for path, sha in [("Specular/a.dds", None),
                              ("WRONG/a.dds", "a"*64)]:
                manifest.write_text(json.dumps({"entries":[
                    {"path":path, "sha256":sha}]}), encoding="utf-8")
                with self.assertRaises(ValueError):
                    inventory_index(manifest)

    def test_conflicting_live_name_fails_closed(self):
        with tempfile.TemporaryDirectory() as d:
            manifest = Path(d) / "manifest.json"
            manifest.write_text(json.dumps({"entries":[
                {"path":"Specular/A.dds", "sha256":"a"*64},
                {"path":"Specular/a.dds", "sha256":"b"*64}]}), encoding="utf-8")
            with self.assertRaises(ValueError):
                inventory_index(manifest)

if __name__ == "__main__":
    unittest.main(verbosity=2)
