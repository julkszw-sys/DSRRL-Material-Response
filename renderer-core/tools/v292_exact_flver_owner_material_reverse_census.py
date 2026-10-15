#!/usr/bin/env python3
"""V292 exact full DSR FLVER owner/MTD reverse census (NO bridge admission).

Read the already SHA-pinned DSR 3944-FLVER / 19985-slot binary corpus, not an
arbitrary material whitelist. Join only *identical* semantic FNV hashes against
source MTD identities; list unmatched hashes instead of fabricating names.
Optional ReShade runtime join requires a live nonzero FLVER fingerprint and
the exact slot. A 16-digit log SHA prefix is diagnostic, not authorization.
"""
from __future__ import annotations
import argparse
import collections
import csv
import json
import re
from pathlib import Path
from generate_dsr_flver_owner_tuple_header import load_payload

ROOT = Path(__file__).resolve().parents[1]
ROUTER = ROOT / "include/dsrrl/operators/material_response/generated_envspec_router_v1.hpp"
ROUTES = ROOT / "include/dsrrl/operators/material_response/generated_routes_v1.hpp"
CORPUS = ROOT / "data/census/dsr_flver_owner_tuple_corpus_v1.bin.z"
KV = re.compile(r"(\w+)=([^\s]+)")
MTD_PATTERN = re.compile(r'"([^"\n]+\.mtd)"\s*,\s*0x([a-fA-F0-9]{16})ull')
ROUTE_PATTERN = re.compile(r'\{"[^"]+","([^"]+\.mtd)",\d+u,')

def fnv(name: str) -> int:
    h=14695981039346656037
    for byte in name.encode("utf-8"):
        h=((h^byte)*1099511628211)&0xFFFFFFFFFFFFFFFF
    return h

def names() -> dict[int, set[str]]:
    out=collections.defaultdict(set)
    for name,hexdigest in MTD_PATTERN.findall(ROUTER.read_text(encoding="utf-8")):
        h=int(hexdigest,16)
        if fnv(name)!=h:
            raise ValueError(f"MTD semantic SHA mismatch: {name}")
        out[h].add(name)
    for name in ROUTE_PATTERN.findall(ROUTES.read_text(encoding="utf-8")):
        out[fnv(name)].add(name)
    if len(out)<300:
        raise ValueError("Source MTD router unexpectedly incomplete")
    return out

def source_rows():
    id_to_name=names()
    groups, hashes=load_payload(CORPUS) # checks all SHA and exact 19985 rows
    records=[]
    for full_sha, first, count in groups:
        digest=full_sha.hex()
        for slot in range(count):
            semantic=hashes[first+slot]
            candidates=id_to_name.get(semantic,set())
            name=next(iter(candidates)) if len(candidates)==1 else ""
            records.append({
                "flver_sha256":digest,
                "material_slot":slot,
                "mtd_semantic_fnv64":f"{semantic:016x}",
                "mtd_basename":name,
                "semantic_mapping_state":"EXACT_UNIQUE_SOURCE_NAME" if name else
                    "AMBIGUOUS_SOURCE_NAME" if candidates else "UNKNOWN_SOURCE_NAME"
            })
    if len(records)!=19985:
        raise ValueError(f"Full owner corpus count drift: {len(records)}")
    return records

def runtime_rows(logs, rows):
    indexed=collections.defaultdict(list)
    for r in rows:
        indexed[(r["flver_sha256"][:16],str(r["material_slot"]))].append(r)
    out=[]
    for log in logs:
        for line in log.read_text(encoding="utf-8",errors="replace").splitlines():
            if "[DSRRL V290 SPEC DRAW CENSUS]" not in line:continue
            event=dict(KV.findall(line))
            observed_flver=event.get("flver","")
            slot=event.get("slot","")
            cand=indexed.get((observed_flver,slot),[]) if observed_flver!="0000000000000000" else []
            if len(cand)==1:
                mapping=cand[0]
                state="SOURCE_PREFIX_SLOT_MATCH_DIAGNOSTIC_ONLY"
            elif observed_flver=="0000000000000000":
                mapping={}
                state="RUNTIME_FLVER_OWNER_MISSING"
            else:
                mapping={}
                state="MISSING_OR_AMBIGUOUS_SOURCE_MATCH"
            out.append({
                "log":log.name,
                "rx":event.get("rx",""),
                "family":event.get("family",""),
                "route":event.get("route",""),
                "slot":slot,
                "runtime_flver_prefix":observed_flver,
                "runtime_raw_mtd_prefix":event.get("mtd",""),
                "runtime_cut":event.get("cut",""),
                "runtime_owner":event.get("owner",""),
                "map_state":state,
                "mapped_full_flver_sha":mapping.get("flver_sha256",""),
                "mapped_mtd_basename":mapping.get("mtd_basename",""),
                "mapped_mtd_fnv64":mapping.get("mtd_semantic_fnv64",""),
                "ptde_consumer_status":"NOT_CERTIFIED_BY_THIS_CENSUS",
                "pixel_behavior":"OPEN"
            })
    return out

def write_csv(path,records,keys):
    with path.open("w",encoding="utf-8",newline="") as fp:
        writer=csv.DictWriter(fp,fieldnames=keys)
        writer.writeheader()
        writer.writerows(records)

def run(output: Path, logs: list[Path]):
    output.mkdir(parents=True,exist_ok=True)
    source=source_rows()
    write_csv(output/"dsr_all_19985_owner_slots.csv",source,list(source[0]))
    runtime=runtime_rows(logs,source)
    write_csv(output/"runtime_owner_join.csv",runtime,[
        "log","rx","family","route","slot","runtime_flver_prefix",
        "runtime_raw_mtd_prefix","runtime_cut","runtime_owner","map_state",
        "mapped_full_flver_sha","mapped_mtd_basename","mapped_mtd_fnv64",
        "ptde_consumer_status","pixel_behavior"])
    cloth=[r for r in source if "roughcloth" in r["mtd_basename"].lower()]
    write_csv(output/"exact_roughcloth_owner_slots.csv",cloth,list(source[0]))
    summary={
        "source_flver_groups":3944,
        "source_owner_rows":len(source),
        "source_rows_named_by_exact_mtd_semantic":sum(bool(r["mtd_basename"]) for r in source),
        "source_rows_unknown_semantic":sum(not r["mtd_basename"] for r in source),
        "source_roughcloth_slots":len(cloth),
        "source_roughcloth_flvers":len({r["flver_sha256"] for r in cloth}),
        "runtime_observations":len(runtime),
        "runtime_owner_missing":sum(r["map_state"]=="RUNTIME_FLVER_OWNER_MISSING" for r in runtime),
        "runtime_source_prefix_slot_matched":sum(r["map_state"]=="SOURCE_PREFIX_SLOT_MATCH_DIAGNOSTIC_ONLY" for r in runtime),
        "full_corpus_authority":"STATIC_SHA_PINNED_FLVER_SLOT_MTD_HASH_ONLY",
        "runtime_allowlist_extended":False,
        "ptde_shader_consumer_equivalence":"OPEN",
        "pixel_equivalence":"OPEN"
    }
    (output/"summary.json").write_text(json.dumps(summary,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(summary,sort_keys=True))
    return summary

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--out",type=Path,required=True)
    ap.add_argument("--log",type=Path,action="append",default=[])
    args=ap.parse_args()
    run(args.out,args.log)

if __name__=="__main__":main()
