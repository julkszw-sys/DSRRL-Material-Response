#!/usr/bin/env python3
"""Keep Source1 texture name classification exact and fail-open."""
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from audit_v230_source1_exact_asset import STOCK,audit,fnv_identity

class Source1AssetTests(unittest.TestCase):
    def test_names_are_exact_and_missing(self):
        with tempfile.TemporaryDirectory() as root:
            report=audit(root)
            self.assertEqual([r["dds_exists"] for r in report["resources"]],[False]*3)
            self.assertTrue(all(not r["allowed_to_replace"] for r in report["resources"]))
            for _,name,digest in STOCK:
                self.assertEqual(fnv_identity(name),digest)

    def test_equipment_alias_is_not_source1(self):
        with tempfile.TemporaryDirectory() as root:
            d=Path(root)/"DSRRL"/"Specular"
            d.mkdir(parents=True)
            (d/"wp_a_0200_s.dds").write_bytes(b"DDS "+bytes(124))
            self.assertFalse(audit(root)["resources"][1]["dds_exists"])

    def test_present_uncertified_dds_cannot_authorize(self):
        with tempfile.TemporaryDirectory() as root:
            d=Path(root)/"DSRRL"/"Specular"
            d.mkdir(parents=True)
            (d/"c2500_WP_A_0200_s.dds").write_bytes(b"DDS "+bytes(124))
            spec=audit(root)["resources"][1]
            self.assertTrue(spec["dds_exists"])
            self.assertTrue(spec["dds_magic_present"])
            self.assertFalse(spec["ptde_origin_certified"])
            self.assertFalse(spec["allowed_to_replace"])

if __name__=="__main__":
    unittest.main()
