#!/usr/bin/env python3
"""V441 typed entity ctor/dtor and DrawParam lifetime; static original SHA only."""
from pathlib import Path
import struct,sys,json
from verify_v440_native_channel import PE,verify
from verify_v440_entity_rtti_dispatch import check as rtti
BASE=0x140000000
CLASSES={
 "ASM":{"vt":0x12ebfe8,"ctor":0x215960,"dtor":0x215d20,
 "del":0x21aa30,"size":0x250,"c_lea":0x215992,"c_store":0x215999,
 "m_store":0x2159a0,"dp_c_lea":0x215a22,"dp_c_call":0x215a29,
 "d_lea":0x215d46,"d_store":0x215d4d,"dp_d_lea":0x215de6,
 "dp_d_call":0x215ded,"base_vt":0x215e35,"del_call":0x21aa3f,"del_size":0x21aa49},
 "MODEL":{"vt":0x12eca10,"ctor":0x21f900,"dtor":0x21fbd0,
 "del":0x2237e0,"size":0x1c0,"c_lea":0x21f934,"c_store":0x21f93b,
 "m_store":0x21f942,"dp_c_lea":0x21f9be,"dp_c_call":0x21f9c5,
 "d_lea":0x21fbe2,"d_store":0x21fbe9,"dp_d_lea":0x21fbf9,
 "dp_d_call":0x21fc00,"base_vt":0x21fc55,"del_call":0x2237ef,"del_size":0x2237f9}}
PREIMAGES={0x215999:"488907",0x2159a0:"48894740",0x215a22:"488d8f08010000",
0x215d4d:"488901",0x215de6:"488d8f08010000",0x215e35:"488d052c4a0901",
0x21aa49:"ba50020000",0x21f93b:"488907",0x21f942:"48894740",
0x21f9be:"488d8f08010000",0x21fbe9:"488901",
0x21fbf9:"488d8b08010000",0x21fc55:"488d050cac0801",0x2237f9:"bac0010000"}
def call(p,r):
 b=p.read(r,5);assert b[0]==0xe8
 return r+5+struct.unpack("<i",b[1:])[0]
def rip_lea(p,r):
 b=p.read(r,7);assert b[:3]==bytes.fromhex("488d05")
 return r+7+struct.unpack("<i",b[3:])[0]
def check(blob,strict_sha=True):
 if strict_sha:verify(blob)
 rtti(blob,sha=False);p=PE(blob)
 for r,h in PREIMAGES.items():assert p.read(r,len(bytes.fromhex(h)))==bytes.fromhex(h),hex(r)
 out={}
 for name,k in CLASSES.items():
  assert struct.unpack("<Q",p.read(k["vt"]+8,8))[0]==BASE+k["del"]
  assert rip_lea(p,k["c_lea"])==k["vt"] and rip_lea(p,k["d_lea"])==k["vt"]
  assert rip_lea(p,k["base_vt"])==0x12aa868
  assert call(p,k["dp_c_call"])==0x1d7490
  assert call(p,k["dp_d_call"])==0x1d74f0
  assert call(p,k["del_call"])==k["dtor"]
  assert int.from_bytes(p.read(k["del_size"]+1,4),"little")==k["size"]
  out[name]={"constructor":hex(k["ctor"]),"destructor":hex(k["dtor"]),
   "scalar_deleting_destructor":hex(k["del"]),"bytes":k["size"],
   "embedded_drawparam":"actor+0x108","model_ptr":"actor+0x40"}
 return {"typed_lifecycle":"CONFIRMED_STATIC","classes":out,
   "runtime_actor_generation":"OPEN","same_issued_RX34":"OPEN","pixel":"OPEN"}
if __name__=="__main__":
 print(json.dumps(check(Path(sys.argv[1]).read_bytes()),indent=2))
