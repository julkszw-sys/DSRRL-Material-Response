#!/usr/bin/env python3
"""V445 quick verifier: original retail EXE owner-class and direct-call census.
For exhaustive 86 negative tests use the V445 full evidence ZIP.
This verifies static evidence only, not actor identity/runtime/lighting.
"""
import hashlib, struct, sys
from pathlib import Path

SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
BASE=0x140000000
b=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(b).hexdigest()==SHA, "Retail EXE identity mismatch"
assert b[:2]==b"MZ"
pe=struct.unpack_from("<I",b,0x3c)[0]
assert b[pe:pe+4]==b"PE\0\0"
n=struct.unpack_from("<H",b,pe+6)[0]
opt=struct.unpack_from("<H",b,pe+20)[0]
sections=[]
for i in range(n):
 q=pe+24+opt+i*40
 name=b[q:q+8].split(b"\0")[0].decode()
 size, rva, raw_size, ofs=struct.unpack_from("<IIII",b,q+8)
 chars=struct.unpack_from("<I",b,q+36)[0]
 sections.append((name,rva,raw_size,ofs,chars))
def read(rva,n):
 for _,begin,size,offset,_ in sections:
  if begin<=rva and rva+n<=begin+size:
   return b[offset+rva-begin:offset+rva-begin+n]
 raise AssertionError(f"Missing RVA {rva:x}")
def ptr(rva):
 return struct.unpack("<Q",read(rva,8))[0]
def rel32(rva):
 raw=read(rva,5)
 assert raw[0]==0xe8,f"Not CALL @ {rva:x}"
 return rva+5+struct.unpack("<i",raw[1:])[0]
def direct_calls(target):
 out=[]
 for _,rv,size,ofs,ch in sections:
  if not ch&0x20000000:continue
  chunk=b[ofs:ofs+size]
  for i in range(size-4):
   if chunk[i]!=0xe8:continue
   d=struct.unpack_from("<i",chunk,i+1)[0]
   if rv+i+5+d==target:out.append(rv+i)
 return sorted(out)
expect={
 0x1d7600:[0x574d48,0x5751b4,0x575407,0x5759ca,0x575c1d,0x576b9f,0x705113],
 0x1d7680:[0x4fc015,0x4fd71a,0x4fdbc9,0x4feb6a],
 0x1d77e0:[0x1d7545,0x1d7551,0x2d1f8b,0x2d1f9a,0x2d1fa9,0x2d1fb8,0x2d1fc7],
 0x1d7490:[0xbe8c6,0x215a29,0x21f9c5,0x574270,0x704f4a],
}
for tgt,callers in expect.items():
 assert direct_calls(tgt)==callers,(hex(tgt),direct_calls(tgt))
 for c in callers:assert rel32(c)==tgt
anchor={
 0x5741e0:"488d0529a8e000",0x5741e7:"488901",0x574269:"488d8be8000000",
 0x704f03:"488d05766bcc00",0x704f0a:"488906",0x704f43:"488d8e10010000",
 0x574d25:"498d8de8000000",0x5751ad:"498d8de8000000",
 0x575400:"498d8de8000000",0x5759c3:"498d8de8000000",
 0x575c16:"498d8de8000000",0x576b7c:"498d8fe8000000",
 0x70510c:"488d8e10010000",
 0x4fc002:"488d8b58010000",0x4fdbb6:"488d8b38010000",
 0x1d7600:"48895c2410",0x1d761b:"4883c108",
 0x1d7635:"488d4f18",0x1d7660:"488d4f28",
 0x1d7680:"0fb702",0x1d7683:"66894108",0x1d76ed:"c3",
 0x2d1f81:"488d8eb0010000",0x2d1f90:"488d8ec0010000",
 0x2d1f9f:"488d8ed0010000",0x2d1fae:"488d8ee0010000",
 0x2d1fbd:"488d8ef0010000",
}
for site,hex_value in anchor.items():
 x=bytes.fromhex(hex_value)
 assert read(site,len(x))==x,f"Opcode mismatch {site:x}"
for vtable,typename in ((0x137ea10,b".?AVRemoPartsIns@NS_FRPG@@"),
                          (0x13cba80,b".?AVEzMenuAsmMdlIns@NS_FRPG@@")):
 col=ptr(vtable-8)-BASE
 sig,offset,_,td,_,self_rva=struct.unpack("<IIIIII",read(col,24))
 assert sig==1 and offset==0 and self_rva==col
 assert read(td+16,len(typename))==typename
print("V445 static quick PASS: retail SHA, 4 full direct-call groups,")
print("  RemoPartsIns+E8 6 sites; EzMenuAsmMdlIns+110 1 site;")
print("  FX bulk-copy 4 sites; 5-channel non-MODEL tick 5 sites.")
print("  Runtime, actor-generation and PTDE pixel remain OPEN.")
