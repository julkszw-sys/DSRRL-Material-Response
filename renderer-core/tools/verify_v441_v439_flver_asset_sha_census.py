#!/usr/bin/env python3
"""V441 original exact raw FLVER SHA census from user-supplied evidence ZIP.
Restricted BND3 / DCX format. Does NOT assume the set contains all game assets.
"""
import hashlib,json,struct,sys,zipfile,zlib
from pathlib import Path
TARGET=sys.argv[2].lower()
assert len(TARGET)==64
result={"raw_map_flver":0,"raw_bnd_flver":0,"parser_sha_matches":[],"errors":[]}
def parsed_digest(b,loc):
 assert b[:5]==b"FLVER",loc
 o,l=struct.unpack_from("<II",b,12);end=o+l
 assert 0x40<=end<=len(b),(loc,end,len(b))
 if hashlib.sha256(b[:end]).hexdigest()==TARGET:result["parser_sha_matches"].append(loc)
with zipfile.ZipFile(Path(sys.argv[1])) as zipf:
 for name in zipf.namelist():
  if name.endswith(".flver.dcx"):
   b=zipf.read(name);assert b[:4]==b"DCX\x00"
   parsed_digest(zlib.decompress(b[0x4c:]),name)
   result["raw_map_flver"]+=1
  elif name.endswith(".partsbnd.dcx") or name.endswith(".chrbnd.dcx"):
   b=zlib.decompress(zipf.read(name)[0x4c:])
   assert b.startswith(b"BND3")
   count=struct.unpack_from("<I",b,0x10)[0]
   assert count<256
   for i in range(count):
    flags,size,offset,identifier,name_off,unpacked=struct.unpack_from("<6I",b,0x20+24*i)
    assert offset+size<=len(b) and name_off<len(b)
    inner=b[name_off:name_off+240].split(b"\x00")[0].decode("latin1")
    if not inner.lower().endswith(".flver"):continue
    payload=b[offset:offset+size]
    if payload.startswith(b"DCX\x00"):payload=zlib.decompress(payload[0x4c:])
    parsed_digest(payload,name+"::"+inner)
    result["raw_bnd_flver"]+=1
result["total"]=result["raw_map_flver"]+result["raw_bnd_flver"]
result["scope"]="ONLY_USER_UPLOADED_EVIDENCE_PART04_NOT_COMPLETE_GAME"
print(json.dumps(result,indent=2))
