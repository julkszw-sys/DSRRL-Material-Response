#!/usr/bin/env python3
"""V440 SHA-locked native producer->channel->model ABI static RE verifier."""
import hashlib,json,struct,sys
from pathlib import Path
from v440_native_opcode_fixtures import OPCODES
EXPECTED="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
BASE=0x140000000
CALLS={
0x1d74b7:0x1d7750,0x1d74c4:0x1d7750,0x1d74d1:0x1d7750,
0x1d75aa:0x1d7820,0x1d75c0:0x1d7820,0x1d75db:0x1d7820,
0x219b2b:0x1d7580,0x222bbb:0x1d7580,
0x220256:0xc04a0,0x22026e:0xc0550,
0x220632:0x1c0c10,0x220639:0x1c0be0,
0x220839:0x20fee0,0x20e014:0x22ba20,
0x1c69fe:0x563b80,0x1c6a97:0x563b80,
0x1c6d55:0x563b80,0x1c6dee:0x563b80,
0x1c6bc5:0x563c30,0x1c6f4a:0x563c30,
0x2167ba:0x1c0be0,
}
EXCLUDED={0x219cf8:0x1d7820,0x223068:0x1d7820}
VTABLE={0x12eca60:0x220000,0x12eb148:0x20dda0,0x12eb778:0x20dda0}
class PE:
 def __init__(self,b):
  self.b=b
  assert b[:2]==b'MZ'
  h=struct.unpack_from("<I",b,0x3c)[0]
  assert b[h:h+4]==b"PE\0\0"
  num=struct.unpack_from("<H",b,h+6)[0]
  hdr=struct.unpack_from("<H",b,h+20)[0]
  self.sections=[]
  for i in range(num):
   at=h+24+hdr+i*40
   size,va,raw,offset=struct.unpack_from("<IIII",b,at+8)
   self.sections.append((va,raw,offset))
 def read(self,r,n):
  for va,raw,offset in self.sections:
   if va<=r<va+raw:return self.b[offset+r-va:offset+r-va+n]
  raise ValueError("RVA outside sections: "+hex(r))
 def offset(self,r):
  for va,raw,offset in self.sections:
   if va<=r<va+raw:return offset+r-va
  raise ValueError(hex(r))
def verify(b):
 h=hashlib.sha256(b).hexdigest()
 assert h==EXPECTED,"wrong owner retail EXE SHA"
 p=PE(b)
 for r,code in OPCODES.items():
  x=bytes.fromhex(code)
  assert p.read(r,len(x))==x,("opcode",hex(r))
 for r,dest in {**CALLS,**EXCLUDED}.items():
  raw=p.read(r,5);assert raw[0]==0xe8,("not rel32 call",hex(r))
  assert r+5+struct.unpack("<i",raw[1:])[0]==dest,("CALL wrong target",hex(r))
 for r,dest in VTABLE.items():
  assert struct.unpack("<Q",p.read(r,8))[0]-BASE==dest,("vtable",hex(r))
 return {"static_abi":"CONFIRMED","source_sha256":h,
 "opcode_witnesses":len(OPCODES),"direct_call_edges":len(CALLS),
 "other_object_edges_excluded":len(EXCLUDED),"typed_vtable_slots":len(VTABLE),
 "runtime":"OPEN","native_emitter_to_actor_join":"OPEN","pixel":"OPEN"}
def main():
 if len(sys.argv)<2:raise SystemExit("usage: python verify_v440_native_channel.py <retail_DSR_exe>")
 data=verify(Path(sys.argv[1]).read_bytes())
 print(json.dumps(data,indent=2))
if __name__=="__main__":main()
