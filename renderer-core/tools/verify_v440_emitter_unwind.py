#!/usr/bin/env python3
"""V440 original Win64 pdata static witness; no runtime hook."""
import bisect,struct,sys,json
from pathlib import Path
from verify_v440_native_channel import PE,verify
EMITTERS={
"type5":((0x1c6900,0x1c6aca,0x173ffe8),(0x1c6aca,0x1c6be1,0x174000c),14,[0x1c69fe,0x1c6a97],0x1c6bc5),
"type6":((0x1c6c40,0x1c6e21,0x1740034),(0x1c6e21,0x1c6f66,0x1740058),15,[0x1c6d55,0x1c6dee],0x1c6f4a),
}
def check(b):
 p=PE(b);verify(b)
 pe=struct.unpack_from("<I",b,0x3c)[0]
 n=struct.unpack_from("<H",b,pe+6)[0];h=struct.unpack_from("<H",b,pe+20)[0]
 pd=None
 for i in range(n):
  o=pe+24+h+40*i
  if b[o:o+6]==b".pdata":
   raw=struct.unpack_from("<I",b,o+16)[0]
   fileoff=struct.unpack_from("<I",b,o+20)[0]
   pd=[struct.unpack_from("<III",b,fileoff+k*12) for k in range(raw//12)]
 assert pd
 starts=[x[0] for x in pd]
 def owner(pc):
  i=bisect.bisect_right(starts,pc)-1
  v=pd[i];assert v[0]<=pc<v[1]
  return v
 for label,(parent,child,reg,singles,blend) in EMITTERS.items():
  for pc in singles:assert owner(pc)==parent,(label,hex(pc))
  assert owner(blend)==child
  u=p.read(parent[2],48)
  assert u[0]&7==1 and u[0]>>3==3
  assert any((u[5+i*2]&15)==0 and (u[5+i*2]>>4)==reg for i in range(u[2]))
  v=p.read(child[2],48)
  assert v[0]&7==1 and v[0]>>3==4
  at=4+((v[2]+1)&~1)*2
  assert struct.unpack_from("<III",v,at)==parent
 return {"static_original_nonvol_registers":"PASS","type5":"R14","type6":"R15",
         "blend_chained_unwind":"PASS","runtime_stack_unwind":"OPEN","pixels":"OPEN"}
if __name__=="__main__":
 print(json.dumps(check(Path(sys.argv[1]).read_bytes()),indent=2))
