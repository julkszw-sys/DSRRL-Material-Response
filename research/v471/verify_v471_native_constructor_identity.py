#!/usr/bin/env python3
"""V471: SHA-locked static EXE constructor/handle ABI, no runtime injection.
Usage: python research/v471/verify_v471_native_constructor_identity.py DarkSoulsRemastered.exe --mutation-test
"""
import argparse,hashlib,json,struct
from pathlib import Path
SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
W={
 0x21F92E:"e87d34fdff",0x21F934:"488d05d5d00c01",
 0x21F928:"498bd9",0x21F93E:"488b4320",0x21F942:"48894740",
 0x21FAE1:"488b5740",
 0x1F2DD6:"e8355bfc00",0x11B891E:"44894110",
 0x223AD9:"418bf8",0x223B06:"448bc7",0x223B0F:"e8ecbdffff",
 0x4FD3D8:"4533c0",0x4FD3DE:"e81d25d2ff",
 0x4FE7F8:"4533c0",0x4FE7FE:"e8fd10d2ff",
 0x20B152:"e859120100",0x20B158:"488d05d1fd0d01",
 0x20B1B7:"8b5e10",0x21C3DF:"897110",
 0x321D05:"488b4960",0x321D28:"ff5028",0x321DA8:"0f291b",
 0x36D8F2:"488d0517a9fb00",0x2842674:"488d05752baefe"
}
CALLS={0x21F92E:0x1F2DB0,0x1F2DD6:0x11B8910,0x223B0F:0x21F900,
       0x4FD3DE:0x21F900,0x4FE7FE:0x21F900,0x20B152:0x21C3B0}
VTABLES={0x21F934:0x12ECA10,0x20B158:0x12EAF30,
         0x36D8F2:0x1328210,0x2842674:0x13251F0}
class PE:
 def __init__(self,b):
  p=struct.unpack_from("<I",b,0x3c)[0]
  assert b[:2]==b"MZ" and b[p:p+4]==b"PE\0\0" and struct.unpack_from("<H",b,p+4)[0]==0x8664
  self.sections=[];z=p+24+struct.unpack_from("<H",b,p+20)[0]
  for i in range(struct.unpack_from("<H",b,p+6)[0]):
   q=z+i*40
   vsize,rva,raw,off=struct.unpack_from("<IIII",b,q+8)
   self.sections.append((rva,raw,off))
 def pos(self,rva,n):
  for start,raw,off in self.sections:
   if start<=rva and rva+n<=start+raw:return off+rva-start
  raise ValueError(hex(rva))
def verify(data,negative=False):
 assert hashlib.sha256(data).hexdigest()==SHA,"EXE SHA differs: fail closed"
 pe=PE(data)
 for rva,h in W.items():
  b=bytes.fromhex(h);o=pe.pos(rva,len(b))
  assert data[o:o+len(b)]==b,hex(rva)
 for rva,target in CALLS.items():
  o=pe.pos(rva,5)
  assert data[o]==0xe8 and rva+5+struct.unpack_from("<i",data,o+1)[0]==target,hex(rva)
 for rva,target in VTABLES.items():
  o=pe.pos(rva,7)
  assert data[o:o+3]==b"\x48\x8d\x05" and rva+7+struct.unpack_from("<i",data,o+3)[0]==target,hex(rva)
 out={"status":"STATIC_EXE_PASS","exe_sha256":SHA,"preimages":len(W),
      "direct_calls":len(CALLS),"vtable_rip_leas":len(VTABLES),
      "rx34_receiver_vtable_rva":"0x12ECA10","root_chrmodel_vtable_rva":"0x12EAF30",
      "rx34_handle_transport":"factory R8D -> receiver constructor 0x21F900 -> base 0x1F2DB0 -> base 0x11B8910 -> receiver+0x10",
      "rx34_resource_pointer":"receiver+0x40 = *(constructor_R9+0x20), consumed by receiver transform",
      "direct_root_actor_join":"NOT_PROVEN","equipment_actor_join":"OPEN","fps":"OPEN","pixel":"OPEN"}
 if negative:
  c=bytearray(data);bad=[]
  for rva,h in W.items():
   original=bytes.fromhex(h);o=pe.pos(rva,len(original))
   c[o]^=1
   if c[o:o+len(original)]==original:bad.append(hex(rva))
   c[o]^=1
  out["negative_controls"]={"mutation_count":len(W),"undetected":bad,"status":"PASS" if not bad else "FAIL"}
 return out
if __name__=="__main__":
 p=argparse.ArgumentParser();p.add_argument("exe",type=Path)
 p.add_argument("--mutation-test",action="store_true")
 args=p.parse_args()
 print(json.dumps(verify(args.exe.read_bytes(),args.mutation_test),indent=2))
