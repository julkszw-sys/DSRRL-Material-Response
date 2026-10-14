#!/usr/bin/env python3
"""V289 licensed-original Stable RX33-35 operator census; NO shader bytes bundled."""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
import zipfile
import zlib
from pathlib import Path

EXPECTED={
 33:"35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24",
 34:"d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837",
 35:"7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701"
}
FG_LS="28c23273a5fb0b984b55fde72ee59546af4aa8646c55e1cb1bf8db426e9a4363"
POST="d4fdd1ec09075f73609d978616733cc5bc97893606cb51751d97b61d28735512"
BINDER="DSR/shader/FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx"

def u32(b,off):return struct.unpack_from("<I",b,off)[0]
def digest(b):return hashlib.sha256(b).hexdigest()
def words(b):
 n=u32(b,28)
 ss=[u32(b,32+i*4) for i in range(n) if b[u32(b,32+i*4):u32(b,32+i*4)+4]==b"SHEX"]
 if len(ss)!=1:raise AssertionError("one SHEX required")
 at=ss[0];size=u32(b,at+4)
 w=list(struct.unpack_from("<"+"I"*(size//4),b,at+8))
 if w[1]!=len(w):raise AssertionError("SHEX length")
 return w
def instructions(w):
 i=2
 while i<len(w):
  n=(w[i]>>24)&127
  if not n or i+n>len(w):raise AssertionError("instruction decode invalid")
  yield i,w[i]&0x7ff,n
  i+=n

def audit(rx,b):
 if digest(b)!=EXPECTED[rx]:raise AssertionError(f"RX{rx} source SHA mismatch")
 w=words(b)
 sites=[]
 for i,op,n in instructions(w):
  if op!=0x2f or n!=6 or i<204 or i+25>len(w):continue
  post=struct.pack("<21I",*w[i:i+21])
  if digest(post)!=POST:continue
  if w[i+21:i+25]!=[0x0400004c,0x0020800a,0,0x66]:continue
  fog_ls=struct.pack("<204I",*w[i-204:i])
  if digest(fog_ls)!=FG_LS:continue
  sites.append(i)
 if len(sites)!=1:raise AssertionError(f"RX{rx} post LightScattering candidates={len(sites)}")
 i=sites[0]
 changed=w.copy();changed[i:i+21]=[0x0100003a]*21
 if changed[:i]!=w[:i] or changed[i+21:]!=w[i+21:]:
  raise AssertionError("Fog or independent operator changed")
 if digest(struct.pack("<204I",*changed[i-204:i]))!=FG_LS:
  raise AssertionError("Fog+LS source modified")
 return {"rx":rx,"source_sha256":EXPECTED[rx],"fog_ls204_sha256":FG_LS,
         "dsr_post_ls21_sha256":POST,"post_ls_word":i,
         "fog_stock_dwords_preserved":True,
         "only_changed_post_ls_dwords":21}

def from_zip(path):
 with zipfile.ZipFile(path) as z:data=z.read(BINDER)
 if data[:4]!=b"DCX\\0":raise AssertionError("DCX")
 payload=zlib.decompress(data[0x4c:])
 found={};pos=0
 while (pos:=payload.find(b"DXBC",pos))>=0:
  if pos+32>=len(payload):break
  length=u32(payload,pos+24)
  if 1000<length<100000 and pos+length<=len(payload):
   data=payload[pos:pos+length]
   h=digest(data)
   for rx,want in EXPECTED.items():
    if h==want:found[rx]=data
  pos+=4
 if set(found)!=set(EXPECTED):raise AssertionError("missing exact DSR receivers")
 return found

def main():
 p=argparse.ArgumentParser()
 p.add_argument("owner_source_zip")
 p.add_argument("--json")
 args=p.parse_args()
 result=[audit(rx,data) for rx,data in sorted(from_zip(args.owner_source_zip).items())]
 if args.json:Path(args.json).write_text(json.dumps(result,indent=2)+"\n")
 print("V289_STABLE_STOCK_FOG_LS_SCOPE_PASS 3/3 receivers; 204-word Fog+LS preserved; 21-word post-LS candidate")
if __name__=="__main__":main()
