#!/usr/bin/env python3
"""V464 bounded kind22 RX34 actor ownership CSV. Never extrapolate to pixels."""
import argparse,csv,json,collections
from pathlib import Path
STATUS={0:"UNTYPED",1:"UNKNOWN_NO_OWNER",2:"PLAYERINS_LIVE_MODEL_HANDLE",
        3:"ENEMYINS_LIVE_MODEL_HANDLE",4:"AMBIGUOUS",
        5:"HOOK_OFF",6:"INVALID_HANDLE"}
def analyze(path):
 groups=collections.defaultdict(lambda:collections.defaultdict(list))
 counts=collections.Counter()
 for r in csv.reader(path.open("r",encoding="utf-8-sig",newline="")):
  if len(r)!=29 or r[4]!="22":continue
  v=[int(x,16) for x in r[5:]]
  source,row,ent,handle,status=v[1:6]
  cls=STATUS.get(status,"UNKNOWN_STATUS")
  # Full generation is mandatory. low24 alone is never an owner identity.
  allocated=bool(handle&0x80000000)
  full_equal=(v[10]==handle and allocated)
  owner_valid=(status in (2,3) and v[6]!=0 and v[8]!=0 and full_equal and v[11]==1)
  if status in (2,3) and not owner_valid:
   cls="REJECT_INCONSISTENT_OWNER_PAYLOAD"
  sample={"frame":int(r[2]),"source_kind":source,"row":row,
   "entity":f"0x{ent:x}","entity_handle":f"0x{handle:08x}",
   "generation":(handle>>24)&0x7f,"slot":handle&0xffffff,
   "actor_class":cls,"owner":f"0x{v[6]:x}",
   "model":f"0x{v[8]:x}","model_handle":f"0x{v[10]:08x}",
   "candidate_count":v[11],"source1_certified":source==1,
   "owner_live_match":owner_valid,"router_hits":v[19],
   "router_owner_records":v[20]}
  groups[ent][handle].append(sample)
  counts[cls]+=1
  counts["events"]+=1
 out=[]
 for ent,handles in sorted(groups.items()):
  for h,events in sorted(handles.items()):
   passes={x["actor_class"] for x in events if x["owner_live_match"]}
   if len(passes)>1: state="CONFLICT_REQUIRES_INSPECTION"
   elif passes:state=next(iter(passes))
   else:state="OPEN_NOT_CERTIFIED"
   out.append({"entity":hex(ent),"full_handle":f"0x{h:08x}",
     "row_values":sorted({x["row"] for x in events}),
     "statuses":sorted({x["actor_class"] for x in events}),
     "state":state,"samples":events[:8]})
 return {"version":"V464","records":dict(counts),"entities":out,
    "hard_limits":["Class EnemyIns does not distinguish friendly dialogue NPC from hostile enemy.",
      "A live owner+model handle join does not certify full gameplay lifetime when generation wraps.",
      "No GPU RT/CB pixel coverage or PTDE-visible comparison is established.",
      "Only source kind 1 retains native Source1 packet certification; Source2 stays protected."],
    "actor_identity_runtime":"CONFIRMED_FOR_MATCHED_LIVE_TYPES_ONLY" if any(
      any(x["owner_live_match"] for x in h["samples"]) for h in out) else "OPEN",
    "pixel":"OPEN"}
if __name__=="__main__":
 p=argparse.ArgumentParser();p.add_argument("csv",type=Path);p.add_argument("--json",type=Path)
 a=p.parse_args();r=analyze(a.csv);s=json.dumps(r,ensure_ascii=False,indent=2)+"\n"
 if a.json:a.json.write_text(s,encoding="utf8")
 print(s,end="")
