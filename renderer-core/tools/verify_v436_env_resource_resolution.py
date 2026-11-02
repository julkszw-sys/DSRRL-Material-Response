#!/usr/bin/env python3
"""V436 compact SHA-locked static ENV resource resolver census; no binary patch."""
import hashlib,json,struct,sys
from pathlib import Path
SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
CALL={
 0x1eb622:0x563f90,0x1eb66a:0x563f90,0x1eb6db:0x564000,0x1eb721:0x564000,
 0x1eb63f:0x1ead10,0x1eb682:0x1ead10,0x1eb6f8:0x1ead10,0x1eb739:0x1ead10,
 0x1eb6a3:0xbadc0,0x1eb6c9:0xbadc0,0x1eb758:0xbadc0,0x1eb77b:0xbadc0,
 0x1eaf68:0x563e70,0x1eb11e:0x563f00,0x1ead25:0x587150,
}
REF={0x563eb3:0x137d4a8,0x563ec9:0x12e9038,0x563f43:0x137d4e8,
     0x563f59:0x12e9018,0x563fc8:0x137d528,0x563fe6:0x12e9db0,
     0x564030:0x137d560,0x1ead46:0x12b3a78}
NAMES={
 0x12e9038:"EnvDif_%s_%03d",0x12e9018:"EnvSpc_%s_%03d",
 0x137d4a8:"EnvDif_%s_%03d_%1d",0x137d4e8:"EnvSpc_%s_%03d_%1d",
 0x12e9db0:"GI_EnvDif_%s_%02d_%04d",
 0x137d528:"GI_EnvDif_%s_%02d_%04d_%1d",
 0x137d560:"GI_EnvSpc_%s_%02d_%04d_%d",
 0x137d4d0:"envLight",0x137d510:"envLightSpc",
 0x12b3a78:"SYSTEX_INVALID"
}
OPS={0x563f9a:"b8ad8bdb68",0x563fb2:"69c210270000",
     0x56400f:"b8ad8bdb68",0x564027:"69c210270000",
     0x1ead1e:"488b8908230000",0x1ead38:"8b4010",
     0x1eb6a8:"4489bb54230000",0x1eb75d:"89bb58230000",
     0xbadd5:"443984b1d4000000",0xbadf5:"c784b118010000ffffffff"}
def verify(path):
 data=Path(path).read_bytes()
 assert hashlib.sha256(data).hexdigest()==SHA,"original EXE SHA mismatch"
 pe=struct.unpack_from("<I",data,0x3c)[0]
 assert data[pe:pe+4]==b"PE\\0\\0" and struct.unpack_from("<H",data,pe+24)[0]==0x20b
 n=struct.unpack_from("<H",data,pe+6)[0]
 sh=pe+24+struct.unpack_from("<H",data,pe+20)[0]
 sections=[struct.unpack_from("<IIII",data,sh+40*i+8)[1:] for i in range(n)]
 # sections tuple is (rva, size_raw, ptr_raw)
 def at(rva,size):
  for va,rawsize,raw in sections:
   if va<=rva and rva+size<=va+rawsize:return data[raw+rva-va:raw+rva-va+size]
  raise AssertionError("RVA not mapped: "+hex(rva))
 for site,value in OPS.items():assert at(site,len(value)//2)==bytes.fromhex(value),hex(site)
 for site,target in CALL.items():
  b=at(site,5);assert b[0]==0xe8 and site+5+struct.unpack_from("<i",b,1)[0]==target,hex(site)
 for site,target in REF.items():
  b=at(site,7);assert b[:3]==b"\\x48\\x8d\\x15" and site+7+struct.unpack_from("<i",b,3)[0]==target,hex(site)
 for rva,expected in NAMES.items():
  b=at(rva,220)
  s=b.decode("utf-16le").split("\\0")[0]
  assert s==expected,(hex(rva),s)
 assert struct.unpack("<I",at(0x563fb4,4))[0]==10000
 assert struct.unpack("<I",at(0x564029,4))[0]==10000
 return {"version":"V436","exe_sha":SHA,"calls":len(CALL),"literals":len(NAMES),
  "rip_references":len(REF),"code_checks":len(OPS),
  "meaning":"A/B selects environment diffuse/specular resource IDs through per-context name lookup, not a proven physical LightBank row",
  "runtime":"OPEN","Source1_RX34_join":"OPEN","PTDE_pixels":"OPEN"}
if __name__=="__main__":
 print(json.dumps(verify(sys.argv[1]),indent=2))
