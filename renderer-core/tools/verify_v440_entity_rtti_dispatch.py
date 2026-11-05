#!/usr/bin/env python3
"""V440-R4 owner retail MSVC RTTI: sibling actor draw entities, shared bases."""
import json,struct,sys
from pathlib import Path
from verify_v440_native_channel import PE,verify
BASE=0x140000000
TYPES={
"asm":(0x12ebfe8,0x163d2f8,0x163d320,0x163d338,0x1af9920,0x2161f0,"FrpgAsmModelDrawEntity"),
"model":(0x12eca10,0x163d738,0x163d760,0x163d778,0x1af9ba0,0x220000,"FrpgModelDrawEntity")}
def name(p,r):
 return p.read(r+16,150).split(b"\0")[0].decode("ascii")
def check(b,sha=True):
 if sha:verify(b)
 p=PE(b);out={}
 for k,(vt,col,chd,bca,td,dispatch,klass) in TYPES.items():
  assert struct.unpack("<Q",p.read(vt-8,8))[0]==BASE+col
  assert struct.unpack("<Q",p.read(vt+0x50,8))[0]==BASE+dispatch
  assert struct.unpack("<6I",p.read(col,24))==(1,0,0,td,chd,col)
  assert name(p,td)==".?AV"+klass+"@NS_FRPG@@"
  assert struct.unpack("<4I",p.read(chd,16))==(0,0,4,bca)
  brs=[];names=[]
  for i in range(4):
   r=struct.unpack("<I",p.read(bca+4*i,4))[0]
   d=struct.unpack("<7I",p.read(r,28))
   assert tuple(d[2:5])==(0,0xffffffff,0) and d[5]==0x40
   brs.append(r);names.append(name(p,d[0]))
  assert names==[".?AV"+klass+"@NS_FRPG@@",
    ".?AVFrpgDrawEntity@NS_FRPG@@",
    ".?AVHGDrawEntity@DLHG@@",
    ".?AVHGEntity@DLHG@@"]
  out[k]={"class":klass,"vtable_RVA":hex(vt),"dispatch_RVA":hex(dispatch),
          "base_descriptors":list(map(hex,brs))}
 assert out["asm"]["base_descriptors"][1:]==out["model"]["base_descriptors"][1:]
 return {"original_MSVC_RTTI":"CONFIRMED","relationship":"SIBLING_CLASSES",
  "shared_base":"FrpgDrawEntity","class_a":out["asm"],"class_b":out["model"],
  "runtime_actor_equipment_identity":"OPEN","pixel":"OPEN"}
if __name__=="__main__":print(json.dumps(check(Path(sys.argv[1]).read_bytes()),indent=2))
