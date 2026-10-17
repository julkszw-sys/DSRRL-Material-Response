#!/usr/bin/env python3
"""Inventory forced stock/fail-open decisions reachable in renderer source.

Includes *all* executable return-false, disabled/stock, and quarantine
text occurrences. This is a CANDIDATE census, not a CFG proof of all
executed branches or a license to disable fail-open protections.
"""
from pathlib import Path
from collections import Counter
import argparse
import json
import re

def subsystem(path):
    s=str(path).lower()
    for name,parts in {
        "lightbank":("pmetal_env_source","lightbank","upper_lower","hemdir3"),
        "envspec":("envspec","env_spec","cubemap","probe"),
        "material":("material_owner","material_response","material_selector","mtd"),
        "resource_sidecar":("material_resource","texture_identity","resource_bridge","spec_rgb"),
        "receiver_shader":("pipeline_registry","shader","materialize"),
        "pointlight":("pointlight","clustered_pnts","local_specular"),
        "draw_state":("draw_transaction","island_draw","pixel_srv_shadow"),
        "subsurface":("subsurface",),
        "postprocess":("bloom","motion_blur","tonemap"),
    }.items():
        if any(part in s for part in parts):return name
    return "shared_core"

root=Path(__file__).resolve().parents[1]
source_paths=[root/"integrated/integrated_addon.cpp"]
for folder in ("src","include"):
    source_paths.extend((root/folder).rglob("*.cpp"))
    source_paths.extend((root/folder).rglob("*.hpp"))

needles=re.compile(r"return false\s*;|fail[_-]?open|stock[_-]?dsr|native_draw|source_reject|material_reject|probe_reject|quarantin|restore_fail|DSRRL_.*_BYPASS",re.I)
items=[]
for path in sorted(set(source_paths)):
    if not path.is_file() or "/generated_" in str(path) or path.name.startswith("generated_"):continue
    lines=path.read_text(encoding="utf-8",errors="replace").splitlines()
    for i,line in enumerate(lines):
        if not needles.search(line):continue
        if line.lstrip().startswith("//") or line.lstrip().startswith("*"):continue
        lo=max(0,i-6)
        items.append({
            "source":str(path.relative_to(root.parent)),
            "line":i+1,
            "subsystem":subsystem(path),
            "matched":line.strip()[:230],
            "preceding":' '.join(x.strip() for x in lines[lo:i+1])[-650:],
            "classification":"REVIEW_REQUIRED",
        })

groups=dict(sorted(Counter(x["subsystem"] for x in items).items()))
result={
    "version":"v351-20261017",
    "purpose":"source reachability review, NOT proof of runtime fallbacks",
    "all_return_false_and_stock_text_hits":len(items),
    "by_subsystem":groups,
    "known_live_release_blockers":[
        "V296 stock material fallback on exact Oct10 manifests (observed in RC3)",
        "V304 typed DSR-only LightBank atomic stock draw (observed RC3, V351 branch bypasses only for valid typed donors)",
        "V350 P_Metal materialize fail_diffuse_base result=4",
        "RX35 EnvSpec probe_reject",
        "incomplete MTD route/owner binding and source producer cache lifetime",
        "D3D11 bind/issue/restore fail-open/quarantine path",
    ],
    "release_status":"BLOCKED: construction and single-point runtime are not material/pixel proof",
    "candidates":items,
}
ap=argparse.ArgumentParser()
ap.add_argument("--output",default="v351_fallback_census.json")
args=ap.parse_args()
Path(args.output).write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding="utf-8")
assert result["all_return_false_and_stock_text_hits"]>200
assert "v304_typed_dsr_only_native_draw" in (root/"tools/materialize_v304_typed_dsr_only_donors.py").read_text(encoding="utf-8")
assert "stock_material_fail_open" in (root/"include/dsrrl/runtime/v296_oct10_material_atomic_gate.hpp").read_text(encoding="utf-8")
print("V351_CENSUS_PASS",len(items),groups," RELEASE_BLOCKED")
