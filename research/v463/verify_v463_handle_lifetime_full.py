#!/usr/bin/env python3
"""V463 SHA-locked native followup receiver identity and low24 table lookup."""
import argparse,hashlib,json
from pathlib import Path
from importlib.util import spec_from_file_location,module_from_spec
p=Path(__file__).resolve().parents[1]/'v462'/'verify_v462_registration_transport.py'
s=spec_from_file_location('v462',p);v=module_from_spec(s);s.loader.exec_module(v)
W={int(x,16):bytes.fromhex(y) for x,y in (line.split(':') for line in """11be816:83fa01
11be819:7563
11be81b:418b5810
11be851:448bc3
11be858:ff5008
11be85b:488bf8
11be85e:83c8ff
11be861:85db
11be863:7907
11be865:8bc3
11be867:25ffffff00
11be86c:4863c8
11be86f:498b4708
11be873:48893cc8
11be87e:418b4010
11be882:3bd8
11be884:741b
11be886:8bd8
11be888:83c8ff
11be88b:85db
11be88d:7907
11be88f:8bc3
11be891:25ffffff00
11be896:4863c8
11be899:498b4708
11be89d:488b3cc8
11be8a1:488b07
11be8a4:488bcf
11be8a7:ff5010
11b891e:44894110
21caa1:8b5310""".splitlines())}
def verify(b,checksha=True):
 if checksha:assert hashlib.sha256(b).hexdigest()==v.ORIGINAL,'EXE SHA mismatch'
 pe=v.PE(b)
 for r,raw in W.items():assert pe.read(r,len(raw))==raw,hex(r)
 return {'status':'STATIC_PASS','sha256':v.ORIGINAL,'opcode_witnesses':len(W),
 'create':'full handle R8D -> typed Seed factory -> entity+0x10; table indexed by low24',
 'followup':'if full DWORD handle matches EBX cached, reuse RDI; otherwise registry[low24] -> virtual+0x10',
 'generation_check_in_this_lookup_basic_block':False,
 'generation_check_downstream':'OPEN','gameplay_owner':'OPEN','pixel':'OPEN'}
def negatives(b):
 pe=v.PE(b);fail=[]
 for r in W:
  x=bytearray(b);x[pe.off(r)]^=1
  try:verify(x,False)
  except AssertionError:pass
  else:fail.append(hex(r))
 x=bytearray(b);x[-1]^=1
 try:verify(x,True)
 except AssertionError:pass
 else:fail.append('sha')
 return {'tested':len(W)+1,'unexpected_passes':fail,'status':'PASS' if not fail else 'FAIL'}
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('exe',type=Path);a.add_argument('--mutations',action='store_true')
 x=a.parse_args();b=x.exe.read_bytes();r=verify(b)
 if x.mutations:r['negative_controls']=negatives(b)
 print(json.dumps(r,indent=2))
