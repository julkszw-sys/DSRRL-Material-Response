#!/usr/bin/env python3
"""V283 exact raw-FLVER SHA+slot to raw-MTD donor join, construction only.

Uses the SHA-pinned complete 3944 owner / 19985 slot binary from the main
repository. This is **NOT** texture identity, shader activation or pixel proof.
"""
import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path
from compile_prebind_candidates import constants_from_header
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "renderer-core/tools"))
from generate_dsr_flver_owner_tuple_header import load_payload

def fnv64(text):
    result = 14695981039346656037
    for b in text.encode("utf-8"):
        result = ((result ^ b) * 1099511628211) & 0xffffffffffffffff
    return result

def compile_owner_join(root):
    groups, mtd_hashes = load_payload(root / "renderer-core/data/census/dsr_flver_owner_tuple_corpus_v1.bin.z")
    donors = json.loads((root / "renderer-core/data/material_response_routes_v1.json").read_text(encoding="utf-8"))["routes"]
    power = constants_from_header(root / "renderer-core/include/dsrrl/operators/material_response/generated_material_constants_v1.hpp")
    by_mtd = defaultdict(list)
    for x in donors:
        by_mtd[fnv64(x["mtd_name"])].append(x)

    # Do not disambiguate a shared basename hash with guesses: candidates
    # remain distinct unless all present semantic parameters agree exactly.
    entries = []
    mtd_ambiguous = []
    for flver, first, count in groups:
        for slot in range(count):
            mtd_hash = mtd_hashes[first + slot]
            cand = by_mtd.get(mtd_hash, [])
            if not cand:
                continue
            if len(cand) > 1 and len({(x["sha256"], x["c101"], x["route_index"],
                                         tuple(x["receiver_triplet"])) for x in cand}) > 1:
                mtd_ambiguous.append(dict(flver_sha256=flver.hex(),
                                          material_slot=slot,
                                          mtd_name_hash=f"{mtd_hash:016x}",
                                          matches=len(cand)))
                continue
            x = cand[0]
            const = power[x["route_index"]]
            entries.append(dict(flver_sha256=flver.hex(), material_slot=slot,
                                mtd_name=x["mtd_name"], mtd_name_hash=f"{mtd_hash:016x}",
                                raw_mtd_sha256=x["sha256"], route_index=x["route_index"],
                                receiver_ids=x["receiver_triplet"], c100=const["c100"],
                                c101=x["c101"], ptde_specular_power=const["ptde_specular_power"],
                                material_family=x["material_family"], lod_min=x["lod_min"],
                                lod_max=x["lod_max"], status="CANDIDATE_NOT_GPU_AUTHORIZED"))
    # DSR 19,985 owner tuples include models with same names but different
    # texture/receiver and non-homologous PTDE assets. They are metadata only.
    return {"schema":"dsrrl.v283.owner_material_candidates.v1",
            "status":"SOURCE_ONLY_NO_SHADER_NO_GPU_NO_PIXEL",
            "counts":{"flver_groups":len(groups),"owner_slots":len(mtd_hashes),
                      "material_candidate_slots":len(entries),
                      "ambiguous_mtd_hashes":len(mtd_ambiguous)},
            "entries":entries, "ambiguous_materials":mtd_ambiguous,
            "activation_authorized":False,
            "requires":["live model generation","live exact FLVER sha+slot",
                        "actual raw-MTD SHA", "exact t0/t1/t2 native resource identity",
                        "certified receiver ABI and PTDE operator island",
                        "authoritative sidecar DDS bytes + SRV and lifetime",
                        "actual PTDE producer and beta"]}

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--root",type=Path,default=ROOT)
    ap.add_argument("--output",type=Path,required=True)
    args=ap.parse_args()
    result=compile_owner_join(args.root)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(result["counts"],sort_keys=True))
    print("ATOM_NONMETAL_BIND=OFF")

if __name__=="__main__":
    main()
