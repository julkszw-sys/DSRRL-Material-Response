#!/usr/bin/env python3
"""Reproducible exact original-EXE FgModel source and registration RE.

Validates *four instruction-aligned* FgModel constructor callsites.
Other byte-wise E8 matches are not automatically classified as calls.
"""
from pathlib import Path
import hashlib,struct,sys
SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
PRE={
  0x27c514:"48 8b 78 10", # global object +0x10 -> RDI source
  0x27c546:"41 b0 01", # ctor mode flag
  0x27c549:"48 8b d7", # source = RDI
  0x27c54f:"e8 0c 1d 4f 00", # ctor
  0x282640:"48 8b 70 10", # same global object +0x10 -> RSI
  0x282672:"41 b0 01",
  0x282675:"48 8b d6", # source = RSI
  0x28267b:"e8 e0 bb 4e 00",
  0x6a62c8:"4c 8b b4 f7 48 02 00 00", # source = array slot
  0x6a6322:"45 33 c0", # ctor mode 0
  0x6a6325:"49 8b d6", # source = R14
  0x6a632b:"e8 30 7f 0c 00",
  0x72f1a5:"e8 36 1b 00 00", # source helper 0x730CE0
  0x72f1aa:"48 8b e8", # RBP receives helper result
  0x72f2b9:"41 b0 01",
  0x72f2bc:"48 8b d5", # source = RBP
  0x72f2c2:"e8 99 ef 03 00",
  0x76e28f:"48 89 51 30", # FgModel+0x30 = RDX
  0x76e3e7:"48 8b 43 30", # +0x30 read back
  0x76e3f0:"0f b6 90 ca 00 00 00", # source+0xCA byte
  0x76e40a:"e8 61 bd 9e ff", # registration call 0x15A170
  0x76e40f:"89 43 38", # FgModel+0x38 = EAX
  0x76e464:"44 8b 47 38", # destructor consumes the handle
  0x76e468:"ba 10 00 00 00", # registry mode 16
  0x76e470:"e8 9b 52 a4 00", # registration notification 0x11B3710
  0x76e29a:"c7 41 40 01 00 00 00", # reference count initial 1
  0x76e550:"ff 41 40", # reference count ++
  0x76e564:"ff 49 40", # reference count --
  0x11ba1b0:"81 e3 ff ff ff 00", # 24-bit registry slot index
  0x11ba1b6:"81 e2 00 00 00 ff", # high-byte native generation
  0x11ba1be:"0f ba eb 1f", # bit31 allocated marker
  0x11ba1d2:"8b c3", # returns registered slot+generation handle
  0x11ba0fb:"81 e7 ff ff ff 00", # recycled low24 slot
  0x11ba10c:"8d 82 00 00 00 01", # generation increment
  0x11ba115:"25 00 00 00 7f", # generation low 7 bits modulo128
  0x11ba11a:"48 83 c8 01", # free state marker

}
CALLERS=(0x27c54f,0x28267b,0x6a632b,0x72f2c2)
def verify(path):
 b=Path(path).read_bytes()
 assert hashlib.sha256(b).hexdigest()==SHA,"Unsupported executable identity"
 pe=struct.unpack_from("<I",b,0x3C)[0]
 assert b[pe:pe+4]==b"PE\0\0"
 count=struct.unpack_from("<H",b,pe+6)[0]
 size=struct.unpack_from("<H",b,pe+20)[0]
 sections=[]
 for i in range(count):
  j=pe+24+size+40*i
  sz,va,rawsize,raw=struct.unpack_from("<IIII",b,j+8)
  sections.append((va,rawsize,raw))
 def off(rva):
  return next(raw+rva-va for va,n,raw in sections if va<=rva<va+n)
 for rva,h in PRE.items():
  want=bytes.fromhex(h)
  assert b[off(rva):off(rva)+len(want)]==want,f"opcode mismatch at {rva:x}"
 for rva in CALLERS:
  o=off(rva)
  target=rva+5+struct.unpack_from("<i",b,o+1)[0]
  assert target==0x76e260,f"unexpected FgModel ctor target {rva:x}"
 print(f"V417 PASS: {len(PRE)} opcode preimages, {len(CALLERS)} instruction-aligned constructor calls")
 print("V417 exact pool registration: low24 slot, high7 generation, bit31 allocated; actor identity OPEN")
if __name__=="__main__":
 if len(sys.argv)!=2:raise SystemExit("Usage: python test_v417_fgmodel_ctor_sources_exe_abi.py DarkSoulsRemastered.exe")
 verify(sys.argv[1])
