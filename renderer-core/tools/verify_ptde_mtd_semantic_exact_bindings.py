#!/usr/bin/env python3
import json
import pathlib

p = pathlib.Path(__file__).resolve().parents[1] / "data" / "census" / "ptde_mtd_semantic_exact_bindings_v1.json"
d = json.loads(p.read_text(encoding="utf-8"))
assert d["schema"] == 1
assert d["runtime_activation"] == "OPEN"
assert d["pixel_equivalence"] == "OPEN"
records = d["records"]
assert len(records) == 8
assert [r["bridge_binding_id"] for r in records] == list(range(40, 48))
seen = set()
for r in records:
    key = (r["mtd_name"], r["raw_sha256"])
    assert key not in seen
    seen.add(key)
    assert len(r["raw_sha256"]) == 64
    assert r["material_family"] == "DifSpcBmp"
    assert r["receiver_scope"] == [33, 34, 35]
    assert r["gate_policy"] in {"DIRECT_EXACT", "PTDE_COMPANION_REQUIRED"}
    assert r["operators"]["material_response"] == "USE"
    assert r["operators"]["hemenv"] == "USE"
    assert r["operators"]["pointlight"] == "NO_USE"
    for op in ("spec_rgb", "env_spec", "subsurface"):
        assert r["operators"][op] == "UNKNOWN"
print("ptde_mtd_semantic_exact_bindings_v1: PASS", len(records))
