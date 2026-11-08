#!/usr/bin/env python3
"""V453 read-only retail DSR EXE callsite census.
Raw E8 matches are candidates, not proven function starts or runtime actor identity.
No injection, game hooks, or CSV capture.
"""
import argparse,hashlib,json,struct
from pathlib import Path
SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
TARGETS={
 0x326320:[0x326c02,0x326f6e,0x3270b1],
 0x326360:[],
 0x157cf0:[0x21c554,0x21c71f],
 0x355890:[0x36639d],
}
WITNESS={
 0x21c71b:"488b5618", # same FrpgModel +0x18 source reference on alternate registration
 0x21c71f:"e8ccb5f3ff",
 0x21c724:"894610", # registration result stored +0x10
 0x326c02:"e819f7ffff",
 0x326f6e:"e8adf3ffff",
 0x3270b1:"e86af2ffff",
 0x36639d:"e8eef4feff",
}
class PE:
 def __init__(self,raw):
  self.raw=raw
  pe=struct.unpack_from("<I",raw,0x3c)[0]
  assert raw[:2]==b"MZ" and raw[pe:pe+4]==bytes((80,69,0,0))
  n=struct.unpack_from("<H",raw,pe+6)[0]
  opt=struct.unpack_from("<H",raw,pe+20)[0]
  self.sections=[]
  for i in range(n):
   x=pe+24+opt+40*i
   name=raw[x:x+8].split(bytes((0,)))[0].decode("ascii","replace")
   virt,rva,sz,off=struct.unpack_from("<IIII",raw,x+8)
   flags=struct.unpack_from("<I",raw,x+36)[0]
   self.sections.append((name,rva,sz,off,flags))
 def read(self,rva,n):
  for name,base,sz,off,flags in self.sections:
   if base<=rva and rva+n<=base+sz:return self.raw[off+rva-base:off+rva-base+n]
  raise AssertionError("unmapped RVA "+hex(rva))
 def calls(self,target):
  out=[]
  for name,rva,sz,off,flags in self.sections:
   if not flags & 0x20000000:continue
   data=self.raw[off:off+sz]
   pos=0
   while True:
    pos=data.find(bytes((0xe8,)),pos)
    if pos<0:break
    if pos+5<=len(data):
     delta=struct.unpack_from("<i",data,pos+1)[0]
     if rva+pos+5+delta==target:out.append(rva+pos)
    pos+=1
  return sorted(out)
def audit(raw,checksha=True):
 if checksha:assert hashlib.sha256(raw).hexdigest()==SHA,"wrong original EXE"
 p=PE(raw)
 for addr,hexb in WITNESS.items():
  expected=bytes.fromhex(hexb)
  assert p.read(addr,len(expected))==expected,"witness mismatch "+hex(addr)
 result={}
 for target,expected in TARGETS.items():
  got=p.calls(target)
  assert got==expected,(hex(target),got,expected)
  result[hex(target)]=[hex(x) for x in got]
 return {"status":"STATIC_PASS","original_sha256":SHA,"exact_preimages":len(WITNESS),"callsite_census":result,
  "new":"0x21c71f alternate registration uses FrpgModel+0x18 and stores registration handle FrpgModel+0x10",
  "important":"0x326360 has zero raw direct E8 callers in scanned executable sections; cannot infer unused because indirect calls, tailcalls, obfuscation and inlining remain possible",
  "npc_player_identity":"OPEN","pixel":"OPEN"}
if __name__=="__main__":
 ap=argparse.ArgumentParser();ap.add_argument("exe",type=Path);ap.add_argument("--output",type=Path);a=ap.parse_args()
 out=audit(a.exe.read_bytes());txt=json.dumps(out,indent=2)+"\n"
 if a.output:a.output.write_text(txt,encoding="utf-8")
 print(txt)
