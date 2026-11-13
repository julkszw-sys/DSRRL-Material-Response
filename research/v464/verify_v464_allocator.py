#!/usr/bin/env python3
"""V464 original retail DSR EXE allocator; no mutation of the game EXE."""
import argparse,hashlib,json
from pathlib import Path
from importlib.util import spec_from_file_location,module_from_spec
p=Path(__file__).resolve().parents[1]/'v461'/'verify_v461_owner_handle.py'
s=spec_from_file_location('v461',p);v=module_from_spec(s);s.loader.exec_module(v)
W={int(x,16):bytes.fromhex(y) for x,y in (line.split(':') for line in """11ba161:895c2408
11ba16a:488b4120
11ba175:83caff
11ba17b:ff5010
11ba17e:4863531c
11ba182:448b4b18
11ba186:413bd1
11ba18b:488b4310
11ba18f:4c630c90
11ba193:8d4201
11ba196:89431c
11ba199:488b4308
11ba19f:4e8d04c8
11ba1a3:4a8b04c8
11ba1a7:a801
11ba1ab:8bd0
11ba1ad:418bd9
11ba1b0:81e3ffffff00
11ba1b6:81e2000000ff
11ba1bc:0bda
11ba1be:0fbaeb1f
11ba1c2:4883f003
11ba1c6:498900
11ba1d2:8bc3
11be865:8bc3
11be867:25ffffff00
11be86f:498b4708
11be873:48893cc8
11be891:25ffffff00
11be89d:488b3cc8
11b891e:44894110""".splitlines())}
def verify(b,check_sha=True):
 if check_sha:assert hashlib.sha256(b).hexdigest()==v.SHA
 pe=v.PE(b)
 for r,x in W.items():assert pe.read(r,len(x))==x,hex(r)
 return {'status':'STATIC_PASS','sha256':v.SHA,'witnesses':len(W),
         'handle':'(slotIndex & 0x00ffffff) | (slotEntry & 0xff000000) | 0x80000000',
         'high8_monotonic_generation':'NOT_PROVEN','actor_RX34':'OPEN','pixel':'OPEN'}
def negatives(b):
 pe=v.PE(b);bad=[]
 for r in W:
  x=bytearray(b);x[pe.off(r)]^1
  try:verify(x,False)
  except AssertionError:pass
  else:bad.append(hex(r))
 x=bytearray(b);x[-1]^1
 try:verify(x,True)
 except AssertionError:pass
 else:bad.append('SHA')
 return {'attempted':len(W)+1,'unexpected_passes':bad,'status':'PASS' if not bad else 'FAIL'}
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('exe',type=Path);a.add_argument('--mutations',action='store_true');x=a.parse_args()
 b=x.exe.read_bytes();r=verify(b)
 if x.mutations:r['negative_controls']=negatives(b)
 print(json.dumps(r,indent=2))
