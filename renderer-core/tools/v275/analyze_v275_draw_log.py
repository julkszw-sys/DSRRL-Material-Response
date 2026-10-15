#!/usr/bin/env python3
"""Analyze actual V275 owner material draw routing, not PTDE-visible pixels.

Usage:
 python renderer-core/tools/v275/analyze_v275_draw_log.py ReShade.log --out out_dir
 Optional --labels labels.csv with full_mtd_sha256,status where status BAD/GOOD.
 Logs are source authority plus pre-dispatch D3D11 state and transaction status;
 planned replacement cannot prove a GPU shader/sidecar actually produced pixels.
"""
from __future__ import annotations
import argparse,csv,json,re,collections
from pathlib import Path

TAG="[DSRRL V275 MATERIAL DRAW]"
REQUIRED=("stage","phase","rx","name","route","decision_active","reason",
          "full_mtd","full_flver","sem","slot","profile_sha_exact",
          "name_exact","planned_t10","repl_t10","host_t1","host_t10",
          "replace_ps","planned_ps","fog12w","fog_bucket")
def decode_line(line):
    if TAG not in line:return None
    text=line[line.find(TAG)+len(TAG):]
    # Fields never contain spaces other than the diagnostic NAME for none of
    # the canonical Cloth/Leather MTD entries (underscore + bracket syntax).
    data=dict(re.findall(r"(\w+)=([^ \r\n]+)",text))
    if any(field not in data for field in REQUIRED):return None
    return data
def load_labels(path):
    if path is None:return {}
    with path.open(newline="",encoding="utf8-sig") as f:
        rows=list(csv.DictReader(f))
    allowed={"GOOD","BAD","UNKNOWN"}
    labels={}
    for row in rows:
        sha=row["full_mtd_sha256"].strip().lower()
        status=row["status"].strip().upper()
        if status not in allowed or not re.fullmatch(r"[0-9a-f]{64}",sha):
            raise ValueError("Invalid label; require full 64-character hash and GOOD/BAD/UNKNOWN")
        labels[sha]=status
    return labels

