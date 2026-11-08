#!/usr/bin/env python3
"""V452 read-only native ChrIns ownership / LightBank writer static verifier.
Runs AFTER exact SHA-locked V451 FrpgModel->typed Seed PE verification.
Never modifies exe. Positive proof is static structure, not gameplay-owner join.
"""
import argparse,hashlib,json,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"v451"))
from verify_v451_native_model_resource import PE,BASE,SHA,verify as verify_v451

EXTRA={
 0x349620:"4055",0x3496a8:"488bcf",0x3496b1:"488d05b097fd00",
 0x3496b8:"488907",0x349fec:"4c8be9",0x34a16c:"488b5328",
 0x34a170:"498bc9",0x34a173:"e8380fecff",0x34a178:"488bc8",
 0x34a17f:"49894d60",0x20b0b0:"48894c2408",
 0x20b152:"e859120100",0x20b158:"488d05d1fd0d01",
 0x20b15f:"488906",0x157e00:"8bc7",
 0x11b3d50:"83c8ff",0x11b3d53:"85d2",0x11b3d55:"7907",
 0x11b3d57:"8bc2",0x11b3d59:"25ffffff00",
 0x326324:"4c8b4160",0x326328:"410fbe8892000000",
 0x326339:"41818884000000e0000000",0x32634c:"41888892000000",
 0x326364:"488b4960",0x326368:"0fbe8193000000",
 0x326373:"83898400000020",0x32637a:"889193000000",
 0x326bfb:"0fbe570a",0x326c02:"e819f7ffff",
 0x326c0a:"e811b50b00",0x326c17:"0fbe8a93000000",
 0x326c22:"838a8400000020",0x326c29:"888293000000",
 0x326f66:"410fbe560a",0x326f6e:"e8adf3ffff",
 0x326f76:"e8351afdff",0x326f83:"0fbe8a93000000",
 0x326f8e:"838a8400000020",0x326f95:"888293000000",
 0x3270a9:"410fbe560a",0x3270b1:"e86af2ffff",
 0x3270c6:"0fbe5004",0x3270d2:"0fbe8193000000",
 0x3270dd:"83898400000020",0x3270e4:"889193000000",
 0x3e2124:"440fbf4108",0x3e2129:"ba05000000",
 0x3e2131:"e88ad61300",0x3e2140:"0fbe440104",
 0x2f89b4:"440fbf4108",0x2f89b9:"ba01000000",
 0x2f89c1:"e8fa6d2200",0x2f89d0:"0fbe440104",
 0x51f7c6:"83fa0b",0x51f7cb:"4585c0",
 0x51f7d6:"443b84c1c8010000",0x51f7e0:"418b84c2cc010000",
 0x51f7e8:"488b4948",0x51f7f1:"486354810c",0x51f7fe:"498bc1"
}
CALLS={0x3496ab:0x31e0c0,0x34a173:0x20b0b0,
 0x20b152:0x21c3b0,0x326c02:0x326320,0x326c0a:0x3e2120,
 0x326f6e:0x326320,0x326f76:0x2f89b0,0x3270b1:0x326320,
 0x3e2131:0x51f7c0,0x2f89c1:0x51f7c0}
TYPES={
 0x131e008:".?AVChrIns@NS_FRPG@@",0x1322e68:".?AVEnemyIns@NS_FRPG@@",
 0x13251f0:".?AVPlayerIns@NS_FRPG@@",0x12eaf30:".?AVChrModel@NS_FRPG@@"}
VTABLE={0x1322e68+0x40:0x349fd0,0x13251f0+0x40:0x355890,
 0x131e008+0x40:0x31ef90,0x12cc308+8:0x223ac0}
def inspect(raw,with_sha=True):
 if with_sha and hashlib.sha256(raw).hexdigest()!=SHA: raise AssertionError("Original EXE SHA mismatch")
 result=verify_v451(raw,sha=False)
 p=PE(raw)
 for pos,expected in EXTRA.items():
  if p.read(pos,len(bytes.fromhex(expected)))!=bytes.fromhex(expected):
   raise AssertionError("opcode mismatch "+hex(pos))
 for pos,target in CALLS.items():
  if p.call(pos)!=target: raise AssertionError("call mismatch "+hex(pos))
 for pos,name in TYPES.items():
  if p.rtti(pos)!=(name,0): raise AssertionError("RTTI mismatch "+hex(pos))
 for pos,target in VTABLE.items():
  if p.u64(pos)!=BASE+target: raise AssertionError("vtable mismatch "+hex(pos))
 return {"result":"STATIC_PASS","original_sha256":hashlib.sha256(raw).hexdigest(),
         "v451_parent_result":result["result"],"extra_instructions":len(EXTRA),
         "extra_calls":len(CALLS),"extra_rtti":len(TYPES),"extra_vtable":len(VTABLE),
         "NPC_owner":"EnemyIns+0x60 -> ChrModel*","PlayerIns_model_field":"OPEN",
         "categories":[1,5],"gameplay_actor_to_RX34":"OPEN","pixel":"OPEN"}
def mutations(raw):
 p=PE(raw);sites=set(EXTRA)|set(CALLS)|set(VTABLE)|set(v-8 for v in TYPES);bad=[]
 for pos in sorted(sites):
  data=bytearray(raw);data[p.at(pos,1)]^=1
  try:inspect(data,False)
  except AssertionError:continue
  bad.append(hex(pos))
 data=bytearray(raw);data[-1]^=1
 try:inspect(data,True)
 except AssertionError:pass
 else:bad.append("source_sha")
 return {"attempts":len(sites)+1,"unrejected":bad,"status":"PASS" if not bad else "FAIL"}
if __name__=="__main__":
 parser=argparse.ArgumentParser();parser.add_argument("exe",type=Path);parser.add_argument("--mutations",action="store_true")
 opts=parser.parse_args();raw=opts.exe.read_bytes();r=inspect(raw)
 if opts.mutations:r["mutation_tests"]=mutations(raw)
 print(json.dumps(r,indent=2))
