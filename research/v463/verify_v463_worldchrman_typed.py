#!/usr/bin/env python3
"""Original retail DSR: verify typed WorldChrManImp global and ChrIns/model anchors.
No EXE patch; WorldChrManImp+0x68 gameplay class remains a runtime hypothesis.
"""
import argparse,hashlib,json,struct
from pathlib import Path
SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
W={int(k,16):bytes.fromhex(v) for k,v in
   (x.split(":") for x in """27bf20:4053
27bf32:48833d16bf9f0100
27bf3c:4c8b05cdcf8e01
27bf48:ba10000000
27bf4d:b9301f0000
27bf52:e8b97ea400
27bf5c:4885c0
27bf61:488bd3
27bf64:488bc8
27bf67:e864190f00
27bf6d:488905dcbe9f01
27bf74:488bc8
27bf7c:e97f1e0f00
36d8ec:488bea
36d8ef:488bf9
36d8f2:488d0517a9fb00
36d8f9:488901
36d8fc:48895110
36d900:8b4208
36d903:894118
36d906:4c8db960050000
36d90d:4c897920
36d911:8b4218
36d914:894128
36d917:4c8da180060000
36d91e:4c896130
36d968:8d5308
36d96b:b998070000
36d970:e89b649500
36d97d:4885c0
36d982:488bd5
36d985:488bc8
36d988:e8b332feff
36d98f:488bc3
36d992:48898728050000
350c5e:488bd9
350c61:488911
350c64:8b4218
350c67:894108
34a173:e8380fecff
34a17f:49894d60
321d05:488b4960
321d28:ff5028
20b14f:488bce
20b152:e859120100
21c550:488b5318
21c554:e897b7f3ff
21c559:894310
11b891e:44894110
21f92e:e87d34fdff
21f951:4533ff
21f954:4c897f50""".splitlines())}
E={0x27bf52:0xcc3e10,0x27bf67:0x36d8d0,0x36d970:0xcc3e10,
   0x36d988:0x350c40,0x34a173:0x20b0b0,0x20b152:0x21c3b0,
   0x21c554:0x157cf0,0x21f92e:0x1f2db0}
class PE:
 def __init__(self,b):
  self.b=b;p=struct.unpack_from("<I",b,0x3c)[0]
  assert b[:2]==b"MZ" and b[p:p+4]==b"PE\x00\x00"
  n=struct.unpack_from("<H",b,p+6)[0];o=struct.unpack_from("<H",b,p+20)[0]
  self.sec=[]
  for i in range(n):
   t=p+24+o+40*i;_,rva,sz,off=struct.unpack_from("<IIII",b,t+8)
   self.sec.append((rva,sz,off))
 def off(self,r,n=1):
  for a,size,o in self.sec:
   if a<=r and r+n<=a+size:return o+r-a
  raise ValueError(f"nonmapped {r:x}")
 def read(self,r,n):
  o=self.off(r,n);return self.b[o:o+n]
 def call(self,r):
  b=self.read(r,5);assert b[0]==0xe8
  return r+5+struct.unpack_from("<i",b,1)[0]
 def ptr(self,r):return struct.unpack("<Q",self.read(r,8))[0]
def check(raw,check_sha=True):
 if check_sha:assert hashlib.sha256(raw).hexdigest()==SHA
 p=PE(raw)
 for r,by in W.items():assert p.read(r,len(by))==by,hex(r)
 for r,dst in E.items():assert p.call(r)==dst,(hex(r),hex(dst))
 assert 0x27bf32+8+struct.unpack("<i",p.read(0x27bf35,4))[0]==0x1c77e50
 assert 0x27bf6d+7+struct.unpack("<i",p.read(0x27bf70,4))[0]==0x1c77e50
 assert 0x36d8f2+7+struct.unpack("<i",p.read(0x36d8f5,4))[0]==0x1328210
 for vt,col,td,name in [
  (0x1328210,0x164b700,0x1b00c08,b".?AVWorldChrManImp@NS_FRPG@@"),
  (0x13251f0,0x164ad48,0x1b00708,b".?AVPlayerIns@NS_FRPG@@"),
  (0x12eaf30,0x163caa8,0x1af9490,b".?AVChrModel@NS_FRPG@@")]:
  assert p.ptr(vt-8)==0x140000000+col
  sig,_,_,typeid,_,selfr=struct.unpack("<6I",p.read(col,24))
  assert sig==1 and selfr==col and typeid==td
  assert p.read(td+16,len(name)+1)==name+b"\x00"
 return {"status":"STATIC_PASS","exe_SHA256":SHA,
  "opcode_witnesses":len(W),"direct_E8_edges":len(E),
  "WorldChrManImp_global":"0x1C77E50",
  "WorldChrManImp_vtable":"0x1328210",
  "WorldChrManImp_plus_0x68_candidate_actor":"READ_BY_CHRINS_CALLERS_CLASS_NOT_YET_PROVEN",
  "WorldChrManImp_plus_0x528_subsystem":"CTOR_0x350C40_NOT_PROVEN_NPC_LIST",
  "PlayerIns_vtable":"0x13251F0","ChrModel_vtable":"0x12EAF30",
  "PlayerIns_model_field":"ChrIns+0x60",
  "gameplay_ownership":"OPEN","GPU_pixel":"OPEN","PTDE_pixel":"OPEN"}
def negatives(b):
 p=PE(b);bad=[]
 tests=set(W)|set(E)|{0x164b700,0x1328208,0x1b00c18}
 for r in sorted(tests):
  q=bytearray(b);q[p.off(r)]^=1
  try:check(q,False)
  except AssertionError:continue
  bad.append(hex(r))
 q=bytearray(b);q[-1]^=1
 try:check(q,True)
 except AssertionError:pass
 else:bad.append("SHA")
 return {"tested":len(tests)+1,"bad":bad,"status":"PASS" if not bad else "FAIL"}
if __name__=="__main__":
 a=argparse.ArgumentParser();a.add_argument("exe",type=Path)
 a.add_argument("--mutations",action="store_true")
 x=a.parse_args();b=x.exe.read_bytes();r=check(b)
 if x.mutations:r["negative_controls"]=negatives(b)
 print(json.dumps(r,ensure_ascii=False,indent=2))
