#!/usr/bin/env python3
"""Exact original DSR MSVC RTTI: ModelDrawEntity embedded in two FX outer classes."""
import struct,json,sys
from pathlib import Path
from verify_v440_native_channel import PE,verify
BASE=0x140000000
OUTER={
 "FrpgFxClusterAppearance_Model":(0x136c0d0,0x165fbe0,0x1b107a8,0x50,0x4fd3cc,0x4fd3de,0x4fc09a,0x4fc09e),
 "FrpgFxParticleAppearance_Model":(0x136c208,0x165fd10,0x1b10850,0x30,0x4fe7ec,0x4fe7fe,0x4fdc7a,0x4fdc7e)
}
def typ(p,r):return p.read(r+16,120).split(b"\0")[0].decode("ascii")
def target(p,r):
 b=p.read(r,5);assert b[0]==0xe8
 return r+5+struct.unpack("<i",b[1:])[0]
def check(blob,strict_sha=True):
 if strict_sha:verify(blob)
 p=PE(blob);res={}
 for name,(vt,col,td,sub,cl,cc,dl,dc) in OUTER.items():
  assert struct.unpack("<Q",p.read(vt-8,8))[0]==BASE+col
  sig,off,cd,typ_r,chd,self_ref=struct.unpack("<6I",p.read(col,24))
  assert (sig,off,cd,typ_r,self_ref)==(1,0,0,td,col)
  assert typ(p,td)==".?AV"+name+"@NS_FRPG@@"
  count,arr=struct.unpack("<II",p.read(chd+8,8));assert count==3
  base_names=[]
  for i in range(3):
   r=struct.unpack("<I",p.read(arr+i*4,4))[0]
   d=struct.unpack("<I",p.read(r,4))[0]
   base_names.append(typ(p,d))
  assert name in base_names[0] and "FX" in base_names[1] and "FX" in base_names[2]
  for r in (cl,dl):
   b=p.read(r,4)
   assert b[:3] in (bytes.fromhex("498d4e"),bytes.fromhex("488d4f")) and b[3]==sub
  assert target(p,cc)==0x21f900 and target(p,dc)==0x21fbd0
  res[name]={"outer_vtable":hex(vt),"inner_model_offset":hex(sub),
   "inner_drawparam_offset_from_parent":hex(sub+0x108),
   "inner_model_pointer_offset_from_parent":hex(sub+0x40),
   "base_types":base_names}
 return {"typed_nested_VFX_model":"CONFIRMED_STATIC","parents":res,
         "runtime_source_generation":"OPEN","armor_mapping":"OPEN","pixel":"OPEN"}
if __name__=="__main__":print(json.dumps(check(Path(sys.argv[1]).read_bytes()),indent=2))
