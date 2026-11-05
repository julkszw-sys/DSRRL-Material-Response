#!/usr/bin/env python3
"""V441-R2 exact direct call census, using the inherited V440 PE ABI."""
import sys,struct,json,collections
from pathlib import Path
from verify_v440_native_channel import PE,verify
from verify_v440_entity_rtti_dispatch import check as rtti
from verify_v440_dispatch_modes import check as dispatch
from verify_v440_alternate_builder import check as asm
from verify_v440_builder_unwind import check as builder
TARGET={
"type5":0x1c0be0,"type6":0x1c0c10,"single":0x563b80,"blend":0x563c30,
"tick_all":0x1d7530,"set_all":0x1d7580,"set_one":0x1d7820,
"tick_one":0x1d77e0,"prepass":0x20fee0,"shader_selector":0x22ba20}
EXPECTED={
"type5":[0x2167ba,0x220639],"type6":[0x220632],
"single":[0x1c69fe,0x1c6a97,0x1c6d55,0x1c6dee],
"blend":[0x1c6bc5,0x1c6f4a],
"tick_all":[0x219d8f,0x222d5f],
"set_all":[0xc0240,0x219b2b,0x222bbb],
"set_one":[0x1d75aa,0x1d75c0,0x1d75db,0x1d762d,0x1d7646,0x1d7664,
0x219cf8,0x223068,0x2d39c0,0x2d39e6,0x2d3a0c,0x2d3a32,0x2d3a58,0x55d05c,0x55d084],
"tick_one":[0x1d7545,0x1d7551,0x2d1f8b,0x2d1f9a,0x2d1fa9,0x2d1fb8,0x2d1fc7],
"prepass":[0x210533,0x216c13,0x220839],
"shader_selector":[0x20e014,0x20eb7a,0x20fb99]}
OWNERS={0x219d79:"4881c108010000",0x222d49:"4881c108010000",
0x219d94:"488d8bf0010000",0x222d64:"488d8b64010000",
0x219cc7:"4881c1f0010000",0x223037:"4881c164010000",
0xc01fb:"488d4b48",0x2d39ab:"488d8fb0010000",0x2d39d1:"488d8fc0010000",
0x2d39f7:"488d8fd0010000",0x2d3a1d:"488d8fe0010000",
0x2d3a43:"488d8ff0010000",0x55d046:"4883c158",0x55d076:"4883c158"}
SPECIAL={0x217913:0x20e9d0,0x217d5d:0x20e9d0,0x22119b:0x20e9d0,
0x221412:0x20e9d0,0x222033:0x20f9f0}
def target(p,r):
 b=p.read(r,5);assert b[0]==0xe8,hex(r)
 return r+5+struct.unpack("<i",b[1:])[0]
def check(blob,strict_sha=True):
 if strict_sha:verify(blob)
 rtti(blob,sha=False);dispatch(blob,sha=False);asm(blob,sha=False)
 builder(blob,verify_source=False)
 p=PE(blob)
 seen=collections.defaultdict(list)
 rev={v:k for k,v in TARGET.items()}
 for va,length,off in p.sections:
  if va not in (0x1000,0x2019000):continue
  data=memoryview(blob)[off:off+length]
  for pos in range(len(data)-4):
   if data[pos]==0xe8:
    t=va+pos+5+struct.unpack_from("<i",data,pos+1)[0]
    if t in rev:seen[rev[t]].append(va+pos)
 for k,v in EXPECTED.items():assert seen[k]==sorted(v),(k,seen[k])
 for r,v in SPECIAL.items():assert target(p,r)==v,hex(r)
 for r in (0x2172aa,0x220ced):assert p.read(r,3)==bytes.fromhex("ff5018")
 for r,v in OWNERS.items():assert p.read(r,len(bytes.fromhex(v)))==bytes.fromhex(v),hex(r)
 return {"direct_rel32_exact":"CONFIRMED","call_counts":{k:len(v) for k,v in seen.items()},
  "owner_preimages":len(OWNERS),"specialized_model_calls":len(SPECIAL),
  "runtime_indirect_calls":"OPEN","actor_generation":"OPEN","pixel":"OPEN"}
if __name__=="__main__":
 print(json.dumps(check(Path(sys.argv[1]).read_bytes()),indent=2))
