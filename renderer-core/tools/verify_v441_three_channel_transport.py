#!/usr/bin/env python3
"""V441 original channel2 and channel3 CPU context operator trace; no GPU/PTE inference."""
import struct,json,sys
from pathlib import Path
from verify_v440_native_channel import PE,verify
from verify_v440_entity_rtti_dispatch import check as rtti
CALLS={0x2167cc:0x1c0c40,0x220647:0x1c0c40,
 0x2167de:0x1c0c60,0x220655:0x1c0c60,
 0x1c8717:0x1c8b00,0x1c8707:0x1c8bb0,0x1c8847:0x1c8b00}
JUMPS={0x1c0c4b:0x1c8650,0x1c0c6b:0x1cac50}
BYTES={0x1c0c40:"4d8bc84c8b4140488b4968",
 0x1c0c60:"4d8bc84c8b4140488b4970",
 0x1c8677:"498d4918",0x1c86a4:"39af8c220000",
 0x1c86c8:"41b802000000",0x1c871c:"89af8c220000",
 0x1c873c:"f3410f10761c",0x1cac7d:"498d4928",
 0x1caca7:"41399f90220000",0x1caccc:"41b807000000",
 0x1cad9f:"41899f90220000",0x1cadb3:"f3410f10752c",
 0x1c8bda:"0f29a290020000",0x1c8ba1:"0f2987f0120000",
 0x1c8c0b:"0f29a290020000"}
OFF={0x1cad1f:0x1320,0x1cad27:0x1330,0x1cad2f:0x1340,
 0x1cad37:0x1350,0x1cad3f:0x1360,0x1cad47:0x1370,
 0x1cad4f:0x2c0,0x1cad57:0x2d0,0x1cad5f:0x2e0,
 0x1cad67:0x2f0,0x1cad6f:0x300,0x1cad77:0x310,
 0x1cad7f:0x1300,0x1cad87:0x1310,
 0x1cad8f:0x2a0,0x1cad97:0x2b0}
def rel(p,r,opcode):
 b=p.read(r,5);assert b[0]==opcode
 return r+5+struct.unpack("<i",b[1:])[0]
def check(blob,strict_sha=True):
 if strict_sha:verify(blob)
 rtti(blob,sha=False);p=PE(blob)
 for r,t in CALLS.items():assert rel(p,r,0xe8)==t,hex(r)
 for r,t in JUMPS.items():assert rel(p,r,0xe9)==t,hex(r)
 for r,h in BYTES.items():assert p.read(r,len(bytes.fromhex(h)))==bytes.fromhex(h),hex(r)
 for r,t in {**OFF,**{r+0x157:o for r,o in OFF.items()}}.items():
  b=p.read(r,8)
  assert b[:3]==bytes.fromhex("410f29") and b[3]&0xc7==0x87,hex(r)
  assert struct.unpack_from("<i",b,4)[0]==t,hex(r)
 return {"channels":"CONFIRMED_STATIC","class_count":2,
  "second":{"dp":"actor+0x108+0x18","wrapper":"0x1c0c40",
    "consumer":"0x1c8650","resource_category":2,
    "ctx_cached_selector":"+0x228c","ctx_output":["+0x290","+0x12f0"]},
  "third":{"dp":"actor+0x108+0x28","wrapper":"0x1c0c60",
    "consumer":"0x1cac50","resource_category":7,
    "ctx_cached_selector":"+0x2290",
    "ctx_output_vec_A":"+0x2a0..0x310","ctx_output_vec_B":"+0x1300..0x1370",
    "verified_simd_store_sites":32},
  "gpu_constant_or_srv_consumer":"OPEN","PTDE_target":"OPEN",
  "pixel_causality":"OPEN"}
if __name__=="__main__":
 print(json.dumps(check(Path(sys.argv[1]).read_bytes()),indent=2))
