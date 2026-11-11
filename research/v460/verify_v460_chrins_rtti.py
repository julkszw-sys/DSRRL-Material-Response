#!/usr/bin/env python3
"""SHA-locked, read-only native MSVC RTTI census for ChrIns and NpcTalkIns.
Does not modify the retail EXE or draw/rendering. No gameplay actor classification.
"""
import argparse,hashlib,json,re,struct
from pathlib import Path
SHA='a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b'
EXPECTED={
 'ChrIns':('0x16499c0',['ChrIns','FieldInsBase']),
 'EnemyIns':('0x164a808',['EnemyIns','ChrIns','FieldInsBase']),
 'PlayerIns':('0x164ad48',['PlayerIns','ChrIns','FieldInsBase']),
 'ReplayGhostIns':('0x164b0d8',['ReplayGhostIns','PlayerIns','ChrIns','FieldInsBase']),
}
class PE:
 def __init__(self,b):
  self.b=b;hdr,=struct.unpack_from('<I',b,0x3c)
  assert b[:2]==b'MZ' and b[hdr:hdr+4]==b'PE\0\0'
  sections,=struct.unpack_from('<H',b,hdr+6)
  optsz,=struct.unpack_from('<H',b,hdr+20)
  self.sec={}
  for i in range(sections):
   x=hdr+24+optsz+40*i
   name=b[x:x+8].split(b'\0')[0].decode('ascii')
   _,rva,rawsz,rawoff=struct.unpack_from('<IIII',b,x+8)
   self.sec[name]=(rva,rawsz,rawoff)
 def read(self,rva,n):
  for start,size,off in self.sec.values():
   if start<=rva and rva+n<=start+size:return self.b[off+rva-start:off+rva-start+n]
  raise ValueError('Unmapped RVA %x'%rva)
 def data(self,name):
  rva,size,off=self.sec[name];return rva,self.b[off:off+size]
def census(data,sha_check=True):
 if sha_check:assert hashlib.sha256(data).hexdigest()==SHA,'Original EXE SHA mismatch'
 p=PE(data);db,ds=p.data('.data');rb,rs=p.data('.rdata');td={}
 for m in re.finditer(rb'\.\?A[VU][A-Za-z0-9_$@?+]+@@',ds):
  i=m.start();name=m.group().decode('ascii','replace')
  if i>=16 and i+len(m.group())<len(ds) and ds[i+len(m.group())]==0:
   td[db+i-16]=name
 loc=[]
 for i in range(0,len(rs)-24,4):
  r=rb+i;s,offset,cd,t,ch,selfr=struct.unpack_from('<IIIIII',rs,i)
  if s!=1 or selfr!=r or t not in td:continue
  try:
   _,_,n,arr=struct.unpack('<IIII',p.read(ch,16))
   if n>64:continue
   bases=[]
   for j in range(n):
    ptr,=struct.unpack('<I',p.read(arr+j*4,4))
    tptr,=struct.unpack('<I',p.read(ptr,4))
    bases.append(td.get(tptr,'?'+hex(tptr)))
  except (ValueError,struct.error):continue
  loc.append((r,td[t],offset,bases))
 family=[x for x in loc if '.?AVChrIns@NS_FRPG@@' in x[3]]
 assert len(family)==4,('Census changed',family)
 found={}
 for r,typed,offset,bases in family:
  name=typed.removeprefix('.?AV').split('@')[0]
  assert name in EXPECTED
  er,parents=EXPECTED[name]
  assert r==int(er,16) and offset==0
  assert bases==['.?AV'+x+'@NS_FRPG@@' for x in parents]
  found[name]={'COL_RVA':hex(r),'inheritance':parents}
 assert set(found)==set(EXPECTED)
 npc=[x for x in loc if x[1]=='.?AVNpcTalkIns@NS_FRPG@@']
 assert len(npc)==0 and data.count(b'NpcTalkIns')==1
 assert any('DbgMenuEzCbItem@VNpcTalkIns' in x[1] for x in loc)
 return {'status':'STATIC_PASS','EXE_SHA':SHA,'type_descriptors':len(td),
  'complete_object_locators':len(loc),'ChrIns_family':found,
  'NpcTalkIns_raw_mentions':1,'NpcTalkIns_debug_template_RTTI':'OBSERVED',
  'NpcTalkIns_standalone_RTTI':'NOT_OBSERVED',
  'talking_NPC_is_EnemyIns':'OPEN','ChrModel_owner_NPC_type':'OPEN',
  'runtime_actor_generation':'OPEN','GPU_RX34_pixel':'OPEN'}
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('exe',type=Path)
 b=a.parse_args();print(json.dumps(census(b.exe.read_bytes()),indent=2))
