#!/usr/bin/env python3
"""V417 exact native FgModel registration / LightBank issued draw audit.

Never claims gameplay actor identity, lifetime generation or pixel causality.
Only the exact (context, native_producer_seq) and row allows an observation join.
"""
from __future__ import annotations
import argparse,json,re
from pathlib import Path
from collections import Counter,defaultdict

SOURCE="[DSRRL V417 FG REGISTRY SOURCE JOIN]"
DRAW="[DSRRL V411 RX34 OWNER SEQ DRAW]"
KV=re.compile(r"([a-zA-Z_]\w*)=([^\s]+)")

def analyze(path:Path):
    sources=defaultdict(set);draws=[];counts=Counter()
    for no,line in enumerate(path.open(encoding="utf8",errors="replace"),1):
        if SOURCE in line:
            d=dict(KV.findall(line));counts["source_samples"]+=1
            key=(d.get("context"),d.get("native_producer_seq"))
            if all(key) and d.get("model") and d.get("parent"):
                # Match the row too. The registry ID is a weak consistency
                # signal and must never be the only ownership key.
                sources[key].add((
                    d.get("model"),d.get("parent"),
                    d.get("registry_id"),d.get("refcount"),
                    d.get("upstream"),d.get("upstream_word0"),
                    d.get("upstream_word0_readable"),d.get("row_a")))
        elif DRAW in line:
            d=dict(KV.findall(line));d["line"]=no
            draws.append(d);counts["draw_samples"]+=1
    candidates=defaultdict(lambda:defaultdict(set))
    anomalies=[]
    for d in draws:
        if d.get("kind")!="1" or d.get("stamp_state")!="1":
            counts["non_source1_or_unstamped"]+=1;continue
        key=(d.get("owner"),d.get("native_producer_seq"))
        matching=sources.get(key,set())
        if not matching:
            counts["missing_bounded_witness"]+=1;continue
        if len(matching)!=1:
            counts["conflicting_identity_witness"]+=1
            if len(anomalies)<25:
                anomalies.append({"line":d["line"],"issue":"multiple_model_parent_candidates","key":key})
            continue
        row=d.get("row")
        model,parent,reg,refs,upstream,word0,readable,source_row=next(iter(matching))
        if row!=source_row:
            counts["row_mismatch"]+=1
            if len(anomalies)<25:
                anomalies.append({"line":d["line"],"issue":"source_vs_draw_row_mismatch",
                                  "source_row":source_row,"draw_row":row,"key":key})
            continue
        counts["exact_sequence_row_observational_join"]+=1
        material=(d.get("owner"),d.get("route"),d.get("slot"),
                  d.get("shader"),d.get("flver_sha"),d.get("mtd_sha"))
        candidates[material][row].add((model,parent,reg,upstream,word0,readable))
    pairs=[]
    for material,byrow in candidates.items():
        if "30" not in byrow or "31" not in byrow:continue
        row30=byrow["30"];row31=byrow["31"]
        def present(c):
            return [{"model":v[0],"fg_parent":v[1],"registry_id":v[2],
                     "upstream":v[3],"upstream_word0":v[4],
                     "upstream_word0_readable":v[5]} for v in sorted(c)]
        modelparent=lambda v:(v[0],v[1])
        parents_a={modelparent(x) for x in row30}
        parents_b={modelparent(x) for x in row31}
        handles_a={x[2] for x in row30}
        handles_b={x[2] for x in row31}
        pairs.append({
            "material_key":dict(zip(
                ("context","route","slot","shader","flver_sha","mtd_sha"),material)),
            "row30":present(row30),"row31":present(row31),
            "shared_exact_model_parent":sorted(parents_a&parents_b),
            "shared_registry_ids":sorted(handles_a&handles_b),
            "same_renderable_candidate":bool(parents_a&parents_b),
            "registry_id_is_actor_id":False,
            "registered_object_lifetime":"OPEN",
            "gpu_pass_and_visible_pixel":"OPEN"})
    return {"counts":dict(counts),"row30_row31":pairs,"anomalies":anomalies,
            "evidence_limits":[
                "The FgModel registry number is not a proven unique actor/lifetime generation.",
                "One source first qword need not be a vtable; upstream class remains unverified.",
                "Different model+parent observations can be separate actors or separate renderables.",
                "Same parent in separate observations does not certify same GPU pass or visible pixels.",
                "Truncated witness logs mean missing samples are UNKNOWN, not opposite-row evidence.",
                "Producer equality and draw join do not imply PTDE pixel equivalence."]}
if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("log",type=Path)
    ap.add_argument("-o","--output",type=Path)
    args=ap.parse_args()
    txt=json.dumps(analyze(args.log),indent=2,ensure_ascii=False)+"\n"
    if args.output:args.output.write_text(txt,encoding="utf8")
    else:print(txt,end="")
