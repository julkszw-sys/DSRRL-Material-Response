#!/usr/bin/env python3
"""V451 exact retail DSR PE64 static model resource descriptor->draw entity graph.
Read-only and self-contained. Does NOT certify actual gameplay actor identity.
"""
import argparse,hashlib,json,struct
from pathlib import Path
BASE=0x140000000
SHA='a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b'
OPC={
0x21c3d3:'488d055e040d01',0x21c3df:'897110',0x21c4f3:'48897b18',
0x21c550:'488b5318',0x21c554:'e897b7f3ff',0x21c559:'894310',
0x21caa1:'8b5310',0x21cac7:'e8f45f0000',0x223adf:'488bf1',
0x223b03:'4c8bce',0x223b0f:'e8ecbdffff',0x21f928:'498bd9',
0x21f93e:'488b4320',0x21f942:'48894740',0x21f9be:'488d8f08010000',
0x1f2dcd:'498bf9',0x1f2de6:'8b5710',0x1f2de9:'488bcb',
0x1f2dec:'e85f0ffc00',0x1f2df1:'48894620',0x11b3d50:'83c8ff',
0x11b3d59:'25ffffff00',0x11b3d5e:'488b4930',0x11b3d65:'488b4108',
0x11b3d69:'488b04d0',0x220198:'488b4720',0x2201ae:'f30f104058',
0x22024c:'488d8f10010000'}
CALLS={0x21c554:0x157cf0,0x21cac7:0x222ac0,0x223b0f:0x21f900,0x1f2dec:0x11b3d50}
class PE:
 def __init__(self,raw):
  self.raw=raw
  assert raw[:2]==b'MZ'
  h=struct.unpack_from('<I',raw,0x3c)[0]
  assert raw[h:h+4]==bytes((80,69,0,0))
  count=struct.unpack_from('<H',raw,h+6)[0]
  o=struct.unpack_from('<H',raw,h+20)[0]
  self.sections=[]
  for i in range(count):
   off=h+24+o+i*40
   sz,rva,rawsz,rawpos=struct.unpack_from('<IIII',raw,off+8)
   self.sections.append((rva,rawsz,rawpos))
 def at(self,rva,n):
  for base,sz,off in self.sections:
   if base<=rva and rva+n<=base+sz:
    return off+rva-base
  raise AssertionError('unmapped RVA '+hex(rva))
 def read(self,rva,n):return self.raw[self.at(rva,n):self.at(rva,n)+n]
 def u64(self,rva):return struct.unpack('<Q',self.read(rva,8))[0]
 def call(self,rva):
  b=self.read(rva,5)
  assert b[0]==0xe8
  return rva+5+struct.unpack('<i',b[1:])[0]
 def rtti(self,v):
  col=self.u64(v-8)-BASE
  sig,off,cd,td,cl,self_rva=struct.unpack('<IIIIII',self.read(col,24))
  assert sig==1 and self_rva==col
  return self.read(td+16,128).split(bytes((0,)))[0].decode('ascii'),off
def verify(data,sha=True):
 if sha:assert hashlib.sha256(data).hexdigest()==SHA,'nonoriginal EXE SHA'
 p=PE(data)
 for r,b in OPC.items():assert p.read(r,len(bytes.fromhex(b)))==bytes.fromhex(b),'opcode '+hex(r)
 for r,t in CALLS.items():assert p.call(r)==t,'call '+hex(r)
 assert p.rtti(0x12ec838)==('.?AVFrpgModel@NS_FRPG@@',0)
 assert p.rtti(0x12eca10)==('.?AVFrpgModelDrawEntity@NS_FRPG@@',0)
 assert p.u64(0x12ec858)==BASE+0x21c970
 assert p.u64(0x12eca30)==BASE+0x223380
 return {'result':'PASS','witnesses':len(OPC),'calls':len(CALLS),
  'rtti':2,'scope':'descriptor -> drawEntity model-resource only',
  'frpgmodel_descriptor_identity':'UNVERIFIED','gameplay_actor':'OPEN','pixel':'OPEN'}
def mutate(raw):
 p=PE(raw); sites=sorted(set(OPC)|set(CALLS)|{0x12ec830,0x12eca08,0x12ec858,0x12eca30})
 bad=[]
 for r in sites:
  q=bytearray(raw);q[p.at(r,1)]^=1
  try:verify(q,False)
  except AssertionError:continue
  bad.append(hex(r))
 q=bytearray(raw);q[-1]^=1
 try:verify(q,True)
 except AssertionError:pass
 else:bad.append('sha')
 return {'total':len(sites)+1,'unrejected':bad,'result':'PASS' if not bad else 'FAIL'}
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('exe',type=Path);a.add_argument('--mutations',action='store_true');args=a.parse_args()
 data=args.exe.read_bytes();out=verify(data)
 if args.mutations:out['mutations']=mutate(data)
 print(json.dumps(out,indent=2))
