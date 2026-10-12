#!/usr/bin/env python3
"""Negative oracles: distinct nonmetal material domains never collapse to metal."""
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audit_v232_nonmetal_lerp_material_cut as v232


class LerpMaterialDomainContract(unittest.TestCase):
    def test_authored_domains_are_distinct(self):
        result = v232.census()
        self.assertEqual(result["exact_owner_rows"], 36)
        domains = result["material_domains"]
        for family, expected in v232.EXPECTED.items():
            self.assertEqual(domains[family]["slot"], expected["slot"])
            self.assertEqual(domains[family]["c101_rgb"], [expected["c101"]]*3)
            self.assertEqual(domains[family]["c102"], expected["c102"])
            self.assertEqual(domains[family]["exact_owner_rows"], 12)
        self.assertEqual(domains["P_DullLeather[DSB].mtd"]["raw_mtd_variants"], 2)
        self.assertEqual(domains["P_Leather[DSB].mtd"]["raw_mtd_variants"], 1)
        self.assertEqual(domains["P_Metal[DSB].mtd"]["raw_mtd_variants"], 1)
        self.assertEqual(len({(x["slot"],tuple(x["c101_rgb"]),x["c102"])
                              for x in domains.values()}), 3)
        self.assertEqual(result["PTDE_material_consumer_equivalence"], "OPEN")
        self.assertFalse(result["shader_or_addon_modified"])

    def test_wrong_nonmetal_slot_rejected(self):
        raw = v232.read(v232.ROUTER)
        changed = raw.replace(
            "envspec_router_state::present, 0u, false,",
            "envspec_router_state::present, 2u, false,",
            1,
        )
        # Mutate precisely the P_DullLeather record, not an arbitrary row.
        index = raw.index('"P_DullLeather[DSB].mtd"')
        end = raw.index('"P_Leather[DSB].mtd"', index)
        row = raw[index:end]
        row_mutated = row.replace(
            "envspec_router_state::present, 0u, false,",
            "envspec_router_state::present, 2u, false,", 1)
        self.assertNotEqual(row, row_mutated)
        with self.assertRaises(AssertionError):
            v232.parse_router(raw[:index]+row_mutated+raw[end:],
                                "P_DullLeather[DSB].mtd")

    def test_missing_or_duplicated_mtd_rejected(self):
        raw = v232.read(v232.ROUTER)
        marker = '"P_Metal[DSB].mtd"'
        with self.assertRaises(AssertionError):
            v232.parse_router(raw.replace(marker, '"BROKEN.mtd"'), "P_Metal[DSB].mtd")
        with self.assertRaises(AssertionError):
            v232.parse_router(raw + '\n"' + "P_Metal[DSB].mtd" + '",', "P_Metal[DSB].mtd")

    def test_exact_manifest_no_broadening(self):
        original = v232.read(v232.MANIFEST)
        rows = v232.parse_manifest(original)
        self.assertEqual(len(rows), 36)
        self.assertEqual(len({r[0] for r in rows}), 21)
        self.assertEqual(len({r[3].lower() for r in rows}), 18)
        with self.assertRaises(AssertionError):
            v232.parse_manifest(original.replace(rows[0][0], "0"*64, 1) +
                                '\n    { "' + "f"*64 + '", 0u, "P_Metal[DSB].mtd", "fake_s" },')

    def test_r19_operator_identity_guard(self):
        v232.assert_operator_cut(v232.read(v232.LERP),
                                 v232.read(v232.DRAW),
                                 v232.read(v232.GATE))
        lerp = v232.read(v232.LERP)
        with self.assertRaises(AssertionError):
            v232.assert_operator_cut(
                lerp.replace("workflow_material_hits != 1u",
                             "workflow_material_hits != 2u",1),
                v232.read(v232.DRAW), v232.read(v232.GATE))


if __name__ == "__main__":
    unittest.main(verbosity=2)
