#!/usr/bin/env python3
"""V461 original DSR SHA-locked read-only actor/model/seed/draw entity ID ABI.
Does not assert FrpgModel+0x10==DrawEntity+0x10 without registration transport.
"""
from pathlib import Path
import argparse,hashlib,json,struct
BASE=0x140000000
SHA='a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b'
W={
 0x1d2180:'488b05e1a6a901',0x1d2187:'8b4010',0x1d218a:'c3',
 0x157d11:'e86aa40700',0x157d1a:'448bf8',0x157d24:'e837240601',
 0x157d35:'8bf8',0x157d8d:'4c8bc5',0x157ddd:'418bd7',
 0x157df6:'e8e52a0000',0x157e00:'8bc7',
 0x15a97e:'897e10',0x15a9a7:'48896e20',
 0x21c550:'488b5318',0x21c554:'e897b7f3ff',0x21c559:'894310',
 0x21caa1:'8b5310',0x21cac7:'e8f45f0000',
 0x223ad9:'418bf8',0x223ae2:'4c8b02',0x223b03:'4c8bce',
 0x223b06:'448bc7',0x223b0f:'e8ecbdffff',
 0x21f928:'498bd9',0x21f92e:'e87d34fdff',0x21f93e:'488b4320',
 0x21f942:'48894740',0x21f9be:'488d8f08010000',
 0x21f9c5:'e8c67afbff',
 0x1f2dcd:'498bf9',0x1f2dd6:'e8355bfc00',0x1f2de6:'8b5710',
 0x1f2dec:'e85f0ffc00',0x1f2df1:'48894620',
 0x11b8910:'488d05c1dc4400',0x11b8917:'48895108',
 0x11b891e:'44894110',0x11b8922:'c7411401000000',
 0x11b8929:'418b4108',0x11b892d:'894118',
 0x11b8930:'418b410c',0x11b8934:'89411c',
 0x34a173:'e8380fecff',0x34a17f:'49894d60',
 0x321d05:'488b4960',0x321d28:'ff5028',
}
EDGES={0x157d11:0x1d2180,0x157d24:0x11ba160,0x157df6:0x15a8e0,
 0x21c554:0x157cf0,0x21cac7:0x222ac0,0x223b0f:0x21f900,
 0x21f92e:0x1f2db0,0x21f9c5:0x1d7490,0x1f2dd6:0x11b8910,
 0x1f2dec:0x11b3d50,0x34a173:0x20b0b0}
class PE:
 def __init__(self,raw):
  self.raw=raw;off=struct.unpack_from('<I',raw,0x3c)[0]
  assert raw[:2]==b'MZ' and raw[off:off+4]==b'PE\0\0'
  count=struct.unpack_from('<H',raw,off+6)[0]
  opt=struct.unpack_from('<H',raw,off+20)[0]
  self.sections=[]
  for i in range(count):
   h=off+24+opt+40*i
   size,rva,rsz,pos=struct.unpack_from('<IIII',raw,h+8)
   self.sections.append((rva,rsz,pos))
 def off(self,rva,n=1):
  for start,size,off in self.sections:
   if start<=rva and rva+n<=start+size:return off+rva-start
  raise ValueError(f'unmapped RVA {rva:x}')
 def read(self,rva,n):return self.raw[self.off(rva,n):self.off(rva,n)+n]
 def direct(self,rva):
  b=self.read(rva,5);assert b[0]==0xe8
  return rva+5+struct.unpack_from('<i',b,1)[0]
def verify(raw,checksum=True):
 if checksum:assert hashlib.sha256(raw).hexdigest()==SHA,'wrong retail EXE'
 p=PE(raw)
 for addr,hexb in W.items():
  expected=bytes.fromhex(hexb);assert p.read(addr,len(expected))==expected,('witness',hex(addr))
 for addr,dest in EDGES.items():assert p.direct(addr)==dest,('direct',hex(addr))
 # 0x1D2180 is precisely two loads and RET, not a per-call ID allocator.
 assert p.read(0x1d2180,11)==bytes.fromhex('488b05e1a6a9018b4010c3')
 assert p.read(0x11b891e,4)==bytes.fromhex('44894110')
 return {'version':'V461','status':'STATIC_PASS','original_exe_sha256':SHA,
  'opcode_witnesses':len(W),'direct_E8_edges':len(EDGES),
  'seed_context':{'source':'0x1D2180 global pointer [RIP+...] -> dword global_object+0x10',
    'not_fresh_per_call_generator':True,
    'transport':'0x157D11 -> R15D -> Seed+0x10 (0x15A97E) -> 0x1F2DE6 lookup -> entity+0x20'},
  'model_registration_handle':{'source':'0x157D24 calls registration helper, returns EAX -> EDI at 0x157D35',
    'write':'FrpgModel+0x10 at 0x21C559',
    'message_routing':'FrpgModel+0x10 loaded into EDX at 0x21CAA1 before 0x804C emission'},
  'render_entity_receiver_handle':{'producer':'factory 0x223AC0 R8D into 0x21F900 and 0x1F2DB0',
    'write':'0x11B891E stores R8D in native receiver+0x10',
    'other':'receiver+0x08 stores RDX transport descriptor via 0x11B8917',
    'seed_lookups':'seed+0x10 (global context ID) into receiver+0x20 resource lookup'},
  'producer_handle_equals_receiver_handle_for_six_RX34_entities':'OPEN: cannot compare before capturing both handles in session',
  'actor_identity':'OPEN','pixel_equivalence':'OPEN'}
def negatives(raw):
 p=PE(raw);fails=[]
 for addr in sorted(set(W)|set(EDGES)):
  b=bytearray(raw);b[p.off(addr)]^=1
  try:verify(b,False)
  except AssertionError:pass
  else:fails.append(hex(addr))
 b=bytearray(raw);b[-1]^=1
 try:verify(b,True)
 except AssertionError:pass
 else:fails.append('SHA')
 return {'tests':len(set(W)|set(EDGES))+1,'unexpected_passes':fails,'status':'PASS' if not fails else 'FAIL'}
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('exe',type=Path);a.add_argument('--mutations',action='store_true');a.add_argument('--json',type=Path)
 x=a.parse_args();b=x.exe.read_bytes();r=verify(b)
 if x.mutations:r['mutation_controls']=negatives(b)
 s=json.dumps(r,indent=2,ensure_ascii=False)+'\n'
 if x.json:x.json.write_text(s)
 print(s)
