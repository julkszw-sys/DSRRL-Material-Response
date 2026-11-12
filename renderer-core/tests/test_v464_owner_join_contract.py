#!/usr/bin/env python3
"""Synthetic V464 source/decoder contract, no game runtime or PTDE pixel claim."""
import csv,io,importlib.util,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
src=(root/"include/dsrrl/runtime/v464_actor_owner.hpp").read_text()
overlay=(root/"tools/v464_actor_owner_overlay.py").read_text()
assert "0x32D410u" in src and "0x12EAF30u" in src
assert "0x13251F0u" in src and "0x1322E68u" in src
assert "0x80000000u" in src and "now.handle!=s.handle" in src
assert "full handle" in src.lower() and "unknown" in src.lower()
assert "v464::resolve(entity459,true)" in overlay
assert "V464_OWNER_HANDLE_AT_ISSUE" in overlay
assert "valid_builder451" in overlay
assert "first_or_transition" in overlay
for forbidden in ("latest_hook_source(","publish_hook_source(","row_a = 30","row_a=30",
                  "source_kind =","source_kind=","D3D11"):
 assert forbidden not in overlay,forbidden
handle=0x8100001f
def row(status,actor=0,model=0,model_handle=0,count=0,frame=100):
 vals=[0]*24
 vals[:21]=[17,1,30,0x2189d9f0,handle,status,actor,
    0x1413251f0 if status==2 else 0x141322e68,
    model,0x1412eaf30,model_handle,count,0,0,0,9,3,345,15,12,5]
 return ["1","123456",str(frame),"155","22"]+[f"{v:016x}" for v in vals]
rows=[row(1,frame=101),
      row(2,actor=0x1234000,model=0x5678000,model_handle=handle,count=1,frame=102),
      row(3,actor=0x9990000,model=0x8880000,model_handle=handle,count=1,frame=103),
      row(2,actor=0x1234000,model=0x5678000,model_handle=0x8200001f,count=1,frame=104)]
with tempfile.TemporaryDirectory() as d:
 file=Path(d)/"capture.csv"
 with file.open("w",newline="") as f:
  w=csv.writer(f);w.writerow(["seq","qpc","frame","tid","kind"]+[f"v{i:02d}" for i in range(24)])
  w.writerows(rows)
 spec=importlib.util.spec_from_file_location("a464",root/"tools/analyze_v464_rx34_actor_owner.py")
 module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
 result=module.analyze(file)
 assert result["records"]["UNKNOWN_NO_OWNER"]==1
 assert result["records"]["PLAYERINS_LIVE_MODEL_HANDLE"]==1
 assert result["records"]["ENEMYINS_LIVE_MODEL_HANDLE"]==1
 assert result["records"]["REJECT_INCONSISTENT_OWNER_PAYLOAD"]==1
 assert all(x["full_handle"]=="0x8100001f" for x in result["entities"])
 assert result["pixel"]=="OPEN"
 print("V464 synthetic actor handle analyzer PASS (4 cases, no gameplay/PX claims)")
