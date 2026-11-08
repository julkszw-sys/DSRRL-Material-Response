#!/usr/bin/env python3
"""V454 SHA-locked original DSR EXE static actor/Seed routing extension of V453.
Read only; no game injection, no owner attribution from shared FLVER/context.
Requires sibling ../v453/verify_v453_writer_callsite_census.py in GitHub source tree.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,struct
parent=Path(__file__).resolve().parents[1]/"v453"/"verify_v453_writer_callsite_census.py"
spec=importlib.util.spec_from_file_location("v453_verified",parent)
v453=importlib.util.module_from_spec(spec);spec.loader.exec_module(v453)
BASE=0x140000000
WITNESS={
0x321ce0:"48895c2418",0x321d05:"488b4960",0x321d09:"4885c9",
0x321d1d:"488b01",0x321d28:"ff5028",0x20ba20:"0f2805a9f50d01",
0x34a173:"e8380fecff",0x34a17f:"49894d60",0x354bd4:"e8e794fcff",
0x157df6:"e8e52a0000",
0x4fbf0f:"488b4708",0x4fbf17:"4c8b4028",0x4fbf1b:"8b17",
0x4fbf1d:"488d4d17",0x4fbf21:"e8bae9c5ff",
0x4fdabf:"488b4708",0x4fdac7:"4c8b4028",0x4fdacb:"8b17",
0x4fdacd:"488d4d17",0x4fdad1:"e80acec5ff",
0x32d448:"488bf1",0x32d452:"488b4168",0x32d456:"488b7828",
0x32d483:"488b9fe8010000",0x32d495:"ff5010",
0x32d4b4:"488bd3",0x32d4b7:"488bce",0x32d4ba:"e89196ffff",
0x32d4d8:"488bd3",0x32d4db:"488bce",0x32d4de:"e80d9affff",
0x32d50e:"488b96e0040000",0x32d515:"488bce",0x32d518:"e83396ffff",
}
CALLS={0x34a173:0x20b0b0,0x354bd4:0x31e0c0,
0x157df6:0x15a8e0,0x4fbf21:0x15a8e0,0x4fdad1:0x15a8e0,
0x32d4ba:0x326b50,0x32d4de:0x326ef0,0x32d518:0x326b50}
VTS={0x131e008+0x120:0x321ce0,0x1322e68+0x120:0x321ce0,
0x13251f0+0x120:0x321ce0,0x12eaf30+0x28:0x20ba20,
0x1322e68+0x40:0x349fd0,0x13251f0+0x40:0x355890}
RTTI={0x131e008:".?AVChrIns@NS_FRPG@@",
0x1322e68:".?AVEnemyIns@NS_FRPG@@",
0x13251f0:".?AVPlayerIns@NS_FRPG@@",
0x12eaf30:".?AVChrModel@NS_FRPG@@",
0x12cc308:".?AVFrpgModelDrawEntitySeed@NS_FRPG@@"}
def u64(p,r):return struct.unpack("<Q",p.read(r,8))[0]
def rtti(p,vt):
 col=u64(p,vt-8)-BASE
 sig,off,cd,td,cl,rv=struct.unpack("<IIIIII",p.read(col,24))
 assert sig==1 and rv==col
 return p.read(td+16,128).split(bytes((0,)))[0].decode("ascii"),off
def verify(raw,checksha=True):
 if checksha:assert hashlib.sha256(raw).hexdigest()==v453.SHA
 old=v453.audit(raw,checksha)
 p=v453.PE(raw)
 for addr,hexb in WITNESS.items():
  assert p.read(addr,len(bytes.fromhex(hexb)))==bytes.fromhex(hexb),("opcode",hex(addr))
 for site,dest in CALLS.items():
  b=p.read(site,5)
  assert b[0]==0xe8 and site+5+struct.unpack("<i",b[1:])[0]==dest,("call",hex(site))
 for slot,dest in VTS.items():assert u64(p,slot)==BASE+dest,("slot",hex(slot))
 for vt,name in RTTI.items():assert rtti(p,vt)==(name,0),("rtti",hex(vt))
 assert p.calls(0x15a8e0)==[0x157df6,0x4fbf21,0x4fdad1], "extra Seed constructor call"
 assert p.calls(0x20b0b0)==[0x34a173], "ChrModel ctor direct E8 census"
 return {"status":"CONFIRMED_STATIC","original_sha256":v453.SHA,
 "prior_v453":old["status"],"exact_preimages":len(WITNESS),
 "direct_call_edges":len(CALLS),"RTTI":len(RTTI),"vtable_slots":len(VTS),
 "chrins_shared_method":"ChrIns/EnemyIns/PlayerIns vtable+0x120=0x321CE0 reads owner+0x60; typed ChrModel+0x28=0x20BA20",
 "player_ctor_base":"0x354BD4 -> ChrIns 0x31E0C0",
 "enemy_model_store":"0x34A17F EnemyIns+0x60=ChrModel*",
 "player_exact_ChrModel_store":"OPEN",
 "seed_constructor_calls":["0x157DF6","0x4FBF21","0x4FDAD1"],
 "alternate_seed_scope":"0x4FBF21/0x4FDAD1 use stack-local Seed and bypass 0x157CF0; gameplay owner unknown",
 "owner_descriptor_router":"0x32D4BA -> 0x326B50, 0x32D4DE -> 0x326EF0, owner+0x4E0 -> 0x32D518 -> 0x326B50",
 "runtime_owner_to_rx34":"OPEN","NPC_row31_contamination":"OPEN","PTDE_pixel":"OPEN"}
def mutations(raw):
 p=v453.PE(raw)
 sites=sorted(set(WITNESS)|set(CALLS)|set(VTS)|{x-8 for x in RTTI})
 fail=[]
 for site in sites:
  q=bytearray(raw)
  # Unique PE file offset from original mapped sections
  loc=None
  for name,rva,sz,off,flags in p.sections:
   if rva<=site<rva+sz:loc=off+site-rva;break
  assert loc is not None
  q[loc]^=1
  try:verify(q,False)
  except (AssertionError,ValueError,IndexError):pass
  else:fail.append(hex(site))
 q=bytearray(raw);q[-1]^=1
 try:verify(q,True)
 except AssertionError:pass
 else:fail.append("SHA")
 return {"attempted":len(sites)+1,"unrejected":fail,"status":"PASS" if not fail else "FAIL"}
if __name__=="__main__":
 a=argparse.ArgumentParser();a.add_argument("exe",type=Path);a.add_argument("--mutations",action="store_true")
 a.add_argument("--json",type=Path);args=a.parse_args()
 raw=args.exe.read_bytes();out=verify(raw)
 if args.mutations:out["mutations"]=mutations(raw)
 output=json.dumps(out,ensure_ascii=False,indent=2)+"\n"
 if args.json:args.json.write_text(output,encoding="utf-8")
 print(output)
