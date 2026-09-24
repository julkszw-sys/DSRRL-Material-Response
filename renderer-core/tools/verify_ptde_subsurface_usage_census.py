#!/usr/bin/env python3
import json, pathlib, sys

p = pathlib.Path(__file__).resolve().parents[1] / "data" / "census" / "ptde_subsurface_usage_census_v1.json"
d = json.loads(p.read_text(encoding="utf-8"))
assert d["schema"] == 1
assert set(d["classification"]) == {"USES_SUBSURFACE","NO_SUBSURFACE","UNKNOWN"}
assert d["default_record"]["classification"] == "UNKNOWN"
records=d["records"]
assert records
seen=set()
for r in records:
    key=(r["ptde_mtd_sha256"], r["ptde_shader_family"], tuple(r["receiver_scope"]))
    assert key not in seen
    seen.add(key)
    assert r["classification"] in d["classification"]
    if r["classification"]=="NO_SUBSURFACE":
        assert r["confidence"] in ("HIGH_CONFIDENCE","CONFIRMED")
        assert r["ptde_mtd_sha256"]
        assert r["receiver_scope"]
body=[r for r in records if r["key"]=="ptde.ps_body_dsb.stable_hemenv"]
assert len(body)==1
r=body[0]
assert r["classification"]=="NO_SUBSURFACE"
assert r["ptde_mtd_sha256"]=="af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95"
assert r["receiver_scope"]==[33,34,35]
print("ptde_subsurface_usage_census_v1: PASS")
