#!/usr/bin/env python3
"""V441: exact owner and direct-rel32 call census over both retail DSR .text sections.
Imports inherited V440 retail EXE tests. Only direct CALL rel32, not indirect/thunks.
"""
from pathlib import Path
import collections,hashlib,json,struct,sys
from verify_v440_native_channel import PE,verify
from verify_v440_entity_rtti_dispatch import check as rtti
from verify_v440_dispatch_modes import check as dispatch
from verify_v440_alternate_builder import check as asm_builder
from verify_v440_builder_unwind import check as default_builder
TARGET={
 "type5_wrapper":0x1c0be0,"type6_wrapper":0x1c0c10,
 "native_single_packer":0x563b80,"native_blend_packer":0x563c30,
 "three_channel_tick":0x1d7530,"triple_setter":0x1d7580,
 "channel_setter":0x1d7820,"channel_tick":0x1d77e0,
 "model_prepass":0x20fee0,"model_shader_selector":0x22ba20
}
EXPECTED={
 "type5_wrapper":[0x2167ba,0x220639],
 "type6_wrapper":[0x220632],
 "native_single_packer":[0x1c69fe,0x1c6a97,0x1c6d55,0x1c6dee],
 "native_blend_packer":[0x1c6bc5,0x1c6f4a],
 "three_channel_tick":[0x219d8f,0x222d5f],
 "triple_setter":[0xc0240,0x219b2b,0x222bbb],
 "channel_setter":[0x1d75aa,0x1d75c0,0x1d75db,0x1d762d,0x1d7646,0x1d7664,
 0x219cf8,0x223068,0x2d39c0,0x2d39e6,0x2d3a0c,0x2d3a32,0x2d3a58,0x55d05c,0x55d084],
 "channel_tick":[0x1d7545,0x1d7551,0x2d1f8b,0x2d1f9a,0x2d1fb8,0x2d1fc7,0x2d1fa9],
 "model_prepass":[0x210533,0x216c13,0x220839],
 "model_shader_selector":[0x20e014,0x20eb7a,0x20fb99]
}
OWNER_PREIMAGES={
 0x219d79:"4881c108010000",0x222d49:"4881c108010000",
 0x219d94:"488d8bf0010000",0x222d64:"488d8b64010000",
 0x219cc7:"4881c1f0010000",0x223037:"4881c164010000",
 0xc01fb:"488d4b48",
 0x2d39ab:"488d8fb0010000",0x2d39d1:"488d8fc0010000",
 0x2d39f7:"488d8fd0010000",0x2d3a1d:"488d8fe0010000",
 0x2d3a43:"488d8ff0010000",
 0x55d046:"4883c158",0x55d076:"4883c158"
}
SPECIALIZED_RENDER_CALLS={
 0x217913:0x20e9d0,0x217d5d:0x20e9d0,
 0x22119b:0x20e9d0,0x221412:0x20e9d0,
 0x222033:0x20f9f0
}
VIRTUAL_MODEL_CALLS=(0x2172aa,0x220ced)
def census(pe):
 out=collections.defaultdict(list)
 target_by_addr={v:k for k,v in TARGET.items()}
 for name,va,vsize,size,start in pe.sections:
  if name!=".text":continue
  data=memoryview(pe.b)[start:start+size]
  for i in range(len(data)-4):
   if data[i]!=0xe8:continue
   dst=(va+i+5+struct.unpack_from("<i",data,i+1)[0])&0xffffffff
   if dst in target_by_addr:out[target_by_addr[dst]].append(va+i)
 return {k:sorted(out[k]) for k in TARGET}
def check(pe,strict_sha=True):
 if strict_sha:verify(pe)
 rtti(pe,require_sha=False)
 dispatch(pe,require_sha=False)
 asm_builder(pe,require_sha=False)
 default_builder(pe)
 got=census(pe)
 for name,expected in EXPECTED.items():
  assert got[name]==sorted(expected),("direct_calls",name,got[name])
 for r,t in SPECIALIZED_RENDER_CALLS.items():
  assert pe.call_target(r)==t,("specialized",hex(r))
 for r in VIRTUAL_MODEL_CALLS:
  assert pe.read(r,3)==bytes.fromhex("ff5018"),("vcall",hex(r))
 for r,expected in OWNER_PREIMAGES.items():
  v=bytes.fromhex(expected)
  assert pe.read(r,len(v))==v,("semantic_owner",hex(r))
 return {"status":"CONFIRMED_STATIC_DIRECT_CALL_CENSUS",
  "retail_exe_sha256":hashlib.sha256(pe.b).hexdigest(),
  "direct_call_sites":{k:[hex(i) for i in v] for k,v in got.items()},
  "owner_preimages":len(OWNER_PREIMAGES),
  "specialized_render_calls":{hex(r):hex(t) for r,t in SPECIALIZED_RENDER_CALLS.items()},
  "type_checked_virtual_model_calls":[hex(x) for x in VIRTUAL_MODEL_CALLS],
  "indirect_or_obfuscated_edge_census":"OPEN",
  "source_to_concrete_actor_to_issued_RX34":"OPEN","PTDE_pixels":"OPEN"}
if __name__=="__main__":
 print(json.dumps(check(PE(Path(sys.argv[1]).read_bytes())),indent=2))
