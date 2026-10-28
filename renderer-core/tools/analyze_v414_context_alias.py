#!/usr/bin/env python3
"""Read-only V413 log RE. Never treats context identity as gameplay actor proof."""
import argparse
import collections
import json
import re
from pathlib import Path

TAGS=("V383 REMO MODEL MATERIAL","V409 RX34 GPU TRANSITION","V411 RX34 OWNER SEQ DRAW")
FIELDS=re.compile(r"([A-Za-z_]\w*)=([^ ]+)")
TIME=re.compile(r"^(\d\d):(\d\d):(\d\d):(\d{3})")

def analyze(path):
    entries={k:[] for k in TAGS}
    for line,raw in enumerate(Path(path).open(encoding="utf-8",errors="replace"),1):
        for tag in TAGS:
            if "[DSRRL "+tag+"]" not in raw: continue
            d=dict(FIELDS.findall(raw)); d["line"]=line
            m=TIME.match(raw)
            if m:
                h,mm,s,ms=map(int,m.groups())
                d["ms_day"]=((h*60+mm)*60+s)*1000+ms
            entries[tag].append(d); break
    owners=collections.defaultdict(lambda:{"flver":set(),"descriptors":set(),"count":0})
    for d in entries[TAGS[0]]:
        if "owner" not in d: continue
        o=owners[d["owner"]];o["count"]+=1
        if d.get("flver_sha256"):o["flver"].add(d["flver_sha256"])
        if d.get("descriptor"):o["descriptors"].add(d["descriptor"])
    grouped=collections.defaultdict(list)
    names=("owner","route","slot","shader","flver_sha","mtd_sha")
    for d in entries[TAGS[1]]:
        if d.get("rx")=="34":grouped[tuple(d.get(k,"") for k in names)].append(d)
    patterns=[]
    for key,group in grouped.items():
        pairs=[(a,b) for a,b in zip(group,group[1:])
               if a.get("kind")==b.get("kind")=="1" and
               a.get("row")=="30" and b.get("row")=="31" and
               "ms_day" in a and "ms_day" in b]
        if pairs:
            patterns.append({"key":dict(zip(names,key)),
              "rows":dict(collections.Counter(d.get("row") for d in group)),
              "sample_count":len(group),"pairs_30_to_31":len(pairs),
              "gap_ms":dict(collections.Counter(str(b["ms_day"]-a["ms_day"]) for a,b in pairs)),
              "first_lines":[pairs[0][0]["line"],pairs[0][1]["line"]],
              "first_probe_pair":[pairs[0][0].get("probe"),pairs[0][1].get("probe")]})
    patterns.sort(key=lambda d:d["pairs_30_to_31"],reverse=True)
    return {"sample_counts":{k:len(v) for k,v in entries.items()},
      "owner_aliases":{k:{"samples":v["count"],"distinct_flver":len(v["flver"]),
        "distinct_descriptors":len(v["descriptors"])} for k,v in owners.items()},
      "draw_kind":dict(collections.Counter(d.get("kind") for d in entries[TAGS[2]])),
      "patterns":patterns,
      "not_proven":["gameplay actor instance","model lifetime generation","render pass/RTV",
                    "command-list/CB to visible pixel join","PTDE equivalence"]}

if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("log");p.add_argument("-o","--output")
    a=p.parse_args();s=json.dumps(analyze(a.log),indent=2,ensure_ascii=False)+"\n"
    if a.output:Path(a.output).write_text(s,encoding="utf-8")
    else:print(s,end="")
