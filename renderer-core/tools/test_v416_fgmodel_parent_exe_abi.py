#!/usr/bin/env python3
"""V416 read-only exact-EXE ABI attest. Supply the user's own DSR executable."""
from pathlib import Path
import hashlib,struct,sys
SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
BASE=0x140000000
OP={
0x76e277:"48 8d 05 52 14 c7 00",0x76e27e:"48 89 01",
0x76e28b:"48 89 79 28",0x76e28f:"48 89 51 30",
0x76e29a:"c7 41 40 01 00 00 00",
0x76e3d2:"4c 8b cb",0x76e3db:"e8 00 e3 a9 ff",
0x76e3e3:"48 89 7b 28",
0x20c703:"49 8b d9",0x20c71c:"48 8d 05 0d ea 0d 01",
0x20c723:"49 89 06",0x20c726:"49 89 9e 40 01 00 00",
0x20c7ad:"48 c7 86 40 01 00 00 00 00 00 00",
0x76e484:"48 8b 77 28",0x76e4b3:"48 89 6f 28",
0x20dda0:"4c 8b dc",0x20ddff:"4d 8b 37",
0x20df00:"48 8d b7 88 00 00 00",0x20e014:"e8 07 da 01 00",
}
RTTI={0x13df6d0:"FgModel@NS_FRPG",
      0x12eb130:"FgFlverModel@NS_FRPG",
      0x12eb760:"FlverModel@NS_FRPG"}
def verify(path):
 b=Path(path).read_bytes()
 assert hashlib.sha256(b).hexdigest()==SHA,"Wrong DSR executable"
 pe=struct.unpack_from("<I",b,0x3c)[0]
 assert b[pe:pe+4]==b"PE\0\0"
 cnt=struct.unpack_from("<H",b,pe+6)[0]
 size=struct.unpack_from("<H",b,pe+20)[0]
 assert struct.unpack_from("<Q",b,pe+24+24)[0]==BASE
 secs=[]
 for i in range(cnt):
  j=pe+24+size+i*40
  virtual_size,va,rawsize,raw=struct.unpack_from("<IIII",b,j+8)
  secs.append((va,rawsize,raw))
 def off(rva):
  return next(raw+rva-va for va,n,raw in secs if va<=rva<va+n)
 for rva,hexbytes in OP.items():
  expected=bytes.fromhex(hexbytes)
  assert b[off(rva):off(rva)+len(expected)]==expected,f"RVA {rva:08x}"
 for rva,typ in RTTI.items():
  col=struct.unpack_from("<Q",b,off(rva-8))[0]-BASE
  sig,offset,cd,type_rva,ch,self_rva=struct.unpack_from("<IIIIII",b,off(col))
  assert sig==1 and offset==0 and self_rva==col
  p=off(type_rva)+16
  assert typ in b[p:b.index(b"\0",p)].decode("ascii")
 print("V416 19 original-EXE opcode preimages + 3 RTTI classes PASS")
 print("Native gameplay actor / runtime / pixel: OPEN")
if __name__=="__main__":
 if len(sys.argv)!=2:raise SystemExit("Usage: python test_v416_fgmodel_parent_exe_abi.py DarkSoulsRemastered.exe")
 verify(sys.argv[1])
