#!/usr/bin/env python3
import json
import pathlib

p = pathlib.Path(__file__).resolve().parents[1] / "data" / "census" / "ptde_mtd_homology_semantics_v1.json"
d = json.loads(p.read_text(encoding="utf-8"))

assert d["schema"] == 1
assert d["claim_scope"] == "CONSTRUCTION_AND_STATIC_ROUTING_ONLY"
assert d["runtime_activation"] == "OPEN"
assert d["pixel_equivalence"] == "OPEN"
assert d["paired_phn_records"] == 325
assert sum(d["classes"].values()) == 325
assert d["classes"] == {
    "HOMOLOGOUS_SPC": 213,
    "HOMOLOGOUS_NOSPC": 25,
    "NONHOMOLOGOUS_ADDED_SPC": 87,
    "NONHOMOLOGOUS_LOST_SPC": 0,
}
assert sum(d["added_spc_subclasses"].values()) == d["classes"]["NONHOMOLOGOUS_ADDED_SPC"]
assert d["added_spc_subclasses"]["SPC_INSERT_ONLY"] == 20
assert d["added_spc_subclasses"]["SPC_PLUS_OTHER_FAMILY_CHANGE"] == 67
assert d["raw_hashes"]["class_ambiguous_dsr_hashes"] == 7
assert d["raw_hashes"]["runtime_safe_paired_phn_unique_hashes"] == 302
s = d["semantic_classification"]
assert s["HOMOLOGOUS_SPC"]["specular_family"]["state"] == "USE"
assert s["HOMOLOGOUS_NOSPC"]["specular_family"]["state"] == "NO_USE"
assert s["NONHOMOLOGOUS_ADDED_SPC"]["ptde_specular_family"]["state"] == "NO_USE"
assert s["NONHOMOLOGOUS_ADDED_SPC"]["dsr_specular_family"]["state"] == "USE"
for cls in s.values():
    for value in cls.values():
        if isinstance(value, dict) and "state" in value:
            assert value["state"] in {"USE", "NO_USE", "UNKNOWN"}
print("ptde_mtd_homology_semantics_v1: PASS")
