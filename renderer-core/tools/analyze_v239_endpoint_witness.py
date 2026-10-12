#!/usr/bin/env python3
"""Analyze V239 read-only exact material Lerp endpoint witness log.

Reports producer/input continuity only. Prepared shader/CB/SRV is NOT pixel
or issued-draw equivalence. A missing pair is OPEN, never a failure.
"""
import argparse
import math
import re
from collections import defaultdict
from pathlib import Path

MARK="[DSRRL V239 ENDPOINT]"
KVS=re.compile(r"([A-Za-z][A-Za-z0-9_]*)=([^\s]+)")

def parse(raw):
    return [dict(KVS.findall(line.split(MARK,1)[1]))
            for line in raw.splitlines() if MARK in line]

def vec(text):
    try:
        v=tuple(float(x) for x in text.split(","))
        return v if len(v)==3 and all(math.isfinite(x) for x in v) else None
    except (ValueError,AttributeError):
        return None

def same_channel(a,b):
    av,bv=vec(a),vec(b)
    return None if av is None or bv is None else max(abs(x-y) for x,y in zip(av,bv))

def analyze(entries):
    groups=defaultdict(list)
    for r in entries:
        key=(r.get("class"),r.get("route"),r.get("slot"),
             r.get("material"),r.get("flver"),r.get("rx"))
        groups[key].append(r)
    results=[]
    for key,rows in sorted(groups.items()):
        stable=[x for x in rows if x.get("family")=="STABLE"]
        lerp=[x for x in rows if x.get("family")=="LERP"]
        bypair=defaultdict(list)
        for x in lerp:
            bypair[(x.get("bankA"),x.get("rowA"),
                    x.get("bankB"),x.get("rowB"),
                    x.get("probeA"),x.get("probeB"))].append(x)
        pairs=[]
        for pair,parts in sorted(bypair.items()):
            bins=sorted({int(x["bin"]) for x in parts if x.get("bin","").isdigit()})
            beta=[float(x["beta"]) for x in parts
                  if x.get("beta") and math.isfinite(float(x["beta"]))]
            matches=[]
            for l in parts:
                for anchor,s in (("A","specA"),("B","specB")):
                    for z in stable:
                        if l.get("bank"+anchor)!=z.get("bankA") or l.get("row"+anchor)!=z.get("rowA"):
                            continue
                        # Same exact material+source identity. Compare BOTH
                        # EnvSpec and independent EnvDiffuse endpoint signals.
                        spec=same_channel(l.get(s),z.get("specA"))
                        diff=same_channel(l.get("diff"+anchor),z.get("diffA"))
                        if spec is not None and diff is not None:
                            matches.append(dict(side=anchor,spec_delta=spec,diff_delta=diff))
            pairs.append(dict(pair=pair,bands=bins,beta_min=min(beta) if beta else None,
                              beta_max=max(beta) if beta else None,endpoint_matches=matches,
                              signal_continuity=("INPUT_MISMATCH" if any(
                                  x["spec_delta"]>1e-5 or x["diff_delta"]>1e-5 for x in matches)
                                  else "INPUT_MATCH_ONLY" if matches else "OPEN_NO_STABLE_MATCH"),
                              beta_span=("OBSERVED_NEAR_FULL" if min(beta,default=1)<=0.065
                                         and max(beta,default=0)>=0.935
                                         else "OPEN_INCOMPLETE_SPAN")))
        results.append((key,len(stable),pairs))
    return results

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("log",type=Path)
    a=ap.parse_args()
    events=parse(a.log.read_text(encoding="utf-8",errors="replace"))
    if not events:
        raise SystemExit("V239_NO_ENDPOINT_WITNESS_IN_LOG: check exact RX33 material hits")
    print("V239 witness events",len(events),"only_REQUEST_READY, pixel=OPEN")
    for key,n_stable,pairs in analyze(events):
        print("MATERIAL",key,"stable",n_stable,"pairs",len(pairs))
        for p in pairs:
            print(" PAIR",p["pair"],"bands",p["bands"],
                  "beta",p["beta_min"],p["beta_max"],
                  "span",p["beta_span"],
                  "input",p["signal_continuity"],
                  "matched_anchors",len(p["endpoint_matches"]))
    print("PIXEL_AND_DRAW_VALIDATION=OPEN")
if __name__=="__main__":main()
