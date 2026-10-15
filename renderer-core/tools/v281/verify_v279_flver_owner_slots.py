#!/usr/bin/env python3
"""V281: exact DSR FLVER->material-slot->MTD verification against committed
19,985-tuple owner corpus. Read-only, not GPU/pixel validation.
Use: python renderer-core/tools/v281/verify_v279_flver_owner_slots.py
"""
from __future__ import annotations
import bisect
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "renderer-core" / "tools"))
from generate_dsr_flver_owner_tuple_header import load_payload

CORPUS = ROOT / "renderer-core/data/census/dsr_flver_owner_tuple_corpus_v1.bin.z"
TARGETS = (
    {
        "asset": "A",
        "sha": "eb3b77ee419f9e2997b7d427987c002411fdf08edda12834be4a9f51b3c4eb7b",
        "slots": ((2,"P_Leather[DSB].mtd"),(3,"P_Metal[DSB].mtd")),
    },
    {
        "asset": "B",
        "sha": "672e35606495f24e36c704f32fceda9b933563a64e03379375f47f9fe04a0a7d",
        "slots": ((8,"P_Leather[DSB].mtd"),(5,"P_Metal[DSB].mtd")),
    },
)

def fnv1a64(s:str)->int:
    h = 14695981039346656037
    for byte in s.encode("utf-8"):
        h = ((h ^ byte) * 1099511628211) & 0xffffffffffffffff
    return h

def audit()->dict:
    groups, mtd_hashes=load_payload(CORPUS)
    keys=[x[0] for x in groups]
    rows=[]
    for t in TARGETS:
        digest=bytes.fromhex(t["sha"])
        i=bisect.bisect_left(keys,digest)
        present=i<len(keys) and keys[i]==digest
        for slot,name in t["slots"]:
            expected=fnv1a64(name)
            if not present:
                state="FLVER_SHA_NOT_IN_COMMITTED_CORPUS"
                observed=None
            else:
                _,first,count=groups[i]
                if slot>=count:
                    state="SLOT_OUTSIDE_FLVER_MATERIAL_TABLE"
                    observed=None
                else:
                    observed=mtd_hashes[first+slot]
                    state=("OWNER_TUPLE_MATCH" if observed==expected
                           else "MTD_SEMANTIC_MISMATCH")
            rows.append(dict(asset=t["asset"],flver_sha256=t["sha"],
                         slot=slot,mtd=name,expected_fnv1a64=f"{expected:016x}",
                         observed_fnv1a64=None if observed is None else f"{observed:016x}",
                         corpus_flver_present=present,status=state))
    return dict(schema="dsrrl.v281.flver_owner_slot_audit.v1",
                evidence_scope="STATIC_REPOSITORY_CORPUS_ONLY",
                compressed_corpus_sha256=hashlib.sha256(CORPUS.read_bytes()).hexdigest(),
                total_flver_groups=len(groups),total_material_tuples=len(mtd_hashes),
                target_rows=rows,
                verified_owner_tuples=sum(x["status"]=="OWNER_TUPLE_MATCH" for x in rows),
                unresolved=sum(x["status"]=="FLVER_SHA_NOT_IN_COMMITTED_CORPUS" for x in rows),
                semantic_mismatches=sum(x["status"] in ("MTD_SEMANTIC_MISMATCH","SLOT_OUTSIDE_FLVER_MATERIAL_TABLE") for x in rows),
                actual_mesh_vertex_declaration="UNOBSERVED",
                actual_gpu_vs_ps_transport="UNOBSERVED",
                ptde_visible_pixels="UNOBSERVED",
                note="A match authenticates a tuple only; a miss is not proof of broken FLVER or identity producer.")

def main()->int:
    result=audit()
    print(json.dumps(result,indent=2,sort_keys=True))
    # A semantic mismatch on an exact present FLVER is meaningful and must fail.
    # Absent target FLVER is an audit result (OPEN), not build failure.
    return 1 if result["semantic_mismatches"] else 0

if __name__=="__main__":
    raise SystemExit(main())
