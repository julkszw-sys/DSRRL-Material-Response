#!/usr/bin/env python3
"""V458 read-only original-EXE typed FrpgModelDrawEntity Light-transition consumer verification.
Requires research/v457/verify_v457_selector_consumer.py from the same GitHub branch.
Never modifies the EXE; never claims runtime actor/row or PTDE pixel equivalence.
"""
import argparse,hashlib,importlib.util,json,struct
from pathlib import Path
source=Path(__file__).resolve().parents[1]/"v457"/"verify_v457_selector_consumer.py"
spec=importlib.util.spec_from_file_location("v457_static",source)
prior=importlib.util.module_from_spec(spec);spec.loader.exec_module(prior)
BASE=0x140000000
W=dict((int(a,16),b) for a,b in (line.split(":") for line in """c04a0:0fb701
c04a3:668902
c0550:0fb74102
c0554:668902
1c0be0:4883ec38
1c0be4:4d8bc8
1c0bed:4c8b4140
1c0bf1:488b4960
1c0bf5:e8065d0000
1c6900:4055
1c6930:498d4908
1c693f:e85c9befff
1c694d:440fbf38
1c6956:440fbf28
1c695f:453bfd
1c6962:0f8540010000
1c697b:41b805000000
1c6986:e8b5313a00
1c6aa8:f30f1085d0000000
1c6ada:f3410f107e0c
1c6af7:41b805000000
1c6b00:e83b303a00
1c6b1f:410fb7460a
1c6b32:41b805000000
1c6b3d:e8fe2f3a00
1c6b5c:f30f117c2428
1c6b75:e866cc3900
1c6b9e:0f59ce
1c6be1:0f284d10
1c6be8:0f298760120000
1c6bf6:0f298f70120000
1c6c04:0f298740120000
1d7490:48894c2408
1d74a6:488d055be71001
1d74ad:488901
1d74b0:4883c108
1d74b7:e894020000
1d74c4:e887020000
1d74d1:e87a020000
1d7580:48895c2410
1d758a:488bf9
1d7592:4883c108
1d75aa:e871020000
1d75af:488d4f18
1d75c0:e85b020000
1d75ca:488d4f28
1d75db:e840020000
1d7820:0f57c0
1d782f:440fb74102
1d7834:0fb702
1d7837:66443bc0
1d784d:f30f104908
1d7888:0fb702
1d7890:c74108000080bf
1d789f:0fb702
20b546:e825140100
219ad0:4053
219b17:498d8b08010000
219b2b:e850dafbff
21ca5b:40f6c7e0
21ca6c:e88f2b0000
21ca7c:e83f2b0000
21ca89:e8b22b0000
21cac7:e8f45f0000
21f900:488bc4
21f92e:e87d34fdff
21f934:488d05d5d00c01
21f93b:488907
21f93e:488b4320
21f9be:488d8f08010000
21f9c5:e8c67afbff
21f9ca:90
22008c:e83f000000
2200d0:4c8bdc
220114:488b4950
220147:488b4720
220158:f6473920
220246:8b81b4010000
22024c:488d8f10010000
220256:e84502eaff
22025b:488d8f10010000
220262:0fb710
220265:6689551c
22026e:e8dd02eaff
220273:f30f108714010000
22027f:0fb708
22028b:f30f114520
2202a5:66894d1e
220610:0fb64739
220614:488d9f08010000
22062b:4c8bc3
220632:e8d905faff
220639:e8a205faff
22063e:4c8bc3
220647:e8f405faff
22064c:4c8bc3
220655:e80606faff
222ad2:ba4c800000
222b60:4053
222b77:f30f100b
222ba7:498d8b08010000
222bbb:e8c049fbff
222d30:4053
222d49:4881c108010000
222d5f:e8cc47fbff
222d64:488d8b64010000
222d73:4883c430
223380:4053
223386:8d82ba7fffff
223391:83f812
22339a:488d0d5fccddff
2233a1:448b8c81c8352200
2233a9:4c03c9
2233ac:41ffe1
22346c:e8eff6ffff
2235e0:66342200""".splitlines()))
EDGES={
 0x20b546:0x21c970,0x21cac7:0x222ac0,
 0x21f92e:0x1f2db0,0x21f9c5:0x1d7490,
 0x1d74b7:0x1d7750,0x1d74c4:0x1d7750,0x1d74d1:0x1d7750,
 0x22346c:0x222b60,0x222bbb:0x1d7580,
 0x1d75aa:0x1d7820,0x1d75c0:0x1d7820,0x1d75db:0x1d7820,
 0x22008c:0x2200d0,0x220256:0xc04a0,0x22026e:0xc0550,
 0x220639:0x1c0be0,0x1c0bf5:0x1c6900,
 0x1c693f:0xc04a0,0x1c6951:0xc0550,
 0x1c6986:0x569b40,0x1c6b00:0x569b40,0x1c6b3d:0x569b40,
 0x1c6b75:0x5637e0,0x222d5f:0x1d7530,0x219b2b:0x1d7580}