def prepare(rows,labels):
    groups=collections.defaultdict(list)
    for row in rows:
        key=(row["full_mtd"],row["sem"],row["full_flver"],row["slot"])
        groups[key].append(row)
    material_report=[]
    for (mtd,sem,flver,slot),samples in sorted(groups.items()):
        phases=collections.defaultdict(list)
        for r in samples:phases[r["phase"]].append(r)
        events={phase:sorted({r["stage"] for r in rs}) for phase,rs in phases.items()}
        problems=[]
        if any(r["profile_sha_exact"]!="1" or r["name_exact"]!="1"
               for r in samples):problems.append("MTD_AUTHORITY_MISMATCH")
        if any(r.get("ambiguous")=="1" for r in samples):problems.append("SEMANTIC_AMBIGUITY")
        if any(r.get("duplicate_sha")=="1" for r in samples):
            problems.append("SAME_RAW_SHA_MULTIPLE_SEMANTIC_PROFILES")
        if any(r["decision_active"]=="0" for r in samples):
            problems.append("MATERIAL_RESPONSE_INACTIVE_ON_SAMPLED_DRAW")
        if any("PREPARE_FAIL"==r["stage"] for r in samples):
            problems.append("ISLAND_PREPARE_FAIL_OPEN")
        if any("DISPATCH_FAIL_OPEN"==r["stage"] for r in samples):
            problems.append("ISLAND_DISPATCH_FAIL_OPEN")
        if any("RESTORE_FAILED"==r["stage"] for r in samples):
            problems.append("RESTORE_FAILED_QUARANTINE")
        for phase,rs in phases.items():
            planned=[r for r in rs if r["stage"]=="BATCH_PREPARED"]
            if planned and not any(r["stage"]=="DRAW_ISSUED" for r in rs):
                problems.append(phase+"_NO_ATTESTED_DRAW_ISSUED")
            if any(r["replace_ps"]=="1" and r["planned_ps"]=="0000000000000000"
                   for r in planned):
                problems.append(phase+"_NULL_PLANNED_SHADER")
            if any(r["planned_t10"]=="1" and r["repl_t10"]=="0000000000000000"
                   for r in planned):
                problems.append(phase+"_NULL_PLANNED_T10")
        if ("STABLE" not in phases or "LERP" not in phases):
            problems.append("MISSING_STABLE_LERP_SAME_MATERIAL_COVERAGE")
        # NOTE: Fog-cb12.w is a coarse source-state proxy. Never call it beta.
        bins={phase:sorted(set(r["fog_bucket"] for r in rs))
              for phase,rs in phases.items()}
        pair_flags={
            phase:{
                "has_prepared_replacement":any(
                    r["replace_ps"]=="1" and r["planned_ps"]!="0000000000000000"
                    for r in rs if r["stage"]=="BATCH_PREPARED"),
                "has_prepared_t10":any(r["planned_t10"]=="1"
                    for r in rs if r["stage"]=="BATCH_PREPARED"),
                "has_issued_draw":any(r["stage"]=="DRAW_ISSUED" for r in rs),
                "sampled_cb85":sorted({r.get("cb85","") for r in rs}),
            } for phase,rs in phases.items()
        }
        if len(pair_flags)==2 and all(x in pair_flags for x in ("STABLE","LERP")):
            a,b=pair_flags["STABLE"],pair_flags["LERP"]
            for f in ("has_prepared_replacement","has_prepared_t10","has_issued_draw"):
                if a[f]!=b[f]:problems.append("STABLE_LERP_"+f.upper()+"_DIFF")
        material_report.append({
            "mtd_name":samples[0]["name"],"full_mtd_sha256":mtd,"flver":flver,
            "semantic_hash":sem,"slot":slot,"receiver":"/".join(sorted({r["rx"] for r in samples})),
            "label":labels.get(mtd,"UNKNOWN"),"events":events,"fog12w_bins_not_beta":bins,
            "route_index":sorted({r["route"] for r in samples}),
            "route_signatures":pair_flags,
            "flags":sorted(set(problems)), "sampled_event_rows":len(samples)})
    return material_report

def main():
    p=argparse.ArgumentParser()
    p.add_argument("log",type=Path)
    p.add_argument("--out",type=Path,default=Path("v275_comparison"))
    p.add_argument("--labels",type=Path)
    a=p.parse_args()
    rows=[r for line in a.log.read_text(encoding="utf8",errors="replace").splitlines()
          if (r:=decode_line(line)) is not None]
    report=prepare(rows,load_labels(a.labels))
    a.out.mkdir(parents=True,exist_ok=True)
    (a.out/"groups.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf8")
    (a.out/"coverage.json").write_text(json.dumps({
        "log":a.log.name,"matched_draw_events":len(rows),
        "matched_material_groups":len(report),
        "status":"IDENTITY_AND_DRAW_ROUTING_ONLY",
        "pixel":"UNOBSERVED",
        "gpu_physical_cb":"UNOBSERVED",
        "no_authority":"Absence of a record means no attested observation, not no draw",
        "flags_by_type":dict(collections.Counter(
            f for row in report for f in row["flags"])),
        "bad_labelled_materials":sum(row["label"]=="BAD" for row in report),
        "good_labelled_materials":sum(row["label"]=="GOOD" for row in report),
    },indent=2)+"\n",encoding="utf8")
    with (a.out/"outlier_matrix.csv").open("w",newline="",encoding="utf8") as f:
        w=csv.writer(f);w.writerow(["material","mtd_sha","flver","slot","rx","label",
                                    "phases","route","flags"])
        for row in report:w.writerow([
            row["mtd_name"],row["full_mtd_sha256"],row["flver"],row["slot"],
            row["receiver"],row["label"],",".join(row["events"]),
            ",".join(row["route_index"]),";".join(row["flags"])])
    print(f"V275_PARSED={len(rows)} MATCHED_MATERIAL_GROUPS={len(report)}")
    print("PIXEL_OPEN, non-GPU CPU state; see coverage.json and outlier_matrix.csv")

if __name__=="__main__":main()
