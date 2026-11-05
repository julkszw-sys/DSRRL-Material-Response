#!/usr/bin/env python3
"""V440-R3 alternative native type5 packet and model virtual dispatch evidence."""
import bisect,json,struct,sys
from pathlib import Path
from verify_v440_native_channel import PE,verify
ROOT=(0x2162d0,0x217363,0x174d464)
OP={
0x2162d0:"488bc4",0x2162e2:"488da898fdffff",0x21631a:"488bf9",
0x21639c:"4c897520",0x2163a0:"48895528",
0x216414:"488d8f10010000",0x21641e:"e87da0eaff",
0x216423:"488d8f10010000",0x21642d:"6689556c",
0x216436:"e815a1eaff",0x21643b:"f30f108714010000",
0x21644e:"f30f114570",0x21647d:"66894d6e",
0x2167ad:"4c8d8708010000",0x2167ba:"e821a4faff",
0x216bd7:"488d5520",0x216c13:"e8c892ffff",
0x21729c:"48894528",0x2172a3:"488b0c33",
0x2172a7:"488b01",0x2172aa:"ff5018"}
EDGES={0x21641e:0xc04a0,0x216436:0xc0550,0x2167ba:0x1c0be0,0x216c13:0x20fee0}
def check(b,sha=True):
 if sha:verify(b)
 p=PE(b)
 for r,x in OP.items():assert p.read(r,len(bytes.fromhex(x)))==bytes.fromhex(x),hex(r)
 for r,d in EDGES.items():
  v=p.read(r,5)
  assert v[0]==0xe8 and r+5+struct.unpack("<i",v[1:])[0]==d,hex(r)
 section=[x for x in p.sections if x[0]==0x1d0b000]
 assert len(section)==1
 va,raw,offset=section[0]
 pdata=[struct.unpack_from("<III",b,offset+12*i) for i in range(raw//12)]
 starts=[x[0] for x in pdata]
 for pc in (0x2167ba,0x2167bf,0x216c13,0x2172aa,0x2172ad):
  n=bisect.bisect_right(starts,pc)-1
  assert n>=0 and pdata[n]==ROOT,hex(pc)
 u=p.read(ROOT[2],4)
 assert u[0]&7==1 and u[0]>>3==3
 return {"static_alt_builder":"CONFIRMED",
 "root":"0x2162d0","packet":"RBP+0x20",
 "actor":"RDI","source_dp":"actor+0x108",
 "packet_A_B_beta":["+0x4c","+0x4e","+0x50"],
 "type5_wrapper":"0x2167ba","virtual_model_draw":"0x2172aa",
 "prepass":"0x216c13",
 "exact_opcodes":len(OP),"exact_direct_calls":len(EDGES),
 "dynamic_model_identity":"OPEN","live_owner":"OPEN","issued_RX34":"OPEN","pixels":"OPEN"}
if __name__=="__main__":
 print(json.dumps(check(Path(sys.argv[1]).read_bytes()),indent=2))
