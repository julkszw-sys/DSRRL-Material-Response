#!/usr/bin/env python3
"""V440 verified Win64 chained .pdata: same typed builder ancestor for native
LightBank emitter call and virtual model-draw, not a runtime/pixel certificate."""
import bisect,json,struct,sys
from pathlib import Path
from verify_v440_native_channel import PE,verify
ROOT=(0x2200d0,0x2200fe,0x174f4e8)
EMITTER=[
 (0x2204c3,0x22087a,0x174f560),
 (0x22012a,0x220450,0x174f534),
 (0x220107,0x22012a,0x174f51c),
 (0x2200fe,0x220107,0x174f508),ROOT]
DRAW=[(0x220c51,0x220d50,0x174f5c8),ROOT]
MODEL=[
 (0x20de68,0x20e652,0x174b548),
 (0x20ddc5,0x20de68,0x174b51c),
 (0x20dda0,0x20ddc5,0x174b508)]
def check(b):
 verify(b);p=PE(b)
 sections=[x for x in p.sections if x[0]==0x1d0b000]
 assert len(sections)==1
 va,raw,off=sections[0]
 entries=[struct.unpack_from("<III",b,off+i*12) for i in range(raw//12)]
 starts=[x[0] for x in entries]
 def entry(pc):
  idx=bisect.bisect_right(starts,pc)-1
  assert idx>=0 and entries[idx][0]<=pc<entries[idx][1],hex(pc)
  return entries[idx]
 def chain(pc):
  out=[entry(pc)]
  while True:
   assert len(out)<=8
   u=p.read(out[-1][2],70)
   assert (u[0]&7)==1
   flags=u[0]>>3
   assert flags in (3,4),hex(out[-1][2])
   if flags==3:return out
   at=4+((u[2]+1)&~1)*2
   parent=struct.unpack_from("<III",u,at)
   assert parent not in out
   out.append(parent)
 for pc in (0x220637,0x22063e,0x22083e):
  assert chain(pc)==EMITTER,hex(pc)
 for pc in (0x220ced,0x220cf0):
  assert chain(pc)==DRAW,hex(pc)
 for pc in (0x20df86,0x20e014,0x20e019):
  assert chain(pc)==MODEL,hex(pc)
 assert p.read(0x174f4e8,1)[0]>>3==3
 assert p.read(0x174b508,1)[0]>>3==3
 for rva,hexcode in [
  (0x2200d0,"4c8bdc"),(0x2200da,"498dabc8fdffff"),
  (0x220ce6,"488d55d0"),(0x220614,"488d9f08010000"),
  (0x20dde4,"4c8bfa")]:
  assert p.read(rva,len(bytes.fromhex(hexcode)))==bytes.fromhex(hexcode)
 return {
  "native_source_builder_root":"0x2200d0",
  "emitter_to_builder_pdata_levels":len(EMITTER),
  "virtual_draw_to_builder_pdata_levels":len(DRAW),
  "selector_to_model_pdata_levels":len(MODEL),
  "packet":"builder_RBP_minus_0x30",
  "actor":"builder_RDI",
  "candidate_invocation_key":"thread,canonical_builder_frame,packet,actor,generation",
  "original_SHA_unwind_static":"CONFIRMED",
  "runtime_win64_unwind":"OPEN",
  "per_issued_RX34_owner_join":"OPEN",
  "PTDE_pixel":"OPEN"}
if __name__=="__main__":
 print(json.dumps(check(Path(sys.argv[1]).read_bytes()),indent=2))
