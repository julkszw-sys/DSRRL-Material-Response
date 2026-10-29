#!/usr/bin/env python3
"""V418 native Source1 -> RX34 exact issued draw model/registry certificate audit.

Never infer a game actor, GPU RTV/CB, or pixel equivalence. Missing and
ambiguous producer journal snapshots are explicitly rejected observations.
"""
import argparse
from collections import Counter,defaultdict
import json,re
from pathlib import Path
PREFIX="[DSRRL V418 RX34 MODEL ISSUED JOIN]"
FIELD=re.compile(r"([A-Za-z_]\w*)=([^\s]+)")
EXACT={"journal_status":"1","candidates":"1","parent_reciprocal":"1",
       "model_verified":"1","registry_readable":"1","allocated":"1",
       "cache_seq_matches":"1","exact_renderable_join":"1"}
def analyze(path):
    counts=Counter()
    seen=defaultdict(lambda:defaultdict(set))
    examples=defaultdict(list)
    for no,line in enumerate(Path(path).open(encoding="utf8",errors="replace"),1):
        if PREFIX not in line:continue
        d=dict(FIELD.findall(line));counts["issued_samples"]+=1
        invalid=[k for k,v in EXACT.items() if d.get(k)!=v]
        if invalid:
            counts["not_strict"]+=1
            for k in invalid:counts["reject_"+k]+=1
            continue
        counts["strict"]+=1
        key=tuple(d.get(k,"") for k in
             ("owner","route","slot","shader","flver_sha","mtd_sha"))
        try:
            h=int(d["registry_handle"],16)
            slot=h&0xffffff
            generation=(h>>24)&0x7f
        except (KeyError,ValueError):
            counts["reject_invalid_registry_hex"]+=1
            continue
        if d.get("registry_slot")!=str(slot) or d.get("registry_generation")!=str(generation):
            counts["reject_registry_decode_mismatch"]+=1
            continue
        row=d.get("row","")
        identity=(d.get("model"),d.get("fg_parent"),slot,generation)
        seen[key][row].add(identity)
        if len(examples[key])<12:
            examples[key].append({"log_line":no,"row":row,
                                  "native_producer_seq":d.get("native_producer_seq"),
                                  "model":identity[0],"fg_parent":identity[1],
                                  "slot":slot,"generation":generation})
    groups=[]
    for key,byrow in seen.items():
        if "30" not in byrow or "31" not in byrow:continue
        row30,row31=byrow["30"],byrow["31"]
        groups.append({"key":dict(zip(
            ("owner","route","slot","shader","flver_sha","mtd_sha"),key)),
            "row30_model_parent_slot_generation":sorted(row30),
            "row31_model_parent_slot_generation":sorted(row31),
            "shared_exact_renderable_identity":sorted(row30&row31),
            "status":"same_renderable_candidate_present" if row30&row31
                      else "different_renderable_candidates",
            "sample_events":examples[key],
            "gameplay_actor":"OPEN",
            "render_target_and_draw_cb":"OPEN",
            "pixel_behavior":"OPEN"})
    return {"counts":dict(counts),"row30_vs_row31":groups,
        "limits":[
            "This certifies one observed Source1 journal payload to one issued-draw event, not native game actor call.",
            "FgModel registry generation is 7 bits and may wrap; namespace and reuse lifecycle remain OPEN.",
            "A strict identity join does not prove pixel visibility or PTDE equivalence.",
            "V411 transitions are sampled and journal history is bounded to eight; absent samples are UNKNOWN.",
            "Source2 is protected and never certified with Source1's native producer sequence."]}
if __name__=="__main__":
    a=argparse.ArgumentParser()
    a.add_argument("log",type=Path)
    a.add_argument("-o","--output",type=Path)
    opt=a.parse_args()
    result=json.dumps(analyze(opt.log),ensure_ascii=False,indent=2)+"\n"
    if opt.output:opt.output.write_text(result,encoding="utf8")
    else:print(result,end="")
