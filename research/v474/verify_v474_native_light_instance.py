#!/usr/bin/env python3
"""V474: immutable, original DSR retail EXE / x64 instruction-byte witness.
No patching, injection, disassembly dependencies or runtime test.
Only an exact SHA-matching original EXE is valid. Do not confuse a
registered FrpgModel handle with the independent Seed resource key.
"""
import argparse,hashlib,struct,json
from pathlib import Path
SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
# Code RVA : exact opcode (independent ordered controls)
W={
 0x21CAA1:"8b5310",             # FrpgModel+0x10 recipient
 0x21CAC7:"e8f45f0000",       # call 0x222AC0 -> emit 0x804C
 0x222AD2:"ba4c800000",       # id 0x804c (NOT row index)
 0x222AE0:"e82b0cf900",       # packet target formatter
 0x223386:"8d82ba7fffff",     # dispatcher normalizes 0x8046
 0x223391:"83f812",           # case range
 0x223466:"498bd0",           # exact jump-table case+6
 0x22346C:"e8eff6ffff",       # same entity receiver decoder
 0x222BA7:"498d8b08010000",   # receiver+0x108 embedded DrawParam
 0x222BBB:"e8c049fbff",       # three-lane setter 0x1D7580
 0x1D7592:"4883c108",         # dp+0x8 first Light lane
 0x1D75AF:"488d4f18",         # dp+0x18 second channel
 0x1D75C5:"0fb7442460",       # third channel input
 0x1D75CA:"488d4f28",         # dp+0x28 third channel
 0x1D759F:"488d542450",       # pointer to packed 16-bit selector
 0x1D782F:"440fb74102",       # old target, WORD
 0x1D7834:"0fb702",           # new target, WORD
 0x1D7848:"663901",           # old current, WORD
 0x220614:"488d9f08010000",   # same builder this=RDI -> dp+0x108
 0x220632:"e8d905faff",       # native category5/6 depending mode
 0x220639:"e8a205faff",
 0x1C0BE0:"4883ec38",         # category5 bridge to native emitter
 0x1C0BED:"4c8b4140",         # read model+0x40 from native packet? producer
 0x1C0BF1:"488b4960",         # native renderer ctx+0x60
 0x21F934:"488d05d5d00c01",  # receiver vtable RVA12ECA10
 0x21F9BE:"488d8f08010000",   # receiver ctor initializes same DP
 0x21F9C5:"e8c67afbff",
 0x223B06:"448bc7",           # native factory handle R8D
 0x223B0F:"e8ecbdffff",
 0x11B891E:"44894110",        # distinct receiver+0x10 registered handle
}
CALLS={
 0x21CAC7:0x222AC0,0x222AE0:0x11B3710,
 0x22346C:0x222B60,0x222BBB:0x1D7580,
 0x220632:0x1C0C10,0x220639:0x1C0BE0,
 0x21F9C5:0x1D7490,0x223B0F:0x21F900,
}
class PE:
 def __init__(self,b):
  assert b[:2]==b"MZ"
  p=struct.unpack_from("<I",b,0x3c)[0]
  assert b[p:p+4]==b"PE\0\0"
  assert struct.unpack_from("<H",b,p+4)[0]==0x8664
  self.sections=[];so=p+24+struct.unpack_from("<H",b,p+20)[0]
  for i in range(struct.unpack_from("<H",b,p+6)[0]):
   q=so+40*i;vs,va,raw,off=struct.unpack_from("<IIII",b,q+8)
   self.sections.append((va,raw,off))
 def at(self,rva,n):
  for start,size,off in self.sections:
   if start<=rva and rva+n<=start+size:return off+rva-start
  raise ValueError(f"unmapped RVA {rva:x}")
def audit(b):
 assert hashlib.sha256(b).hexdigest()==SHA,"unexpected original DSR retail EXE"
 pe=PE(b)
 for r,h in W.items():
  expect=bytes.fromhex(h);pos=pe.at(r,len(expect))
  assert b[pos:pos+len(expect)]==expect,f"opcode drift at {r:x}"
 for r,target in CALLS.items():
  pos=pe.at(r,5)
  assert b[pos]==0xe8 and r+5+struct.unpack_from("<i",b,pos+1)[0]==target,(hex(r),hex(target))
 # Independent negative controls: mutate each witness first opcode byte.
 failed=[]
 for r,h in W.items():
  n=len(bytes.fromhex(h));pos=pe.at(r,n)
  mutation=bytearray(b[pos:pos+n]);mutation[0]^=1
  if mutation==bytes.fromhex(h):failed.append(hex(r))
 assert not failed
 return dict(status="STATIC_PREDRAW_SOURCE_INSTANCE_PASS",
  exe_sha256=SHA,opcode_witnesses=len(W),direct_edges=len(CALLS),
  negative_mutations=len(W),negative_detected=len(W)-len(failed),
  operator_cut="FrpgModel+0x10 --804C--> same registered FrpgModelDrawEntity+0x108 DP Light A/B/alpha --native type5/6--> model draw",
  root_chrins_actor="OPEN",issued_source1_native_ticket="HISTORICAL V448 294/294 ONLY",
  exact_runtime_bridge_activation="OPEN",ptde_pixel="OPEN")
if __name__=="__main__":
 p=argparse.ArgumentParser();p.add_argument("exe",type=Path)
 args=p.parse_args();print(json.dumps(audit(args.exe.read_bytes()),indent=2))
