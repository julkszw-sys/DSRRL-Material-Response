#!/usr/bin/env python3
"""V442-R2 SHA-locked original DSR per-submodel indexed transform palette ABI."""
import json,sys
from pathlib import Path
from verify_v442_context_multireceiver import PE,check as verify_original
W={
0x1c09b0:"410fb70e",0x1c09b9:"0fbfc1",0x1c09bc:"3b4708",
0x1c09c1:"488b4710",0x1c09c5:"480fbfc9",0x1c09c9:"0fb71448",
0x1c09cf:"83caff",0x1c09de:"4c8d0440",0x1c09e2:"49c1e004",
0x1c09e6:"4d034710",0x1c09ea:"e84136fdff",
0x1c09ef:"4c8d442420",0x1c09fb:"e8f0140000",
0x1c0a02:"4983c602",0x1c0a06:"4883c122",0x1c0a0a:"83c303",
0x1c0a0d:"48c1e104",0x1c0a14:"4903cc",
0x1c0a17:"0f1101",0x1c0a1e:"0f114910",0x1c0a26:"0f114120",
0x1c0a4f:"0fbfc1",0x1c0a52:"3b4708",0x1c0a57:"488b4710",
0x1c0a7d:"49034f10",0x1c0a81:"e86a140000",
0x194030:"4053",0x19403f:"e83c0e0000",
0x194e94:"0f5902",0x194ead:"0f5801",0x194eb3:"0f595220",
0x1c1f90:"0f2802",0x1c1f96:"410f5800",
0x1c1fa1:"410f584810",0x1c1fae:"410f584020"}
CALL={0x1c09ea:0x194030,0x1c09fb:0x1c1ef0,
 0x1c0a81:0x1c1ef0,0x19403f:0x194e80,0x1c1eff:0x1c1f90}
def check(data,sha=True):
 if sha:verify_original(data)
 p=PE(data)
 for r,h in W.items():assert p.read(r,len(bytes.fromhex(h)))==bytes.fromhex(h),("witness",hex(r))
 for r,t in CALL.items():assert p.dest(r)==t,("call",hex(r))
 return {"static":"CONFIRMED","native_operator":"0x1c08e0",
 "descriptor_index":"word [descriptor+6+2*i]",
 "index_bound":"dword [remap+0x8]",
 "remap":"[remap+0x10]+2*index",
 "source":"[input+0x10] + 0x30*mapped_index",
 "matrix_math":"0x194030 -> 0x194e80 (mulps/addps/shufps)",
 "vector_math":"0x1c1ef0 -> 0x1c1f90 (three addps)",
 "destination":"FrpgDrawContext+0x3e0+0x30*i",
 "semantic":"indexed 3x4 transform palette, not LightBank row",
 "shader_consumption":"OPEN","flicker_root_cause":"OPEN","PTDE_pixels":"OPEN"}
def negatives(data):
 p=PE(data);sites=list(W)[:28]+list(CALL)[:4]
 for r in sites:
  corrupt=bytearray(data);corrupt[p.off(r)]^=1
  try:check(bytes(corrupt),sha=False)
  except AssertionError:continue
  raise AssertionError("mutation escaped "+hex(r))
 return len(sites)
if __name__=="__main__":
 b=Path(sys.argv[1]).read_bytes()
 print(json.dumps(check(b),indent=2))
 print("V442_transform_semantic_mutation_negatives",negatives(b))
