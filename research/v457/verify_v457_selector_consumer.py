#!/usr/bin/env python3
"""V457 SHA-locked original DSR EXE CPU Light/Fog/Scattering producer→consumer census.
Read-only. No EXE modification, no hooks, no RX34 or PTDE pixel claims.
"""
import hashlib,struct,sys,json
SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
CALLS={
0x21f600:[0x21ca6c,0x2fc2c6,0x326470,0x357bfd],
0x21f5c0:[0x21ca7c,0x2fc2d8,0x326500,0x357c37],
0x21f640:[0x21ca89,0x2fc2ea,0x326590,0x357c71]}
EDGES={**{site:t for t,sites in CALLS.items() for site in sites},
0x20b546:0x21c970,0x21cac7:0x222ac0,0x22346c:0x222b60,0x222bbb:0x1d7580,
0x2ee134:0x125060,0x2ee14d:0x125060,0x2ee166:0x125060}
W={
0x21f5c0:"440fbe8192000000",0x21f5c8:"0fbe8194000000",
0x21f600:"440fbe8192000000",0x21f608:"0fbe8193000000",
0x21f640:"440fbe8192000000",0x21f648:"0fbe8195000000",
0x21f5cf:"41c1f803",0x21f60f:"41c1f803",0x21f64f:"41c1f803",
0x21f5dc:"4183e07f",0x21f61c:"4183e07f",0x21f65c:"4183e07f",
0x21f5e0:"0fb6c0",0x21f620:"0fb6c0",0x21f660:"0fb6c0",
0x21f5e3:"41c1e008",0x21f623:"41c1e008",0x21f663:"41c1e008",
0x21f5e7:"440bc0",0x21f627:"440bc0",0x21f667:"440bc0",
0x21f5ed:"448902",0x21f62d:"448902",0x21f66d:"448902",
0x21f5f1:"c702ffffffff",0x21f631:"c702ffffffff",0x21f671:"c702ffffffff",
0x21c978:"8bb980000000",0x21c981:"0bb984000000",
0x21c98f:"c7818400000000000000",0x21ca5b:"40f6c7e0",
0x21cac7:"e8f45f0000",0x222ad2:"ba4c800000",
0x222af7:"f30f1132",0x222b0d:"891a",0x222b21:"8b442460",0x222b39:"8b442468",
0x22346c:"e8eff6ffff",0x222ba7:"498d8b08010000",0x222bb5:"8b02",0x222bbb:"e8c049fbff",
0x2ee120:"440fbe8793000000",0x2ee139:"440fbe8794000000",0x2ee152:"440fbe8795000000",
0x2fb96d:"440fbe8093000000",0x2fb98d:"440fbe8094000000",0x2fb9ad:"440fbe8095000000",
0x357c17:"0fbe81ab000000",0x357c51:"0fbe81ac000000",0x357c8b:"0fbe81ad000000",
0x357caa:"0fb78b96000000",0x357cba:"81889c00000000010000",0x357cc4:"668988ae000000",
0x357bca:"4c89ac2488000000"}
LABELS={0x1316d00:"Light id: %d\n",0x1316d10:"Fog id: %d\n",0x1316d20:"Scattering id: %d\n"}
LABEL_LEAS={0x2ee128:0x1316d00,0x2ee141:0x1316d10,0x2ee15a:0x1316d20}
class PE:
 def __init__(self,raw):
  self.raw=raw
  p=struct.unpack_from("<I",raw,0x3c)[0]
  assert raw[:2]==b"MZ" and raw[p:p+4]==b"PE\0\0"
  n=struct.unpack_from("<H",raw,p+6)[0];o=struct.unpack_from("<H",raw,p+20)[0]
  self.sections=[]
  for i in range(n):
   x=p+24+o+40*i
   _,va,size,offset=struct.unpack_from("<IIII",raw,x+8)
   flags=struct.unpack_from("<I",raw,x+36)[0]
   self.sections.append((va,size,offset,flags))
 def offset(self,r,n=1):
  for a,s,f,_ in self.sections:
   if a<=r and r+n<=a+s:return f+r-a
  raise AssertionError(f"unmapped RVA {r:x}")
 def read(self,r,n):
  i=self.offset(r,n)
  return self.raw[i:i+n]
 def direct(self,r):
  b=self.read(r,5)
  assert b[0]==0xe8
  return r+5+struct.unpack("<i",b[1:])[0]
 def raw_calls(self,target):
  out=[]
  for a,n,f,flags in self.sections:
   if not flags&0x20000000:continue
   d=self.raw[f:f+n];p=0
   while 1:
    p=d.find(b"\xe8",p)
    if p<0:break
    if p+5<=n and a+p+5+struct.unpack_from("<i",d,p+1)[0]==target:out.append(a+p)
    p+=1
  return sorted(out)
def verify(raw,sha=True,census=True):
 if sha:assert hashlib.sha256(raw).hexdigest()==SHA,"EXE SHA mismatch"
 p=PE(raw)
 for a,h in W.items():assert p.read(a,len(bytes.fromhex(h)))==bytes.fromhex(h),("opcode",hex(a))
 for a,t in EDGES.items():assert p.direct(a)==t,("E8",hex(a))
 for a,t in LABELS.items():assert p.read(a,len(t)+1)==t.encode()+b"\0",("label",hex(a))
 for a,t in LABEL_LEAS.items():
  b=p.read(a,7)
  assert b[:3]==b"\x48\x8d\x15" and a+7+struct.unpack("<i",b[3:])[0]==t,("LEA",hex(a))
 if census:
  for t,sites in CALLS.items():assert p.raw_calls(t)==sites,("raw E8",hex(t))
 return {"status":"STATIC_PASS","original_SHA256":SHA,"opcode_witnesses":len(W),
         "E8_edges":len(EDGES),"RIP_label_edges":len(LABEL_LEAS),
         "semantics":{"0x93":"Light id","0x94":"Fog id","0x95":"Scattering id","0x96":"unnamed word, CPU state +0xAE"},
         "packers":{"Light":"0x21F600","Fog":"0x21F5C0","Scattering":"0x21F640"},
         "formula":"if signed family>>3 or signed channel is negative: -1; else (((signed family>>3)&127)<<8) | unsigned channel",
         "route":"0x20B546→0x21C970→0x222AC0 (command ID 0x804C); 0x22346C→0x222B60→0x1D7580(receiver+0x108); opcode-to-dispatch matching needs final validation",
         "actor_to_RX34":"OPEN","pixel":"OPEN"}
def negative(raw):
 p=PE(raw);faults=[]
 checks=sorted(set(W)|set(EDGES)|set(LABELS)|set(LABEL_LEAS))
 for a in checks:
  q=bytearray(raw);q[p.offset(a)]^=1
  try:verify(q,False,False)
  except AssertionError:pass
  else:faults.append(hex(a))
 q=bytearray(raw);q[-1]^=1
 try:verify(q,True,False)
 except AssertionError:pass
 else:faults.append("SHA")
 return {"attempted":len(checks)+1,"false_accepts":faults,"status":"PASS" if not faults else "FAIL"}
if __name__=="__main__":
 import argparse
 a=argparse.ArgumentParser()
 a.add_argument("exe");a.add_argument("--mutations",action="store_true")
 v=a.parse_args();b=open(v.exe,"rb").read();r=verify(b)
 if v.mutations:r["negative"]=negative(b)
 print(json.dumps(r,indent=2))