VT={0x12eca10:(".?AVFrpgModelDrawEntity@NS_FRPG@@",0x20,0x223380),
    0x12e5c08:(".?AVFrpgDrawParam@NS_FRPG@@",0x0,0x1d76f0)}
def rtti(p,vt):
 col=struct.unpack("<Q",p.read(vt-8,8))[0]-BASE
 sig,off,cd,td,cl,rev=struct.unpack("<IIIIII",p.read(col,24))
 assert sig==1 and rev==col,("COL",hex(vt))
 return p.read(td+16,180).split(b"\0")[0].decode("ascii","replace"),off,td+16
def verify(raw,check_sha=True,census=True):
 if check_sha:assert hashlib.sha256(raw).hexdigest()==prior.EXE_SHA,"Wrong retail DSR EXE"
 p=prior.PE(raw)
 for a,hexb in W.items():
  x=bytes.fromhex(hexb);assert p.read(a,len(x))==x,("instruction",hex(a))
 for a,d in EDGES.items():assert p.call(a)==d,("call",hex(a),hex(d))
 for vt,(name,slot,dest) in VT.items():
  got,offset,_=rtti(p,vt);assert got==name and offset==0,(hex(vt),got)
  assert struct.unpack("<Q",p.read(vt+slot,8))[0]==BASE+dest,("virtual slot",hex(vt),hex(slot))
 assert 0x804c-0x8046==6
 assert 0x2235c8+4*6==0x2235e0 and struct.unpack("<I",p.read(0x2235e0,4))[0]==0x223466
 if census:
  assert p.raw_e8(0x1d7580)==[0xc0240,0x219b2b,0x222bbb]
  assert p.raw_e8(0x1d7490)==[0xbe8c6,0x215a29,0x21f9c5,0x574270,0x704f4a]
 return {"status":"STATIC_PASS","original_SHA256":prior.EXE_SHA,
 "opcode_witnesses":len(W),"E8_edges":len(EDGES),"RTTI_types":len(VT),"vtable_slots":len(VT),
 "command":"0x804C -> FrpgModelDrawEntity vtable+0x20 -> 0x223380",
 "embedded":"FrpgDrawParam entity+0x108 initialized by 0x1D7490",
 "light_slots":{"current":"entity+0x110","target":"entity+0x112","alpha":"entity+0x114","duration":"entity+0x118","elapsed":"entity+0x11C"},
 "CPU_draw_builder":"0x2200D0 retrieves two Light IDs and alpha",
 "light_consumer":"0x220639 -> 0x1C0BE0 -> 0x1C6900 category 5 local Light resource resolve",
 "CPU_payload":"state+0x1240..+0x12B0",
 "actor_player_vs_NPC":"OPEN","RX34_GPU_CB":"OPEN","PTDE_pixel":"OPEN"}
def controls(raw):
 p=prior.PE(raw);positions=set(W)|set(EDGES)
 for vt,(_,slot,_) in VT.items():
  positions.add(vt-8);positions.add(vt+slot);positions.add(rtti(p,vt)[2])
 fails=[]
 for a in sorted(positions):
  q=bytearray(raw);q[p.off(a)]^=1
  try:verify(q,False,False)
  except (AssertionError,UnicodeDecodeError,ValueError):continue
  fails.append(hex(a))
 q=bytearray(raw);q[-1]^=1
 try:verify(q,True,False)
 except AssertionError:pass
 else:fails.append("SHA")
 return {"attempted":len(positions)+1,"false_accepts":fails,"status":"PASS" if not fails else "FAIL"}
if __name__=="__main__":
 a=argparse.ArgumentParser();a.add_argument("exe",type=Path);a.add_argument("--mutations",action="store_true")
 args=a.parse_args();b=args.exe.read_bytes();r=verify(b)
 if args.mutations:r["negative_controls"]=controls(b)
 print(json.dumps(r,indent=2))
