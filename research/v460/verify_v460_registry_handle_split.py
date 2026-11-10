#!/usr/bin/env python3
"""V460 read-only original DSR SHA-locked dual registrar handles and delta encoded message routing."""
import argparse,hashlib,json,struct
from pathlib import Path
SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
W=dict((int(a,16),h) for a,h in (line.split(":") for line in """\
21c4f3:48897b18
21c550:488b5318
21c559:894310
21c71b:488b5618
21c724:894610
157d08:4d8bf0
157d11:e86aa40700
157d1a:448bf8
157d20:498b4a30
157d24:e837240601
157d2d:448bc0
157d30:ba01000000
157d35:8bf8
157d3e:488bcb
157d41:e8cab90501
157ddd:418bd7
157df6:e8e52a0000
157e00:8bc7
15a97e:897e10
1d2180:488b05e1a6a901
1d2187:8b4010
11ba175:83caff
11ba18b:488b4310
11ba18f:4c630c90
11ba1a7:a801
11ba1b0:81e3ffffff00
11ba1b6:81e2000000ff
11ba1bc:0bda
11ba1be:0fbaeb1f
11ba1d2:8bc3
11b36b0:488b01
11b36c1:482b5108
11b36d5:48c1e211
11b36d9:0b5114
11b36dc:8910
11b36e7:0fbae210
11b36eb:720b
11b36ed:8b4110
11b36f0:418900
11b36f8:c7411400000000
11b371b:7507
11b371d:0fbaea10
11b3710:443b4110
11b3714:44894110
11b3718:895114
21caa1:8b5310
222aca:448bc2
222ad2:ba4c800000
1f2de6:8b5710
1f2df1:48894620
223b03:4c8bce
223b0f:e8ecbdffff
21f928:498bd9
21f93e:488b4320
21f942:48894740
21f954:4c897f50""".splitlines()))
C={0x21c554:0x157cf0,0x21c71f:0x157cf0,0x157d11:0x1d2180,
  0x157d24:0x11ba160,0x157d41:0x11b3710,0x157df6:0x15a8e0,
  0x1f2dec:0x11b3d50,0x223b0f:0x21f900,0x21f92e:0x1f2db0,
  0x222ae0:0x11b3710,0x21cac7:0x222ac0}
class PE:
 def __init__(self,b):
  self.b=b
  h=struct.unpack_from("<I",b,0x3c)[0];assert b[:2]==b"MZ" and b[h:h+4]==b"PE\0\0"
  n=struct.unpack_from("<H",b,h+6)[0];o=struct.unpack_from("<H",b,h+20)[0]
  self.s=[struct.unpack_from("<IIII",b,h+24+o+i*40+8) for i in range(n)]
 def pos(self,a,n=1):
  for _,r,z,f in self.s:
   if r<=a and a+n<=r+z:return f+a-r
  raise AssertionError(hex(a))
 def read(self,a,n):
  p=self.pos(a,n);return self.b[p:p+n]
 def call(self,a):
  b=self.read(a,5);assert b[0]==0xe8
  return a+5+struct.unpack("<i",b[1:])[0]
def verify(b,sha=True):
 if sha:assert hashlib.sha256(b).hexdigest()==SHA
 p=PE(b)
 for a,h in W.items():assert p.read(a,len(bytes.fromhex(h)))==bytes.fromhex(h),hex(a)
 for a,t in C.items():assert p.call(a)==t,hex(a)
 return {"status":"STATIC_CONFIRMED","exe_sha":SHA,"opcode_witnesses":len(W),
   "direct_call_edges":len(C),"model_handle":"FrpgModel+0x10 = registrar return EDI",
   "seed_key":"Seed+0x10 = R15d from global+0x10",
   "command":"0x804C is sent to model handle; repeated handle has bit16 and is omitted from stream",
   "entity_resource":"Seed+0x10 maps to entity+0x20; Seed+0x20 copied to entity+0x40",
   "actor_receiver_and_pixel":"OPEN"}
def mutations(b):
 p=PE(b);fail=[];points=sorted(set(W)|set(C))
 for a in points:
  x=bytearray(b);x[p.pos(a)]^=1
  try:verify(x,False)
  except AssertionError:continue
  fail.append(hex(a))
 x=bytearray(b);x[-1]^=1
 try:verify(x)
 except AssertionError:pass
 else:fail.append("SHA")
 return {"attempted":len(points)+1,"false_accepts":fail,"status":"PASS" if not fail else "FAIL"}
if __name__=="__main__":
 a=argparse.ArgumentParser();a.add_argument("exe",type=Path);a.add_argument("--mutations",action="store_true")
 q=a.parse_args();b=q.exe.read_bytes();r=verify(b)
 if q.mutations:r["negative_mutations"]=mutations(b)
 print(json.dumps(r,indent=2))
