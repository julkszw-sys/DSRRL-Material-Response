#!/usr/bin/env python3
"""V440-R4 exact dispatch mode flags; only default paths emit native LB type5/6."""
import json,struct,sys
from pathlib import Path
from verify_v440_native_channel import PE,verify
from verify_v440_entity_rtti_dispatch import check as rtti
OP={
0x216204:"8bba9c000000",0x216213:"857918",
0x216232:"0fbae70c",0x216248:"40f6c711",
0x21625e:"f7c7000000c0",0x216276:"0fbae712",
0x216285:"f7c700006000",
0x220014:"8bb29c000000",0x220023:"857118",
0x220047:"0fbae60c",0x220054:"f7c6000000c0",
0x220063:"40f6c611",0x220070:"0fbae612",
0x22007d:"f7c600006000"}
EDGES={
0x216241:0x218640,0x216257:0x217370,
0x21626f:0x217970,0x21628d:0x217dc0,0x216294:0x2162d0,
0x22004d:0x221a40,0x22005c:0x2211c0,
0x220069:0x220d60,0x220076:0x221c10,
0x220085:0x221450,0x22008c:0x2200d0}
def check(b,sha=True):
 if sha:verify(b)
 rtti(b,sha=False);p=PE(b)
 for r,w in OP.items():assert p.read(r,len(bytes.fromhex(w)))==bytes.fromhex(w),hex(r)
 for r,d in EDGES.items():
  c=p.read(r,5);assert c[0]==0xe8 and r+5+struct.unpack("<i",c[1:])[0]==d,hex(r)
 return {"original_two_actor_dispatchers":"CONFIRMED_STATIC",
 "shared_input":"context[+0x9c] and actor[+0x18] gate",
 "asm_order":["bit12:0x218640","mask0x11:0x217370",
 "bits30,31:0x217970","bit18:skip","bits21,22:0x217dc0",
 "default:0x2162d0 type5 native"],
 "model_order":["bit12:0x221a40","bits30,31:0x2211c0",
 "mask0x11:0x220d60","bit18:0x221c10","bits21,22:0x221450",
 "default:0x2200d0 type5/type6 native"],
 "branch_preimages":len(OP),"direct_targets":len(EDGES),
 "live_camera_context_flags":"OPEN","issued_RX34":"OPEN","pixels":"OPEN"}
if __name__=="__main__":print(json.dumps(check(Path(sys.argv[1]).read_bytes()),indent=2))
