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
 # Independently disassembled, readable CPU-side command writer/reader.
 # A matching opcode does not by itself prove the decoder's registration.
 0x583bc2:"8b732885f67421",
 0x583bd1:"4c8bc78bd6e8d5b3ffff",
 0x583bce:"488bc84c8bc78bd6e8d5b3ffffeb15",
 0x57efb0:"48895c2408574883ec20",
 0x57efc3:"ba8d800000",
 0x57efc8:"e84347c300",
 0x57efe2:"48893a",
 0x57efef:"e9bc46c300",
 0x11b3710:"443b411044894110895114",
 0x11b36d9:"0b5114891048834118fc",
 0x57f000:"48895c24084889742410574883ec20",
 0x57f024:"488b5918498b30",
 0x57f04b:"48897718",
 0x57f08c:"ba8e800000",
 # MSVC FrpgTextureEntity handler implements both typed opcodes.
 0x57f110:"48895c2408488974241057",
 0x57f125:"81ea8d800000",
 0x57f12b:"745d",
 0x57f12d:"83fa01",
 0x57f130:"7417",
 0x57f18a:"498bd0",
 0x57f18d:"e86efeffff",
 0x57f168:"48c7471800000000",
 0x57eeb4:"488b41184885c0",
 0x57ee74:"488b41184885c0",
 0x57f030:"488d4b08e8f74f7400",
 0xcc4030:"83c8fff00fc101c3",
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
 # Follow only confirmed E8/JMP rel32 edges; a pointer value or equal
 # numeric opcode is not proof of a D3D11 interface or dispatcher pairing.
 for src,dst,op in ((0x583bd6,0x57efb0,0xe8),
                    (0x57efc8,0x11b3710,0xe8),
                    (0x57efef,0x11b36b0,0xe9),
                    (0x57f18d,0x57f000,0xe8)):
  inst=at(src,5)
  if inst[0]!=op or src+5+struct.unpack_from("<i",inst,1)[0]!=dst:
   raise ValueError("Cache command transport xref mismatch: "+hex(src))
 # Exact hooked callsite includes the original E8 and a rel8 JMP; verify
 # both transfer destinations before allowing a pass-through CPU probe.
 pjump=at(0x583bdb,2)
 if pjump[0]!=0xeb or 0x583bdd+struct.unpack("<b",pjump[1:])[0]!=0x583bf2:
  raise ValueError("Original 808D writer callsite branch changed")
 # RTTI + vtable independently identify the cache-node producer class
 # and the typed opcode consumer without mistaking CPU objects for ID3D11.
 def read_ptr(rva):
  return struct.unpack("<Q",at(rva,8))[0]
 def read_col(rva,type_rva):
  sig,off,cd,td,base_rva,self_rva=struct.unpack("<6I",at(rva,24))
  if (sig,off,cd,td,self_rva)!=(1,0,0,type_rva,rva):
   raise ValueError("Unexpected MSVC RTTI COL: "+hex(rva))
 def read_name(rva,expected):
  payload=expected.encode("ascii")+bytes([0])
  if at(rva+16,len(payload))!=payload:
   raise ValueError("Unexpected MSVC RTTI name: "+hex(rva))
 if read_ptr(0x137fae0)!=0x141665798 or read_ptr(0x137fb08)!=0x14057f110:
  raise ValueError("FrpgTextureEntity vtable/handler identity mismatch")
 if read_ptr(0x1380b90)!=0x141665fa0 or read_ptr(0x1380b98)!=0x140582910:
  raise ValueError("TexHdlResCap vtable/ctor identity mismatch")
 read_col(0x1665798,0x1b137f0)
 read_col(0x1665fa0,0x1b13dc0)
 read_name(0x1b137f0,".?AVFrpgTextureEntity@NS_FRPG@@")
 read_name(0x1b13dc0,".?AVTexHdlResCap@NS_FRPG@@")
 # 0x57f12b is JE rel8 to opcode 0x808D handler dispatch (0x57f18a).
 branch=at(0x57f12b,2)
 if branch[0]!=0x74 or 0x57f12d+struct.unpack("<b",branch[1:])[0]!=0x57f18a:
  raise ValueError("0x808D typed dispatch conditional branch changed")
 def entropy(b):
  counts=Counter(b)
  return round(-sum((v/len(b))*math.log2(v/len(b)) for v in counts.values()),3)
 return {
  "status":"CPU_CACHE_NAME_PRODUCER_CONFIRMED__NATIVE_SRV_JOIN_OPEN",
  "sha256":digest,"verified_exact_opcode_anchors":len(ANCHORS),
  "engine_name_lookup":"0x140518a10",
  "packet_source_detour_site":"0x140583bce (15 exact bytes, writer E8+branch rel8 verified)",
  "packet_typed_decoder_site":"0x14057f000 (15 exact bytes, 0x808D handler-controlled consumer)",
  "cache_entry_name_pointer":"+0x08",
  "cache_entry_bucket_next":"+0x10",
  "cache_entry_refcount_one":"+0x18",
  "cache_entry_0x28":"integer, not certified native SRV",
  "cache_entry_0x30":"managed pointer, not certified native D3D resource",
  "entity_payload_release":"0x14057f030 decrements the refcount stored in payload+0x08 via lock xadd helper 0x140cc4030",
  "entity_payload_accessors":"vtable slot +0x40 and +0x48 return entity+0x18 or fallback manager resource",
  "native_srv_direct_pointer":"NOT_VALIDATED_BY_TYPED_CPU_HANDLE",
  "source_to_command_transport":{
    "entry_id_read":"0x140583bc2 reads node+0x28 as integer",
    "writer_call":"0x140583bd6 -> 0x14057efb0",
    "writer_opcode":"0x808D at 0x14057efc3",
    "writer_payload":"0x14057efe2 stores a pointer into aligned message buffer",
    "message_seal":"0x14057efef -> 0x1411b36b0",
    "typed_receiver":"MSVC RTTI NS_FRPG::FrpgTextureEntity at vtable 0x14137FAE8",
    "typed_dispatch":"0x14057F110 tests opcode 0x808D and directly calls 0x14057F000",
    "receiver":"0x14057F000 reads aligned pointer payload and writes managed object+0x18 at 0x14057F04B",
    "source_entry_class":"MSVC RTTI NS_FRPG::TexHdlResCap at vtable 0x141380B98",
    "alternate_opcode":"0x808E at 0x14057f08c",
    "opcode_to_receiver_inside_typed_handler":"CONFIRMED",
    "handler_external_registration":"UNVERIFIED",
    "verified_direct_rel32_edges":4,
    "verified_msvc_rtti_classes":2,
    "native_d3d11_resource_or_srv":"NOT_VERIFIED"
  },
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
