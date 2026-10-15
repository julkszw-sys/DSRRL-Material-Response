#!/usr/bin/env python3
"""Oct-10 exact 1295 Specular names vs V12 texture routes.

A full source NAME manifest, not 1295 DDS payload SHA values. No binding or
material authorization is created. Uses SHA-pinned recovered 20261010 names.
"""
import argparse
import json
import sys
from pathlib import Path
from compile_prebind_candidates import compile_index

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/"renderer-core/tools"))
from generate_sidecar_spec_whitelist_20261010 import load_names,fnv1a_ascii_lower

def join(root):
    oct10=load_names(root/"renderer-core/data/provenance/sidecar_specular_20261010_names.b85")
    v12=compile_index(root,None)
    table=v12["texture_candidate_index"]
    rows=[]
    for name in oct10:
        h=f"{fnv1a_ascii_lower(name):016x}"
        seeded=table.get(h)
        if seeded is not None and seeded["logical_name"]!=name:
            raise ValueError("logical hash maps to distinct resource name")
        rows.append({
            "spec_hash":h,"logical_name":name,
            "v12_named":seeded is not None,
            "v12_pair_count":len(seeded["diffuse"]) if seeded else 0,
            "v12_triple_count":len(seeded["normal"]) if seeded else 0,
            "physical_dds_sha256":None,
            "ready_for_runtime":False,
            "identity_source":"sha256_pinned_oct10_names_only"})
    lookup={x["spec_hash"] for x in rows}
    return dict(
       schema="dsrrl.v283.oct10_specular_name_crosswalk.v1",
       source_names_sha256="6f417110f6085ca8389213578eb9c0a86c100070717a9df20d62790052c151e4",
       counts={
         "oct10_specular_names":len(rows),
         "v12_name_records":v12["counts"]["spec_name_records"],
         "oct10_v12_overlap":sum(x["v12_named"] for x in rows),
         "oct10_not_in_v12":sum(not x["v12_named"] for x in rows),
         "v12_not_in_oct10":sum(k not in lookup for k in table),
         "oct10_with_v12_diffuse_pair":sum(x["v12_pair_count"]>0 for x in rows),
         "oct10_with_v12_normal_triple":sum(x["v12_triple_count"]>0 for x in rows),
       },
       records=rows,activation_authorized=False,
       limitations=[
          "original 20261010 2948-row per-DDS SHA256 inventory not present",
          "name corpus contains no actual DDS bytes or SRV liveness",
          "name does not determine FLVER MTD slot or shader equation",
          "V12 pairs need independent t0/t1/t2 and exact owner validation"
       ]
    )

if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("--root",type=Path,default=ROOT)
    ap.add_argument("--output",type=Path,required=True)
    args=ap.parse_args()
    obj=join(args.root)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(obj,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(obj["counts"],sort_keys=True))
    print("OCT10_1295_NAMES_SHA_VERIFIED__ATOMIC_RUNTIME_OFF")
