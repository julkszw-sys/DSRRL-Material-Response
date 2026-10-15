#!/usr/bin/env python3
"""V290 observed sidecar-to-draw funnel from ONE owner ReShade log.

Input is an empirical scene sample, never global coverage of 1295 assets
and never proof of PTDE pixel equivalence. No production renderer mutation.
"""
from __future__ import annotations
import argparse
import csv
from collections import Counter
import json
from pathlib import Path
import re
from generate_sidecar_spec_whitelist_20261010 import (
    fnv1a_ascii_lower,load_names
)

RE_CENSUS=re.compile(r"\[DSRRL V290 SPEC DRAW CENSUS\] (.+)$")
RE_DRAW=re.compile(r"\[DSRRL V287 MAT TRACE\] (.+)$")
KV=re.compile(r"(\w+)=([^\s]+)")
DRAW_STAGES=("atomic_indexed_draw_issued","atomic_replay_draw_issued","atomic_native_draw_armed","atomic_indexed_native_armed")
KEYS=("rx","family","route","slot","mtd","flver")


def parse(path:Path, sidecar:Path) -> tuple[dict,list[dict]]:
    names=load_names(sidecar)
    lookup={f"{fnv1a_ascii_lower(n):016x}":n for n in names}
    first={}
    draws=set()
    log=path.read_text(encoding="utf-8",errors="replace").splitlines()
    for line in log:
        m=RE_CENSUS.search(line)
        if m:
            d=dict(KV.findall(m.group(1)))
            key=tuple(d.get(k,"") for k in KEYS)
            if key not in first:first[key]=d
            continue
        m=RE_UNROUTED.search(line)
        if m:
            d=dict(KV.findall(m.group(1)))
            d.update({"route":"UNROUTED","slot":"0","mtd":"UNKNOWN",
                      "flver":"UNKNOWN_"+d.get("spec","0"),
                      "rx":"0","family":"2","cut":"NO_REGISTERED_SHADER_FAMILY"})
            key=tuple(d.get(k,"") for k in KEYS)
            if key not in first:first[key]=d
            continue
        m=RE_DRAW.search(line)
        if m:
            d=dict(KV.findall(m.group(1)))
            if d.get("stage") in DRAW_STAGES:
                draws.add(tuple(d.get(k,"") for k in KEYS))
    cuts=Counter()
    manifest_seen=set()
    ready_seen=set()
    draw_seen=set()
    rows=[]
    for key,d in sorted(first.items()):
        h=d.get("spec","0").lower()
        name=lookup.get(h,"")
        is_manifest=bool(name)
        if is_manifest:manifest_seen.add(h)
        if is_manifest and d.get("file")=="1":ready_seen.add(h)
        issued=key in draws
        if is_manifest and issued:draw_seen.add(h)
        cut=d.get("cut","UNKNOWN")
        cuts[cut]+=1
        rows.append({**{k:d.get(k,"") for k in KEYS},
                     "spec_hash":h,
                     "sidecar_name":name,
                     "in_oct10_manifest":int(is_manifest),
                     "file_ready":d.get("file","0"),
                     "first_observed_cut":cut,
                     "draw_witness_for_tuple":int(issued)})
    summary={
        "unrouted_stock_t1_names_observed":len({r["spec_hash"] for r in rows if r["route"]=="UNROUTED"}),
        "scope":"OBSERVED_DRAW_SCOPE_ONLY",
        "sidecar_spec_names_total":len(names),
        "unique_manifest_names_seen_bound_t1":len(manifest_seen),
        "unique_manifest_names_physically_ready_in_sample":len(ready_seen),
        "unique_manifest_names_with_draw_witness_in_sample":len(draw_seen),
        "observed_first_draw_tuples":len(first),
        "first_observed_cut_counts":dict(cuts),
        "not_seen_is_not_absent":True,
        "pixel_equivalence":"NOT_ASSESSED"
    }
    return summary,rows


def main() -> None:
    parser=argparse.ArgumentParser()
    parser.add_argument("--log",type=Path,required=True)
    parser.add_argument("--sidecar",type=Path,
        default=Path(__file__).resolve().parents[1]/"data/provenance/sidecar_specular_20261010_names.b85")
    parser.add_argument("--output-dir",type=Path,required=True)
    args=parser.parse_args()
    summary,rows=parse(args.log,args.sidecar)
    args.output_dir.mkdir(parents=True,exist_ok=True)
    (args.output_dir/"v290_sidecar_draw_summary.json").write_text(
        json.dumps(summary,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    columns=list(rows[0]) if rows else [*KEYS,"spec_hash","sidecar_name",
        "in_oct10_manifest","file_ready","first_observed_cut","draw_witness_for_tuple"]
    with (args.output_dir/"v290_sidecar_draw_rows.csv").open(
        "w",newline="",encoding="utf-8") as f:
        writer=csv.DictWriter(f,fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)
    print(json.dumps(summary,sort_keys=True))


if __name__=="__main__":
    main()
