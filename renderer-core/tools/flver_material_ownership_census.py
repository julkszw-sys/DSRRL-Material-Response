#!/usr/bin/env python3
"""Build the canonical row-level FLVER -> material slot -> MTD -> resource census.

Input is a deterministic TSV export from an owner-side FLVER parser. This tool
normalizes, validates, hashes and sorts rows so the checked-in payload is
source-complete and reproducible. It deliberately does not infer operator
absence from missing texture/resource names.
"""
from __future__ import annotations
import argparse, csv, hashlib, json, pathlib

REQ=("game","flver_identity","material_slot","mtd_name","mtd_sha256")
OPT=("spx_sha256","receiver_index","consumer_family","material_family","texture_semantic","resource_hash","srv_slot","sampler_slot")

def valid_sha(x:str)->bool:
    return len(x)==64 and all(c in "0123456789abcdefABCDEF" for c in x)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("input_tsv",type=pathlib.Path)
    ap.add_argument("output_jsonl",type=pathlib.Path)
    ap.add_argument("--summary",type=pathlib.Path,required=True)
    a=ap.parse_args()
    raw=a.input_tsv.read_bytes(); source_sha=hashlib.sha256(raw).hexdigest()
    rows=[]
    with a.input_tsv.open(encoding="utf-8",newline="") as f:
        rd=csv.DictReader(f,delimiter="\t")
        missing=[k for k in REQ if k not in (rd.fieldnames or [])]
        if missing: raise SystemExit("missing columns: "+", ".join(missing))
        for n,r in enumerate(rd,2):
            if not all((r.get(k) or "").strip() for k in REQ): raise SystemExit(f"row {n}: incomplete identity")
            if not valid_sha(r["mtd_sha256"]): raise SystemExit(f"row {n}: invalid mtd_sha256")
            try: slot=int(r["material_slot"])
            except ValueError: raise SystemExit(f"row {n}: invalid material_slot")
            if slot<0: raise SystemExit(f"row {n}: negative material_slot")
            o={k:(r.get(k) or "").strip() for k in REQ+OPT}
            o["material_slot"]=slot
            # Missing resource/operator evidence remains UNKNOWN by construction.
            o["resource_state"]="USE" if o["texture_semantic"] and o["resource_hash"] else "UNKNOWN"
            rows.append(o)
    rows.sort(key=lambda r:(r["game"],r["flver_identity"],r["material_slot"],r["mtd_sha256"],r["texture_semantic"],r["resource_hash"]))
    seen=set()
    for r in rows:
        k=(r["game"],r["flver_identity"],r["material_slot"],r["mtd_sha256"],r["texture_semantic"],r["resource_hash"])
        if k in seen: raise SystemExit("duplicate exact ownership/resource row: "+repr(k))
        seen.add(k)
    a.output_jsonl.parent.mkdir(parents=True,exist_ok=True)
    with a.output_jsonl.open("w",encoding="utf-8",newline="\n") as f:
        for r in rows: f.write(json.dumps(r,sort_keys=True,separators=(",",":"))+"\n")
    out_sha=hashlib.sha256(a.output_jsonl.read_bytes()).hexdigest()
    identities={(r["game"],r["flver_identity"],r["material_slot"],r["mtd_sha256"]) for r in rows}
    summary={"schema":1,"source_sha256":source_sha,"payload_sha256":out_sha,"rows":len(rows),"ownership_identities":len(identities),"flver_identities":len({(r["game"],r["flver_identity"]) for r in rows}),"policy":{"missing_resource":"UNKNOWN","no_use_from_absence":False,"runtime_activation":"OPEN","pixel_equivalence":"OPEN"}}
    a.summary.write_text(json.dumps(summary,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print(json.dumps(summary,sort_keys=True))
if __name__=="__main__": main()
