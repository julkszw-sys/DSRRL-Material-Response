#!/usr/bin/env python3
import json
import pathlib

p = pathlib.Path(__file__).resolve().parents[1] / "data" / "census" / "ptde_mtd_semantic_census_v1.json"
d = json.loads(p.read_text(encoding="utf-8"))

assert d["schema"] == 1
assert set(d["states"]) == {"USE", "NO_USE", "UNKNOWN"}
assert d["runtime_activation"] == "OPEN"
assert d["pixel_equivalence"] == "OPEN"
assert d["cohorts"]
assert d["exact_overrides"]

allowed = set(d["states"])
for cohort in d["cohorts"]:
    for op, entry in cohort["classification"].items():
        assert entry["state"] in allowed, (cohort["key"], op)
        if entry["state"] != "UNKNOWN":
            assert entry["confidence"] in {"CONFIRMED", "HIGH_CONFIDENCE"}

seen = set()
for rec in d["exact_overrides"]:
    key = (rec["ptde_mtd_name"], rec["ptde_mtd_sha256"], tuple(rec["receiver_scope"]))
    assert key not in seen, key
    seen.add(key)
    assert len(rec["ptde_mtd_sha256"]) == 64
    for op, entry in rec["operators"].items():
        assert entry["state"] in allowed
        if entry["state"] != "UNKNOWN":
            assert entry["confidence"] in {"CONFIRMED", "HIGH_CONFIDENCE"}

body = [r for r in d["exact_overrides"] if r["ptde_mtd_name"] == "Ps_Body[DSB].mtd"]
assert len(body) == 1
assert body[0]["operators"]["subsurface"]["state"] == "NO_USE"

pmetal = [r for r in d["exact_overrides"] if r["ptde_mtd_name"] == "P_Metal[DSB].mtd"]
assert len(pmetal) == 1
assert pmetal[0]["operators"]["env_spec"]["state"] == "USE"

assert any(x.get("source_artifact_id") == 1322 and x.get("records") == 325 for x in d["pending_imports"])
print("ptde_mtd_semantic_census_v1: PASS")
