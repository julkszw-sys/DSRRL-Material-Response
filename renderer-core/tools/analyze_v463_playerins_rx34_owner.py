#!/usr/bin/env python3
"""Read V463 manual F9 RX34 owner-handle join, no game mutations.
DO NOT use with V459 kind21: v17..v23 semantics differ.
"""
import argparse,collections,csv,json
from pathlib import Path

def analyze(path):
 total=collections.Counter();fail=collections.Counter()
 by=(collections.defaultdict(lambda:{"row_counts":collections.Counter(),
       "handles":set(),"player_handles":set(),"actor_vtables":set(),
       "world_ptrs":set(),"samples":0,"full_handle_equals":0,
       "low24_equals_only":0,"valid_player_model":0,"source_kinds":set()}))
 recs=[]
 with path.open(encoding="utf-8-sig",newline="",errors="replace") as f:
  for line in csv.reader(f):
   if not line or line[0].startswith("#") or line[0]=="seq":continue
   if len(line)<29:fail["short"]+=1;continue
   try:
    kind=int(line[4]);total[f"kind_{kind}"]+=1
    if kind!=21:continue
    v=[int(x,16) for x in line[5:29]]
    seq,source,row,entity,dp,engine_model,resource,controller,packet,rbp,\
      res_ok,ctrl_ok,status450,probe,slot,route,context,flags,issued_h,\
      actor,actor_vt,chrmodel,player_h,world=v
    if route!=345 or row not in (30,31):fail["out_of_scope"]+=1;continue
    if source!=1:fail["non_Source1_without_native_authority"]+=1;continue
    key=(entity,issued_h)
    g=by[key];g["samples"]+=1;g["row_counts"][row]+=1
    g["handles"].add(issued_h);g["player_handles"].add(player_h)
    g["actor_vtables"].add(actor_vt);g["world_ptrs"].add(world)
    g["source_kinds"].add(source)
    full=(flags&127)==127
    valid=(flags&63)==63
    if valid:g["valid_player_model"]+=1
    if full:g["full_handle_equals"]+=1
    if valid and not full and (issued_h&0xffffff)==(player_h&0xffffff):
     g["low24_equals_only"]+=1
    # Exact mapping requires v443 V450 certified issued native default builder.
    if probe!=7 or status450!=10:fail["uncertified_native_source"]+=1;continue
    if bool(flags&64)!=(valid and issued_h==player_h):
     fail["owner_status_internal_inconsistency"]+=1
    recs.append({"frame":int(line[2]),"seq":seq,"row":row,"entity":entity,
      "issued_h":issued_h,"player_h":player_h,"world_actor":actor,
      "actor_vt":actor_vt,"chrmodel":chrmodel,"flags":flags,"full":full,
      "valid":valid,"generation_equal":issued_h>>24==player_h>>24,
      "lower_slot_equal":(issued_h&0xffffff)==(player_h&0xffffff)})
   except (ValueError,IndexError):
    fail["invalid"]+=1
 ret=[]
 for (entity,issued_h),g in sorted(by.items()):
  ret.append({"entity":hex(entity),"issued_full_handle":hex(issued_h),
    "row_counts":dict(sorted(g["row_counts"].items())),"samples":g["samples"],
    "typed_player_model_samples":g["valid_player_model"],
    "full_player_match_samples":g["full_handle_equals"],
    "same_slot_different_generation_samples":g["low24_equals_only"],
    "observed_player_model_handles":[hex(x) for x in sorted(g["player_handles"])],
    "world_singleton_ptrs":[hex(x) for x in sorted(g["world_ptrs"])],
    "actor_vtables":[hex(x) for x in sorted(g["actor_vtables"])]})
 valid=[x for x in recs if x["valid"]]
 equal=[x for x in valid if x["full"]]
 others=[x for x in valid if not x["full"]]
 player_rows=sorted({x["row"] for x in equal})
 nonplayer_rows=sorted({x["row"] for x in others})
 if equal and others and not fail["owner_status_internal_inconsistency"]:
  verdict="PLAYER_ROW_OBSERVED_AND_NONPLAYER_HANDLES_OBSERVED__NPC_CLASS_OPEN"
 elif equal:
  verdict="PLAYER_ROW_OBSERVED__NONPLAYER_ROW_UNDETERMINED"
 elif valid:
  verdict="PLAYER_TYPED_BUT_NO_MATCHING_RX34_WITHIN_CAPTURE"
 else:verdict="TYPED_PLAYER_OWNER_NOT_ESTABLISHED__INCONCLUSIVE"
 return {"format":"V463 kind21 fields v17..v23 owner identity NOT V459 Light pair",
   "status":"RUNTIME_ANALYSIS_OF_FILE_NOT_PTDE_PIXEL_PASS",
   "event_counts":dict(total),"failures":dict(fail),
   "issued_Source1_rows":dict(collections.Counter(x["row"] for x in recs)),
   "typed_player_model_issued":len(valid),
   "player_exact_full_handle_issued":len(equal),
   "different_full_handle_issued_with_typed_player":len(others),
   "rows_confirmed_player_by_complete_handle":player_rows,
   "rows_confirmed_not_same_player_model_handle":nonplayer_rows,
   "per_native_entity":ret,
   "verdict":verdict,
   "limits":["WorldChrManImp+0x68 candidate must pass exact PlayerIns vtable",
     "PlayerIns+0x60 must pass exact ChrModel vtable",
     "Matching complete 32-bit handle is stronger than low24 slot-only equality",
     "Nonplayer handle does NOT identify enemy, neutral/dialogue NPC, other pass or actor generation",
     "Source2 RX34 not certified by Source1 actor producer ticket",
     "CPU frame cooccurrence is not same-pixel GPU coverage",
     "Correct PTDE-visible pixels remain OPEN"]}
if __name__=="__main__":
 a=argparse.ArgumentParser();a.add_argument("csv",type=Path)
 a.add_argument("--json",type=Path);x=a.parse_args()
 report=json.dumps(analyze(x.csv),indent=2,ensure_ascii=False)+"\n"
 if x.json:x.json.write_text(report,encoding="utf-8")
 print(report)
