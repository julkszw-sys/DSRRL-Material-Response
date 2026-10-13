#!/usr/bin/env python3
"""V258: intersect checksummed equipment SpecRGB logical names and route census.

Name-level candidates only. Never a shader/material whitelist or live gate.
Both input corpora are checksum-validated by the original generators.
"""
from pathlib import Path
import json
import sys
import importlib.util

ROOT=Path(__file__).resolve().parents[2]
TOOL=ROOT/"tools"
def load_module(path,name):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def run():
    names_mod=load_module(TOOL/"generate_sidecar_spec_whitelist_20261010.py","v258_names")
    routes_mod=load_module(TOOL/"generate_spec_routes_v12.py","v258_routes")
    names=names_mod.load_names(ROOT/"data/provenance/sidecar_specular_20261010_names.b85")
    _,equipment=routes_mod.load(ROOT/"data/provenance/spec_routes_v12.tsv")
    by_hash={names_mod.fnv1a_ascii_lower(n):n for n in names}
    pinned_equipment=set(equipment)
    supported=sorted(pinned_equipment & by_hash.keys())
    unmapped_routes=sorted(pinned_equipment - by_hash.keys())
    # Resource-authority candidate only: no guessed MTD or FLVER.
    records=[{
        "spec_logical_name":by_hash[h],
        "logical_name_fnv64":f"{h:016x}",
        "asset_present_on_owner_disk":None,
        "physical_sidecar_sha256":None,
        "mtd_sha256":None,"flver_sha256":None,
        "shader_sha256":None,"material_slot":None,
        "status":"NAME_ONLY_NOT_MATERIAL_AUTHORIZED",
        "runtime_allow":False
    } for h in supported]
    out={"schema":"dsrrl.v258.equipment_spec_name_candidates.v1",
        "sidecar_name_source_sha256":names_mod.EXPECTED_NAMES_SHA,
        "route_source_sha256":routes_mod.EXPECTED_CANONICAL_DATA_SHA256,
        "total_pinned_sidecar_names":len(names),
        "total_pinned_equipment_route_names":len(pinned_equipment),
        "candidate_name_matches":len(supported),
        "unmatched_equipment_route_hashes":[f"{x:016x}" for x in unmapped_routes],
        "material_or_shader_eligibility":"UNVERIFIED",
        "records":records}
    dest=ROOT/"../v258_sidecar_candidate_census.json"
    dest.write_text(json.dumps(out,sort_keys=True,indent=2)+"\n",encoding="utf8")
    assert len(names)==1295 and len(pinned_equipment)==748
    assert all(not x["runtime_allow"] for x in records)
    print(f"V258_AUTHENTIC_EQUIPMENT_SIDECAR_NAME_CENSUS_PASS matches={len(supported)}/748 unmatched={len(unmapped_routes)}")
    print("V258_ASSET_SHA_MTD_FLVER_SHADER_RECEIVER_GATE=UNPROVEN")
if __name__=="__main__":
    run()
