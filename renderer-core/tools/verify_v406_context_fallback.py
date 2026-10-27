#!/usr/bin/env python3
"""V406 pinned-executable static RE contract; does not patch the EXE."""
import hashlib, struct, sys
from pathlib import Path
data=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest()=="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
pe=struct.unpack_from("<I",data,0x3c)[0]
h=pe+24+struct.unpack_from("<H",data,pe+20)[0]
sections=[]
for i in range(struct.unpack_from("<H",data,pe+6)[0]):
 o=h+40*i; size,base,rawsize,raw=struct.unpack_from("<IIII",data,o+8)
 sections.append((base,raw,rawsize))
def rd(rva,n):
 for base,raw,size in sections:
  if base<=rva and rva+n<=base+size:return data[raw+rva-base:raw+rva-base+n]
 raise AssertionError(hex(rva))
checked=0
def byte(rva,want):
 global checked
 assert rd(rva,len(bytes.fromhex(want)))==bytes.fromhex(want),hex(rva)
 checked+=1
def call(rva,target):
 global checked
 ins=rd(rva,5)
 assert ins[0]==0xe8 and rva+5+struct.unpack_from("<i",ins,1)[0]==target,hex(rva)
 checked+=1
byte(0x1ec98b,"b9 00 25 00 00")
call(0x1ec9cd,0x1ea460)
for rva,hx in [(0x1ea4c4,"48 89 8e 18 23 00 00"),(0x1ea4cf,"48 89 8e 20 23 00 00"),(0x1ea4ee,"48 89 be 38 23 00 00"),(0x1ea4f5,"48 89 be 40 23 00 00")]:byte(rva,hx)
for typ,at,site,store,slot in [(5,0x1ea5ba,0x1ea5c5,0x1ea5d2,0x2338),(2,0x1ea5dd,0x1ea5e8,0x1ea5f5,0x233c),(7,0x1ea600,0x1ea60b,0x1ea618,0x2340),(9,0x1ea623,0x1ea62e,0x1ea63b,0x2344)]:
 call(site,0x569b40)
 byte(store-3,"8b 40 10")
 byte(store,"89 86 "+struct.pack("<I",slot).hex(" "))
for typ,site,fallback,slot in [(5,0x1eab06,0x1eab0f,0x2338),(2,0x1eab46,0x1eab4f,0x233c),(7,0x1eab86,0x1eab8f,0x2340),(9,0x1eabc6,0x1eabcf,0x2344),(6,0x1eac06,0x1eac0f,0x2338)]:
 call(site,0x569b70)
 byte(fallback,"8b 83 "+struct.pack("<I",slot).hex(" "))
byte(0x569b60,"48 8b 44 d0 08")
byte(0x569b90,"8b 04 d0")
for rva,hx in [(0x4fff8b,"39 47 68 74 1c"),(0x4fff90,"89 47 68"),(0x4fffa8,"48 89 47 60"),(0x4fffdc,"39 5f 6c"),(0x4fffdf,"0f 84 df 00 00 00"),(0x50016b,"39 47 68 74 1c"),(0x500170,"89 47 68"),(0x500188,"48 89 47 60"),(0x5001c4,"39 5f 70"),(0x5001c7,"74 36")]:byte(rva,hx)
for rva in (0x4fffa3,0x500183):call(rva,0x11b3d50)
assert rd(0x1af8348+16,80).split(b"\0")[0]==b".?AVFrpgDrawContext@NS_FRPG@@"
print("V406 STATIC RE PASS:",checked,"pinned byte/call checks; runtime/pixel OPEN")
