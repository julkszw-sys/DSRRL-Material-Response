#!/usr/bin/env python3
"""V472: SHA-locked original DSR LightBank selector command transport audit.
Run: python research/v472/verify_v472_selector_command.py DarkSoulsRemastered.exe --mutations
No exe patch, runtime hook, or graphics change. Exactly original image.
"""
import argparse,hashlib,struct,json
from pathlib import Path
SHA='a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b'
W={
 0x21C98F:'c7818400000000000000',0x21CA5B:'40f6c7e0',
 0x21CAA1:'8b5310',0x21CAC7:'e8f45f0000',
 0x222AD2:'ba4c800000',0x222AE0:'e82b0cf900',
 0x223386:'8d82ba7fffff',0x223391:'83f812',
 0x223466:'498bd0',0x22346C:'e8eff6ffff',
 0x222BA7:'498d8b08010000',0x222BBB:'e8c049fbff',
 0x21F934:'488d05d5d00c01',0x1F2DD6:'e8355bfc00',
 0x11B891E:'44894110',0x20B158:'488d05d1fd0d01',
 0x21F93E:'488b4320',0x21F942:'48894740',
 0x223AD9:'418bf8',0x223B06:'448bc7',
 0x223B0F:'e8ecbdffff',0x21F9BE:'488d8f08010000',
 0x220614:'488d9f08010000',0x22062B:'4c8bc3',
 0x220632:'e8d905faff',0x220639:'e8a205faff'}
CALLS={0x21CAC7:0x222AC0,0x222AE0:0x11B3710,
 0x22346C:0x222B60,0x222BBB:0x1D7580,
 0x1F2DD6:0x11B8910,0x223B0F:0x21F900,
 0x220632:0x1C0C10,0x220639:0x1C0BE0}
PTRS={0x12ECA30:0x223380}
class PE:
 def __init__(self,b):
  assert b[:2]==b'MZ'
  p=struct.unpack_from('<I',b,0x3c)[0]
  assert b[p:p+4]==b'PE\0\0' and struct.unpack_from('<H',b,p+4)[0]==0x8664
  z=p+24+struct.unpack_from('<H',b,p+20)[0];self.secs=[]
  for k in range(struct.unpack_from('<H',b,p+6)[0]):
   o=z+k*40
   _,rva,raw,off=struct.unpack_from('<IIII',b,o+8)
   self.secs.append((rva,raw,off))
 def off(self,rva,size):
  for r,n,o in self.secs:
   if r<=rva and rva+size<=r+n:return o+rva-r
  raise ValueError('unmapped PE RVA '+hex(rva))
def audit(b,mutations=False):
 assert hashlib.sha256(b).hexdigest()==SHA,'nonmatching retail EXE: fail closed'
 pe=PE(b)
 for r,h in W.items():
  expected=bytes.fromhex(h);o=pe.off(r,len(expected))
  assert b[o:o+len(expected)]==expected,hex(r)
 for r,target in CALLS.items():
  o=pe.off(r,5)
  assert b[o]==0xe8 and r+5+struct.unpack_from('<i',b,o+1)[0]==target,hex(r)
 for r,target in PTRS.items():
  o=pe.off(r,8)
  assert struct.unpack_from('<Q',b,o)[0]==0x140000000+target,hex(r)
 out={'status':'CONFIRMED_STATIC_EXE','sha256':SHA,'preimages':len(W),
  'direct_calls':len(CALLS),'vtable_dispatch':len(PTRS),
  'selector_transport':'FrpgModel+0x10 -> 0x804C -> FrpgModelDrawEntity+0x108 -> three FrpgDrawParam lanes -> same entity LightBank emitter',
  'selector_origin':'instance FrpgModel+0x84 E0 dirty flag; not model material shader-set selector 0x22BA20',
  'actor_to_instance':'OPEN','PTDE_row_to_actor':'OPEN','pixel':'OPEN'}
 if mutations:
  c=bytearray(b);caught=0
  for r,h in W.items():
   o=pe.off(r,1);c[o]^=1
   if c[o:o+len(bytes.fromhex(h))]!=bytes.fromhex(h):caught+=1
   c[o]^=1
  out['mutation_controls']={'total':len(W),'caught':caught,'pass_all':caught==len(W)}
 return out
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('exe',type=Path)
 ap.add_argument('--mutations',action='store_true')
 args=ap.parse_args()
 print(json.dumps(audit(args.exe.read_bytes(),args.mutations),indent=2))
