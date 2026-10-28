#!/usr/bin/env python3
"""V415 source-to-typed-model and RX34 issued-draw observational join.

No actor-lifetime, RTV/pass or pixel equivalence claims. Missing or conflicting
source witnesses always remain UNKNOWN, not a guessed nearest-model match.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re

TAG_SRC="[DSRRL V415 MODEL SOURCE JOIN]"
TAG_DRAW="[DSRRL V411 RX34 OWNER SEQ DRAW]"
KV=re.compile(r"([A-Za-z_]\w*)=([^\s]+)")

def scan(path):
    source=defaultdict(set)
    draws=[]
    count=Counter()
    with Path(path).open(encoding="utf-8",errors="replace") as log:
        for line_no,line in enumerate(log,1):
            if TAG_SRC in line:
                d=dict(KV.findall(line))
                count["v415_source_witness"]+=1
                key=(d.get("context"),d.get("native_producer_seq"))
                if all(key) and d.get("model"):
                    source[key].add((d["model"],d.get("container",""),
                        d.get("class",""),d.get("row_a",""),d.get("row_b","")))
            elif TAG_DRAW in line:
                d=dict(KV.findall(line));d["line"]=line_no
                draws.append(d);count["v411_issued_sample"]+=1
    by_material=defaultdict(lambda:defaultdict(set))
    examples=defaultdict(list)
    for d in draws:
        if d.get("kind")!="1" or d.get("stamp_state")!="1":
            count["unjoined_other_source_or_stamp"]+=1
            continue
        key=(d.get("owner"),d.get("native_producer_seq"))
        match=source.get(key,set())
        if not match:
            count["missing_model_source_witness"]+=1
            continue
        if len(match)!=1:
            count["ambiguous_model_source_witness"]+=1
            continue
        model,container,cls,row_a,row_b=next(iter(match))
        if d.get("row")!=row_a:
            count["source_row_mismatch"]+=1
            continue
        count["model_source_draw_observational_join"]+=1
        if d.get("rx")!="34":
            continue
        material_key=(d.get("owner"),d.get("route"),d.get("slot"),
                      d.get("flver_sha"),d.get("mtd_sha"),d.get("shader"))
        row=d.get("row")
        by_material[material_key][row].add(model)
        if len(examples[material_key])<10:
            examples[material_key].append(
                {"line":d["line"],"row":row,"model":model,
                 "container":container,"class":cls,
                 "native_producer_seq":d.get("native_producer_seq")})
    focus=[]
    for key,rows in by_material.items():
        if not ("30" in rows and "31" in rows):continue
        a,b=rows["30"],rows["31"]
        focus.append({
            "material_key":dict(zip(("owner","route","slot","flver_sha",
                                     "mtd_sha","shader"),key)),
            "models_row30":sorted(a),
            "models_row31":sorted(b),
            "shared_model_this":sorted(a&b),
            "observation":("shared_native_model_candidate" if a&b else
                           "distinct_native_model_candidates"),
            "samples":examples[key],
            "actor_identity":"UNVERIFIED",
            "render_target":"NOT_SAMPLED",
            "pixel_causality":"OPEN"})
    focus.sort(key=lambda x:(-len(x["samples"]),x["material_key"]["owner"]))
    return {"counts":dict(count),
       "row30_row31_model_candidates":focus,
       "limitations":[
           "source+model witness is observed at selector thread, not certified game actor",
           "matching Source1 sequence does not attest native actor lifetime/generation",
           "V411 issued draw may not prove final CB/RTV and visible pixel",
           "sample caps limit completeness; missing samples are UNKNOWN",
           "Source2 remains independently fail-open and is never guessed"]}

if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("log",type=Path)
    ap.add_argument("-o","--output",type=Path)
    args=ap.parse_args()
    result=json.dumps(scan(args.log),indent=2,ensure_ascii=False)+"\n"
    if args.output:args.output.write_text(result,encoding="utf-8")
    else:print(result,end="")
