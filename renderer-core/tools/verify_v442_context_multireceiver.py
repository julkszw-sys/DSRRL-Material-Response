#!/usr/bin/env python3
"""V442 SHA-locked original DSR FrpgDrawContext post-producer material/palette static witness."""
import hashlib,struct,json,sys
from pathlib import Path
SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
class PE:
 def __init__(self,b):
  self.b=b;off=struct.unpack_from("<I",b,0x3c)[0]
  assert b[:2]==b"MZ" and b[off:off+4]==b"PE\0\0"
  n=struct.unpack_from("<H",b,off+6)[0]
  h=struct.unpack_from("<H",b,off+20)[0]
  self.sections=[]
  for i in range(n):
   k=off+24+h+i*40
   name=b[k:k+8].split(b"\0")[0]
   vs,rva,size,start=struct.unpack_from("<IIII",b,k+8)
   self.sections.append((name,rva,size,start))
 def off(self,r):
  for name,va,size,offset in self.sections:
   if va<=r<va+size:return offset+r-va
  raise AssertionError(hex(r))
 def read(self,r,n):return self.b[self.off(r):self.off(r)+n]
 def dest(self,r):
  x=self.read(r,5);assert x[0]==0xe8
  return r+5+struct.unpack_from("<i",x,1)[0]
BYTES={
 0x21630e:"4c8bf2",0x21639c:"4c897520",0x2167ad:"4c8d8708010000",
 0x2167b4:"498bd6",0x2167c6:"498bd6",0x2167d8:"498bd6",
 0x2172aa:"ff5018",0x220102:"488bf2",0x2201a6:"488975d0",
 0x220614:"488d9f08010000",0x220625:"488bd6",0x220641:"488bd6",
 0x22064f:"488bd6",0x220ced:"ff5018",
 0x20ddff:"4d8b37",0x20e019:"4d8ba660010000",
 0x20e0d1:"498bd6",0x20ec06:"488bd5",0x20fc25:"488bd5",0x210163:"488bd7",
 0x1c0916:"410fb65905",0x1c091b:"4c8be2",0x1c09a0:"bb1c000000",
 0x1c0a06:"4883c122",0x1c0a0a:"83c303",0x1c0a0d:"48c1e104",
 0x1c0a14:"4903cc",0x1c0a17:"0f1101",0x1c0a1e:"0f114910",0x1c0a26:"0f114120",
 0x1c86a4:"39af8c220000",0x1c871c:"89af8c220000",
 0x1caca7:"41399f90220000",0x1cad9f:"41899f90220000",
 0x1cad8f:"410f29bfa0020000",0x1cad7f:"410f29bf00130000"}
CALLS={
 0x2167ba:0x1c0be0,0x2167cc:0x1c0c40,0x2167de:0x1c0c60,
 0x220639:0x1c0be0,0x220632:0x1c0c10,0x220647:0x1c0c40,0x220655:0x1c0c60,
 0x210195:0x1c08e0,0x20e106:0x1c08e0,0x20ec38:0x1c08e0,0x20fc57:0x1c08e0}
EXPECTED=[0x20e106,0x20ec38,0x20fc57,0x210195]
def check(data,require_sha=True):
 if require_sha:assert hashlib.sha256(data).hexdigest()==SHA,"owner EXE SHA mismatch"
 p=PE(data)
 for r,h in BYTES.items():assert p.read(r,len(bytes.fromhex(h)))==bytes.fromhex(h),hex(r)
 for r,d in CALLS.items():assert p.dest(r)==d,hex(r)
 sites=[]
 for name,va,size,off in p.sections:
  if name!=b".text":continue
  b=data[off:off+size]
  for j in range(len(b)-4):
   if b[j]!=0xe8:continue
   if va+j+5+struct.unpack_from("<i",b,j+1)[0]==0x1c08e0:sites.append(va+j)
 assert sorted(sites)==EXPECTED,sites
 return {"static_cpu_context_transport":"CONFIRMED","witness_bytes":len(BYTES),
   "exact_direct_calls":len(CALLS),"context_palette_writer_calls":[hex(x) for x in sites],
   "exact_issued_rx34":"OPEN","gpu_CB_SRV":"OPEN","PTDE_pixel":"OPEN"}
def negatives(data):
 p=PE(data)
 loc=[0x21630e,0x21639c,0x2167cc,0x220102,0x2201a6,
 0x220647,0x220655,0x20ddff,0x20e019,0x20e0d1,
 0x20ec06,0x20fc25,0x210163,0x1c0916,0x1c0a06,
 0x1c0a17,0x1c0a1e,0x1c0a26,0x20e106,0x20ec38,
 0x20fc57,0x210195,0x1c86a4,0x1caca7,0x1cad8f]
 for r in loc:
  broken=bytearray(data);broken[p.off(r)]^=1
  try:check(bytes(broken),require_sha=False)
  except AssertionError:continue
  raise AssertionError("mutation escaped "+hex(r))
 return len(loc)
if __name__=="__main__":
 b=Path(sys.argv[1]).read_bytes()
 print(json.dumps(check(b),indent=2))
 print("semantic_negative_controls",negatives(b))
