#!/usr/bin/env python3
"""V330 read-only operator witness. No addon code or runtime hooks modified.

Verify the exact original PTDE MTD envspec-slot distinction that explains
why t12/t14 handles can differ with identical probe A/B, and optionally
match an owner-provided V326 ReShade log to identical-source Lerp draws.
No visual-positive Wanderer 9530 attribution from unnamed log FLVER prefixes.
"""
from __future__ import annotations
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import re
import struct

ROOT=Path(__file__).resolve().parents[1]
ROUTER=ROOT/"data/census/ptde_mtd_envspec_router_v1.json"
SOURCE=ROOT/"src/runtime/envspec_resource_runtime.cpp"
MTD={
 "P_Leather[DSB].mtd":("0f2b9a1012b83c4d",1,1.5,4.0),
 "P_DullLeather[DSB].mtd":("7ea7d6ce9bb7148d",0,1.0,2.0)
}
PAT=re.compile(r"([a-zA-Z0-9_]+)=([^\s]+)")

def float_bits(v):
 return struct.unpack(">f",bytes.fromhex(v.removeprefix("0x")))[0]

def static_source_contract():
 raw=json.loads(ROUTER.read_text(encoding="utf-8"))
 assert len(raw["records"])==325,"Original router census completeness changed"
 src=SOURCE.read_text(encoding="utf-8")
 assert "const auto key_a =" in src and "const auto key_b =" in src
 assert "probe_a) *" in src and "probe_b) *" in src
 assert "k_ptde_slots +" in src and "slot;" in src
 for name,(sha,slot,c101,c102) in MTD.items():
  matches=[r for r in raw["records"] if r["mtd_name"]==name and r["dsr_mtd_sha256"].startswith(sha)]
  assert len(matches)==1,("Raw MTD identity ambiguous",name)
  r=matches[0]
  assert r["state"]=="PRESENT" and r["envspc_slot"]==slot
  assert all(float_bits(x)==c101 for x in r["c101_f32_bits"])
  assert float_bits(r["c102_f32_bits"])==c102
 print("V330 SOURCE PASS: Leather envslot1 c101=1.5 c102=4; DullLeather envslot0 c101=1 c102=2; PTDE cube index keyed by probe+slot")

def runtime_pairs(log):
 lines=log.read_text(encoding="utf-8-sig",errors="replace").splitlines()
 A=[];B=[]
 for i,text in enumerate(lines,1):
  if "[DSRRL V291 LERP CUBEMAP]" not in text or "stage=indexed_issued" not in text:continue
  f=dict(PAT.findall(text));f["line"]=i
  if f.get("mtd","").startswith(MTD["P_Leather[DSB].mtd"][0]) and f.get("slot")=="4":A.append(f)
  elif f.get("mtd","").startswith(MTD["P_DullLeather[DSB].mtd"][0]) and f.get("slot")=="3":B.append(f)
 matching=("rx","flver","beta","serial","pair_join","spec_a","spec_b","diffuse_a","diffuse_b","s_envspec","s_envdiffuse","b12","t11","t13")
 index=defaultdict(list)
 for b in B:index[tuple(b.get(k) for k in matching)].append(b)
 joined=[]
 for a in A:
  key=tuple(a.get(k) for k in matching)
  matches=index[key]
  if not matches:continue
  b=min(matches,key=lambda b:abs(b["line"]-a["line"]))
  matches.remove(b)
  joined.append({"a_line":a["line"],"b_line":b["line"],"rx":a["rx"],"flver_prefix":a["flver"],
                 "beta":a["beta"],"probe_a":a["spec_a"],"probe_b":a["spec_b"],
                 "leather_envspc_slot":1,"dull_envspc_slot":0,
                 "t12_diff":a["t12"]!=b["t12"],"t14_diff":a["t14"]!=b["t14"],
                 "same_t11_t13":a["t11"]==b["t11"] and a["t13"]==b["t13"],
                 "9530_identity":"UNATTRIBUTED"})
 return {"log_sha256":hashlib.sha256(log.read_bytes()).hexdigest(),
         "pairs":joined,"count":len(joined),
         "no_pixel_claim":True,"9530_asset_sha_to_submesh":"OPEN"}

def main():
 ap=argparse.ArgumentParser()
 ap.add_argument("--log",type=Path)
 ap.add_argument("--output",type=Path)
 args=ap.parse_args()
 static_source_contract()
 if args.log:
  result=runtime_pairs(args.log)
  if args.output:
   args.output.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
  print("V330 LOG PAIRS",result["count"],"9530 map OPEN",
        "distinct t12/t14 expected when slots differ")
if __name__=="__main__":main()
