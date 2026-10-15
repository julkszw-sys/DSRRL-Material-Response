#!/usr/bin/env python3
"""V291: source-known MTD variants and 1295 sidecars against owner log samples.
Rows not visited in-game are NOT diagnoses of failure. Never authorize a shader.
"""
import argparse,csv,json,re,sys
from pathlib import Path
from collections import defaultdict
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
from generate_sidecar_spec_whitelist_20261010 import load_names,fnv1a_ascii_lower
PAT=re.compile(r'"([^"\n]+\.mtd)",[^\n]*\n\s*\{\{(.*?)\}\},\s*envspec_router_state::(\w+),\s*(\d+)u',re.S)
KV=re.compile(r'(\w+)=([^\s]+)')
KINDS=("DSRRL V290 SPEC DRAW CENSUS","DSRRL V287 MAT TRACE","DSRRL V291 MATERIAL STAGE")
ISSUED=("atomic_indexed_draw_issued","atomic_replay_draw_issued","atomic_indexed_native_armed","atomic_native_draw_armed","indexed_issued","draw_issued","indexed_native_armed","native_draw_armed")
def write(path,rows):
    with path.open("w",newline="",encoding="utf-8") as f:
        if rows:
            writer=csv.DictWriter(f,fieldnames=list(rows[0]))
            writer.writeheader();writer.writerows(rows)
        else:f.write("")
def run(logs,output):
    raw=(ROOT/"include/dsrrl/operators/material_response/generated_envspec_router_v1.hpp").read_text()
    materials=[]
    for n,d,state,slot in PAT.findall(raw):
        b=re.findall(r"0x([0-9a-f]{2})u",d,re.I)
        if len(b)!=32:continue
        sha="".join(b)
        materials.append(dict(name=n,raw_mtd_sha256=sha,sha_prefix=sha[:16],
                              envspec_state=state,envspec_slot=slot))
    if len(materials)!=325:raise ValueError("MTD source router count mismatch: "+str(len(materials)))
    names=load_names(ROOT/"data/provenance/sidecar_specular_20261010_names.b85")
    spec_by_hash={f"{fnv1a_ascii_lower(n):016x}":n for n in names}
    by_mtd=defaultdict(list);by_spec=defaultdict(list)
    material_seen={}
    for log in logs:
        for line in log.read_text(encoding="utf-8",errors="replace").splitlines():
            if not any(k in line for k in KINDS):continue
            d=dict(KV.findall(line))
            if "mtd" not in d or "flver" not in d:continue
            d["log"]=log.name
            d["stage"]=d.get("stage","")
            d["cut"]=d.get("cut","")
            key=(log.name,)+tuple(d.get(x,"") for x in ("rx","family","route","slot","mtd","flver"))
            rec=material_seen.setdefault(key,{"events":set(),"cuts":set(),"specs":set(),"log":log.name})
            if d["stage"]:rec["events"].add(d["stage"])
            if d["cut"]:rec["cuts"].add(d["cut"])
            if d.get("spec") in spec_by_hash:
                rec["specs"].add(d["spec"])
    for key,rec in material_seen.items():
        rec["key"]=key
        by_mtd[key[5]].append(rec)
        for spec in rec["specs"]:by_spec[spec].append(rec)
    result=[]
    known={m["sha_prefix"] for m in materials}
    for m in materials:
        rows=by_mtd.get(m["sha_prefix"],[])
        events=set().union(*(r["events"] for r in rows)) if rows else set()
        cuts=set().union(*(r["cuts"] for r in rows)) if rows else set()
        result.append({**m,"observed_tuples":len(rows),
          "issued_tuples":sum(bool(r["events"].intersection(ISSUED)) for r in rows),
          "observed_events":"|".join(sorted(events)),
          "first_cuts":"|".join(sorted(cuts)),
          "status":"NOT_SEEN_IN_LOG" if not rows else
            "DRAW_ISSUED_SAMPLED" if events.intersection(ISSUED) else "DRAW_ISSUANCE_UNPROVEN"})
    spec_rows=[]
    for h,n in sorted(spec_by_hash.items(),key=lambda z:z[1]):
        rows=by_spec.get(h,[])
        spec_rows.append(dict(sidecar_name=n,logical_hash=h,observed_tuples=len(rows),
          issued_tuples=sum(bool(r["events"].intersection(ISSUED)) for r in rows),
          state="NOT_SEEN_IN_LOG" if not rows else "OBSERVED_RESOURCE_NOT_PIXEL_PROOF"))
    unknown=[]
    for key,r in material_seen.items():
        if key[5] not in known:
            unknown.append(dict(log=key[0],rx=key[1],family=key[2],route=key[3],
              slot=key[4],mtd_prefix=key[5],flver_prefix=key[6],
              events="|".join(sorted(r["events"])),cuts="|".join(sorted(r["cuts"]))))
    output.mkdir(parents=True,exist_ok=True)
    write(output/"all_source_mtd.csv",result)
    write(output/"all_1295_sidecars.csv",spec_rows)
    write(output/"unmapped_runtime_mtd.csv",unknown)
    summary=dict(source_router_variants=len(materials),source_unique_names=len(set(m["name"] for m in materials)),
      source_seen_variants=sum(bool(m["observed_tuples"]) for m in result),
      source_not_seen_variants=sum(not m["observed_tuples"] for m in result),
      sidecar_names=len(spec_rows),sidecar_seen_names=sum(bool(s["observed_tuples"]) for s in spec_rows),
      unmapped_runtime_mtd_tuples=len(unknown),pixel_behavior="OPEN",
      scope="PARTIAL_SOURCE_ROUTER_AND_SUPPLIED_GAME_LOGS_NOT_COMPLETE_FLVER_GAME_CENSUS")
    (output/"summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(summary,sort_keys=True))
if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--log",type=Path,action="append",default=[])
    p.add_argument("--output-dir",type=Path,required=True)
    a=p.parse_args();run(a.log,a.output_dir)
