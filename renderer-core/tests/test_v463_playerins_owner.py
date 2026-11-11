#!/usr/bin/env python3
"""Synthetic only: typed owner full handle vs low24 collision, never gameplay proof."""
import csv,importlib.util,tempfile
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"tools"/"analyze_v463_playerins_rx34_owner.py"
sp=importlib.util.spec_from_file_location("v463_analyzer",p)
a=importlib.util.module_from_spec(sp);sp.loader.exec_module(a)
def row(seq,ent,rowid,handle,ownerhandle,flags):
 v=[seq,1,rowid,ent,ent+0x108,0xe897fc0,0xe80,0,0xabcdef,
    0xabcd0,1,1,10,7,0,345,0xabf0021,
    flags,handle,0x12345000,0x1413251f0,0x34567000,ownerhandle,0x12340000]
 assert len(v)==24
 return [seq,100,seq+930,1000,21]+[f"{x:016x}" for x in v]
with tempfile.TemporaryDirectory() as t:
 f=Path(t)/"synthetic.csv"
 with f.open("w",newline="",encoding="utf8") as h:
  w=csv.writer(h);w.writerow(["seq","qpc","frame","tid","kind"]+[f"v{i:02d}" for i in range(24)])
  w.writerow(row(1,0x11000,30,0x8100007a,0x8100007a,127))
  w.writerow(row(2,0x22000,31,0x8200007a,0x8100007a,63))
  w.writerow(row(3,0x33000,31,0x8200007b,0x8100007a,63))
 r=a.analyze(f)
 assert r["typed_player_model_issued"]==3
 assert r["player_exact_full_handle_issued"]==1
 assert r["different_full_handle_issued_with_typed_player"]==2
 assert r["rows_confirmed_player_by_complete_handle"]==[30]
 assert r["rows_confirmed_not_same_player_model_handle"]==[31]
 assert r["per_native_entity"][1]["same_slot_different_generation_samples"]==1
 assert r["verdict"]=="PLAYER_ROW_OBSERVED_AND_NONPLAYER_HANDLES_OBSERVED__NPC_CLASS_OPEN"
print("V463 synthetic owner/handle regression PASS; not real game runtime")
