#!/usr/bin/env python3
"""V455 exact-original DSR PE64 STATIC RE: Seed ID producer & actor descriptor state.
No injected code. E8/E9 byte scans give candidates, not a complete CFG.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

ORIGINAL_SHA256 = "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
BASE = 0x140000000
# Immutable original-exe instruction preimages, each at an instruction boundary.
PREIMAGES = {
 0x1d2180:"488b05e1a6a901", 0x1d2187:"8b4010", 0x1d218a:"c3",
 0x157d08:"4d8bf0", 0x157d0b:"488bea", 0x157d11:"e86aa40700",
 0x157d1a:"448bf8", 0x157d20:"498b4a30", 0x157d24:"e837240601",
 0x157d2d:"448bc0", 0x157d35:"8bf8", 0x157ddd:"418bd7",
 0x157df6:"e8e52a0000", 0x157e00:"8bc7",
 0x21c550:"488b5318", 0x21c554:"e897b7f3ff", 0x21c559:"894310",
 0x11b3d50:"83c8ff", 0x11b3d53:"85d2", 0x11b3d55:"7907",
 0x11b3d57:"8bc2", 0x11b3d59:"25ffffff00", 0x11b3d5e:"488b4930",
 0x11b3d62:"4863d0", 0x11b3d65:"488b4108", 0x11b3d69:"488b04d0",
 0x326b74:"488bb170030000", 0x326b7b:"483bf2",
 0x326b80:"80b9b104000000", 0x326b87:"0f8439030000",
 0x326b8d:"80b98403000000", 0x326b94:"0f852c030000",
 0x326b9a:"4885d2", 0x326b9f:"807a5a00",
 0x326ba3:"0f841d030000", 0x326ba9:"48899170030000",
 0x326bfb:"0fbe570a", 0x326c02:"e819f7ffff",
 0x326c0a:"e811b50b00", 0x326c0f:"85c0", 0x326c11:"781c",
 0x326c13:"488b5360", 0x326c17:"0fbe8a93000000",
 0x326c1e:"3bc8", 0x326c20:"740d",
 0x326c22:"838a8400000020", 0x326c29:"888293000000",
 0x326f66:"410fbe560a", 0x326f6e:"e8adf3ffff",
 0x326f76:"e8351afdff", 0x326f7f:"488b5360",
 0x326f83:"0fbe8a93000000", 0x326f8a:"3bc8",
 0x326f8c:"740d", 0x326f8e:"838a8400000020",
 0x326f95:"888293000000",
 0x326e52:"488b9378030000", 0x326e59:"488bcb",
 0x326e5c:"e8effcffff",
 0x326d29:"c683b10400001e", 0x326d3b:"fec8",
 0x326d3d:"8883b1040000",
 0x32d448:"488bf1", 0x32d452:"488b4168",
 0x32d456:"488b7828",
 0x32d4b4:"488bd3", 0x32d4b7:"488bce",
 0x32d4ba:"e89196ffff", 0x32d4d8:"488bd3",
 0x32d4db:"488bce", 0x32d4de:"e80d9affff",
 0x32d50e:"488b96e0040000", 0x32d515:"488bce",
 0x32d518:"e83396ffff", 0x3205d0:"e83bce0000",
 0x320ebf:"488bcb", 0x320ecb:"4883c420",
 0x320ed0:"e93bc50000",
}

# This is a byte-level executable-section census, not proof that every hit
# is a reachable instruction. Unknown indirect calls remain outside scope.
EXPECTED = {
 0x157cf0:[0x21c554,0x21c71f],
 0x15a8e0:[0x157df6,0x4fbf21,0x4fdad1],
 0x1d2180:[0x156097,0x157389,0x157449,0x157bd2,0x157d11,
           0x157e37,0x157ef7,0x157fb7,0x158b17,0x159257,
           0x1593d7,0x159497,0x159567,0x159632,0x1597b2,
           0x1598a7,0x15a377,0x15a4d7,0x15a598,
           0x4f6b99,0x4fd816,0x4fec68],
 0x326b50:[0x31f05c,0x31f067,0x31f0b2,0x326e5c,
           0x32755c,0x32d4ba,0x32d518,0x33b677,
           0x33b72e,0x3ce10a,0x3ce200,0x3cede1,
           0x3cee3c,0x4af89c,0x4af90c,0x4af980],
 0x326ef0:[0x32d4de],
 0x326320:[0x326c02,0x326f6e,0x3270b1],
 0x326360:[],
 0x32d410:[0x3205d0],
}
EXPECTED_JUMP={0x32d410:[0x320ed0]}

class PE:
 def __init__(self,raw):
  self.raw=raw
  p=struct.unpack_from("<I",raw,0x3c)[0]
  assert raw[:2]==b"MZ" and raw[p:p+4]==bytes((80,69,0,0))
  assert struct.unpack_from("<H",raw,p+24)[0]==0x20b, "not PE32+"
  n=struct.unpack_from("<H",raw,p+6)[0]
  opt=struct.unpack_from("<H",raw,p+20)[0]
  self.sections=[]
  for i in range(n):
   s=p+24+opt+i*40
   size,rva,raw_size,pos=struct.unpack_from("<IIII",raw,s+8)
   flags=struct.unpack_from("<I",raw,s+36)[0]
   name=raw[s:s+8].split(bytes((0,)))[0].decode("ascii","replace")
   self.sections.append((name,rva,raw_size,pos,flags))
 def offset(self,rva,n):
  for name,base,size,pos,flags in self.sections:
   if base<=rva and rva+n<=base+size:
    return pos+rva-base
  raise AssertionError(f"unmapped {rva:#x}")
 def read(self,rva,n):
  start=self.offset(rva,n)
  return self.raw[start:start+n]
 def direct(self,target,opcode):
  out=[]
  for name,rva,size,pos,flags in self.sections:
   if not flags&0x20000000:continue
   data=self.raw[pos:pos+size]
   index=0
   while True:
    index=data.find(bytes((opcode,)),index)
    if index<0:break
    if index+5<=len(data):
     rel=struct.unpack_from("<i",data,index+1)[0]
     if rva+index+5+rel==target:out.append(rva+index)
    index+=1
  return sorted(out)
 def target(self,site,opcode):
  b=self.read(site,5)
  assert b[0]==opcode,(hex(site),"wrong opcode")
  return site+5+struct.unpack("<i",b[1:])[0]

def verify(raw, with_census=True, with_sha=True):
 if with_sha:
  assert hashlib.sha256(raw).hexdigest()==ORIGINAL_SHA256,"original EXE SHA mismatch"
 pe=PE(raw)
 for a,v in PREIMAGES.items():
  want=bytes.fromhex(v)
  assert pe.read(a,len(want))==want,f"byte preimage drift {a:#x}"
 # A single 11-byte getter: MOV RAX,[RIP+global]; MOV EAX,[RAX+0x10]; RET.
 assert pe.read(0x1d2180,11)==bytes.fromhex("488b05e1a6a9018b4010c3")
 assert 0x1d2187+struct.unpack("<i",pe.read(0x1d2183,4))[0]==0x141c6c868-BASE
 assert pe.target(0x157d11,0xe8)==0x1d2180
 assert pe.target(0x157d24,0xe8)==0x11ba160
 assert pe.target(0x157df6,0xe8)==0x15a8e0
 assert pe.target(0x21c554,0xe8)==0x157cf0
 assert pe.target(0x326c0a,0xe8)==0x3e2120
 assert pe.target(0x326f76,0xe8)==0x2f89b0
 assert pe.target(0x326e5c,0xe8)==0x326b50
 assert pe.target(0x3205d0,0xe8)==0x32d410
 assert pe.target(0x320ed0,0xe9)==0x32d410
 assert pe.target(0x32d4ba,0xe8)==0x326b50
 assert pe.target(0x32d4de,0xe8)==0x326ef0
 assert pe.target(0x32d518,0xe8)==0x326b50
 census={}
 if with_census:
  for dest,sites in EXPECTED.items():
   got=pe.direct(dest,0xe8)
   assert got==sites,(hex(dest),list(map(hex,got)),list(map(hex,sites)))
   census[hex(dest)]=[hex(s) for s in sites]
  for dest,sites in EXPECTED_JUMP.items():
   got=pe.direct(dest,0xe9)
   assert got==sites,(hex(dest),list(map(hex,got)),list(map(hex,sites)))
   census[hex(dest)+"_E9"]=[hex(s) for s in sites]
 return {
  "status":"CONFIRMED_STATIC","original_exe_sha256":ORIGINAL_SHA256,
  "preimages_checked":len(PREIMAGES),"census":census,
  "seed_plus_10_source":"read-only global pointer at RVA 0x1c6c868, value [+0x10], not a newly allocated ID in getter",
  "FrpgModel_plus_10_source":"return value from different call RVA 0x11ba160 in registration",
  "Seed_plus_10_uniqueness":"UNVERIFIED; NOT an authenticated gameplay actor/lifetime key",
  "Seed_resource_lookup":"0x11b3d50: negative IDs lower24; nonnegative sentinel -1; registry resource pointer, not certified actor",
  "owner_plus_370":"last accepted descriptor pointer at 0x326b50, guarded by +0x384, descriptor+0x5a",
  "same_descriptor_refresh":"owner+0x4b1 nonzero permits 0x326b50 re-evaluation without descriptor pointer change",
  "owner_plus_378":"independent descriptor passed by 0x326e52/0x326e5c into 0x326b50",
  "owner_plus_4e0":"fallback descriptor passed in 0x32d50e/0x32d518",
  "model_plus_93":"signed descriptor-returned byte updated only for valid nonnegative result and change; dirty flag model+0x84 |= 0x20",
  "actor_to_rx34":"OPEN","NPC_to_player_contamination":"OPEN","PTDE_pixel":"OPEN",
  "operator_changed":False,"addon_built":False,
 }

def negatives(raw):
 pe=PE(raw)
 bad=[]
 for addr in PREIMAGES:
  q=bytearray(raw)
  q[pe.offset(addr,1)]^=1
  try:verify(q,False,False)
  except AssertionError:pass
  else:bad.append(hex(addr))
 q=bytearray(raw);q[-1]^=1
 try:verify(q,False,True)
 except AssertionError:pass
 else:bad.append("SHA")
 return {"total":len(PREIMAGES)+1,"unrejected":bad,
         "status":"PASS" if not bad else "FAIL"}

if __name__=="__main__":
 parser=argparse.ArgumentParser()
 parser.add_argument("exe",type=Path)
 parser.add_argument("--negative",action="store_true")
 parser.add_argument("--json",type=Path)
 args=parser.parse_args()
 raw=args.exe.read_bytes()
 out=verify(raw)
 if args.negative:out["mutation_falsifiers"]=negatives(raw)
 result=json.dumps(out,indent=2)+"\n"
 if args.json:args.json.write_text(result,encoding="utf-8")
 print(result)
