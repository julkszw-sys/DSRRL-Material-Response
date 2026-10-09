#!/usr/bin/env python3
"""Offline, exact-host DSR SPC25 name-cache boundary audit. No disk patch."""
import argparse
import hashlib
import json
import math
import struct
from collections import Counter
from pathlib import Path

SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
BASE=0x140000000
# Anchors are RVA to exact instruction bytes on the pinned retail EXE.
ANCHORS={
 0x583aa6:"4c8d75e748837dff084c0f4375e7",
 0x583ab7:"498bd6498d4d10e84d4ff9ff",
 0x583ac3:"488bd84885c00f842b010000",
 0x518a10:"48895c24080fb7024533c9488bda4c8bd2",
 0x518a70:"4d8b4108",0x518aba:"4d8b4910",
 0x518a4a:"4569c989000000",0x518cb9:"ff4018",
 0x582930:"40534883ec20488bd9e8c25af9ff",
 0x582951:"894328",0x582954:"48894330",
 0x582958:"48894338",0x58295c:"894340",
 0x5829c9:"895128",0x5829cc:"488b4908",
 0x5829f9:"488b4930",0x582a09:"48894330",
 0x583bf2:"ff4318",
}
SAMPLES={
 "utf16_name_cache_lookup":0x518a10,
 "cache_update_target_1":0x582a50,
 "cache_update_target_2":0x582b00,
 "cache_update_target_3":0x582ca0,
 "name_to_data_target":0x584750,
}
def audit(exe):
 raw=Path(exe).read_bytes()
 digest=hashlib.sha256(raw).hexdigest()
 if digest!=SHA:raise ValueError("Pinned DSR retail EXE SHA mismatch: "+digest)
 if raw[:2]!=b"MZ":raise ValueError("Not PE")
 pe=struct.unpack_from("<I",raw,0x3c)[0]
 if raw[pe:pe+4]!=b"PE\x00\x00":raise ValueError("Bad PE header")
 machine,n=struct.unpack_from("<HH",raw,pe+4)
 opt=struct.unpack_from("<H",raw,pe+20)[0]
 magic=struct.unpack_from("<H",raw,pe+24)[0]
 image_base=struct.unpack_from("<Q",raw,pe+48)[0]
 if (machine,magic,image_base)!=(0x8664,0x20b,BASE):
  raise ValueError("Host ABI not PE32+ x64 at expected image base")
 sections=[]
 for i in range(n):
  off=pe+24+opt+i*40
  vsize,rva,size,ptr=struct.unpack_from("<IIII",raw,off+8)
  sections.append((rva,min(size,vsize or size),ptr))
 def at(rva,size):
  for start,count,ptr in sections:
   if start<=rva and rva+size<=start+count:
    return raw[ptr+rva-start:ptr+rva-start+size]
  raise ValueError("Unmapped RVA: "+hex(rva))
 for rva,hexbytes in ANCHORS.items():
  if at(rva,len(hexbytes)//2)!=bytes.fromhex(hexbytes):
   raise ValueError("Exact opcode signature changed: "+hex(rva))
 call=at(0x583abe,5)
 if call[0]!=0xe8 or 0x583abe+5+struct.unpack_from("<i",call,1)[0]!=0x518a10:
  raise ValueError("Texture name no longer calls verified engine-cache lookup")
 def entropy(b):
  counts=Counter(b)
  return round(-sum((v/len(b))*math.log2(v/len(b)) for v in counts.values()),3)
 return {
  "status":"CPU_CACHE_NAME_PRODUCER_CONFIRMED__NATIVE_SRV_JOIN_OPEN",
  "sha256":digest,"verified_exact_opcode_anchors":len(ANCHORS),
  "engine_name_lookup":"0x140518a10",
  "cache_entry_name_pointer":"+0x08",
  "cache_entry_bucket_next":"+0x10",
  "cache_entry_refcount_one":"+0x18",
  "cache_entry_0x28":"integer, not certified native SRV",
  "cache_entry_0x30":"managed pointer, not certified native D3D resource",
  "opaque_on_disk_entropy_512":{
   k:entropy(at(rva,512)) for k,rva in SAMPLES.items()},
  "bridge_status":"FAIL_OPEN: exact engine cache entry to bound native PS t1 SRV not established",
  "ptde_pixels":"OPEN"}
if __name__=="__main__":
 ap=argparse.ArgumentParser()
 ap.add_argument("exe")
 ap.add_argument("--out")
 args=ap.parse_args()
 result=json.dumps(audit(args.exe),indent=2)+"\n"
 if args.out:Path(args.out).write_text(result,encoding="utf-8")
 print(result,end="")
