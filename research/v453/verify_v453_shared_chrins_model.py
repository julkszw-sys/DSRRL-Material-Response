#!/usr/bin/env python3
"""V453 exact original retail DSR PE: PlayerIns/EnemyIns shared ChrIns model field.
Read-only, file SHA-locked; never certifies instance lifetime, actor row or pixels.
"""
import argparse,hashlib,json,struct
from pathlib import Path
IMAGE_BASE=0x140000000
EXE_SHA='a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b'
PREIMAGES={
 0x321cff:'488bf9',0x321d05:'488b4960',0x321d09:'4885c9',
 0x321d12:'48837f6800',0x321d1d:'488b01',0x321d28:'ff5028',
 0x20ba20:'0f2805a9f50d01',0x20ba27:'488bc2',0x20ba2a:'0f2902',
 0x349fec:'4c8be9',0x34a101:'498bcd',0x34a104:'e8b750fdff',
 0x34a109:'488bc8',0x34a10c:'e8cfd9ffff',0x34a111:'488bd8',
 0x34a16c:'488b5328',0x34a170:'498bc9',0x34a173:'e8380fecff',
 0x34a178:'488bc8',0x34a17f:'49894d60',
 0x20b152:'e859120100',0x21c4f3:'48897b18',
 0x21c554:'e897b7f3ff',0x21c559:'894310',
 0x326364:'488b4960',0x326368:'0fbe8193000000',
 0x326373:'83898400000020',0x32637a:'889193000000',
 0x320980:'4c8b4160',0x320987:'4183888400000001',
 0x320993:'488b5160',0x320997:'838a8400000002',
 0x157d11:'e86aa40700',0x157df6:'e8e52a0000',
 0x15a97e:'897e10',
}
E8={0x34a104:0x31f1c0,0x34a10c:0x347ae0,
    0x34a173:0x20b0b0,0x20b152:0x21c3b0,
    0x21c554:0x157cf0,0x157d11:0x1d2180,
    0x157df6:0x15a8e0}
VT={
 0x1322e68+0x120:0x321ce0,
 0x13251f0+0x120:0x321ce0,
 0x131e008+0x120:0x321ce0,
 0x12eaf30+0x28:0x20ba20,
 0x1322e68+0x40:0x349fd0,
 0x13251f0+0x40:0x355890,
 0x131e008+0x40:0x31ef90,
 0x1322e68+0xa8:0x320980,
 0x13251f0+0xa8:0x357a20,
 0x131e008+0xa8:0x320980,
}
RTTI={
 0x131e008:'.?AVChrIns@NS_FRPG@@',
 0x1322e68:'.?AVEnemyIns@NS_FRPG@@',
 0x13251f0:'.?AVPlayerIns@NS_FRPG@@',
 0x12eaf30:'.?AVChrModel@NS_FRPG@@',
 0x12ec838:'.?AVFrpgModel@NS_FRPG@@',
 0x12cc308:'.?AVFrpgModelDrawEntitySeed@NS_FRPG@@',
}
class PE:
 def __init__(self,data):
  assert data[:2]==b'MZ'
  self.data=data
  pe=struct.unpack_from('<I',data,0x3c)[0]
  assert data[pe:pe+4]==bytes((80,69,0,0))
  assert struct.unpack_from('<H',data,pe+24)[0]==0x20b
  n=struct.unpack_from('<H',data,pe+6)[0]
  opt=struct.unpack_from('<H',data,pe+20)[0]
  self.sections=[]
  for j in range(n):
   off=pe+24+opt+40*j
   size,rva,raw_size,raw_off=struct.unpack_from('<IIII',data,off+8)
   self.sections.append((rva,raw_size,raw_off))
 def ofs(self,rva,n):
  for a,s,o in self.sections:
   if a<=rva and rva+n<=a+s:return o+rva-a
  raise AssertionError('unmapped RVA '+hex(rva))
 def read(self,rva,n):
  off=self.ofs(rva,n)
  return self.data[off:off+n]
 def u64(self,rva):return struct.unpack('<Q',self.read(rva,8))[0]
 def e8(self,rva):
  b=self.read(rva,5)
  assert b[0]==0xe8,hex(rva)
  return rva+5+struct.unpack('<i',b[1:])[0]
 def rtti(self,vtable):
  col=self.u64(vtable-8)-IMAGE_BASE
  sign,suboff,cd,type_rva,class_rva,self_rva=struct.unpack('<IIIIII',self.read(col,24))
  assert sign==1 and self_rva==col
  return self.read(type_rva+16,128).split(bytes((0,)))[0].decode('ascii'),suboff

def verify(data,full_sha=True):
 digest=hashlib.sha256(data).hexdigest()
 if full_sha:assert digest==EXE_SHA,'original EXE hash mismatch'
 p=PE(data)
 for r,s in PREIMAGES.items():
  assert p.read(r,len(bytes.fromhex(s)))==bytes.fromhex(s),'instruction '+hex(r)
 for r,expected in E8.items():assert p.e8(r)==expected,'call '+hex(r)
 for r,expected in VT.items():assert p.u64(r)==IMAGE_BASE+expected,'vtable '+hex(r)
 for r,expected in RTTI.items():assert p.rtti(r)==(expected,0),'RTTI '+hex(r)
 return {
  'status':'STATIC_PASS','exe_sha256':digest,
  'aligned_preimages':len(PREIMAGES),'direct_call_edges':len(E8),
  'vtable_cells':len(VT),'rtti_classes':len(RTTI),
  'verified':'EnemyIns, PlayerIns, ChrIns share vtable slot+0x120 => 0x321CE0 reads this+0x60; EnemyIns constructs ChrModel into +0x60; ChrModel+0x28 implements interface used by shared slot',
  'nonuniform_virtual_method':'Slot+0xA8: EnemyIns and ChrIns 0x320980; PlayerIns uses different method 0x357A20; cannot globalize model update policy across roles',
  'model_registration':'ChrModel->FrpgModel ctor, model+0x18 stores external model resource pointer; no proof of backpointer owner in FrpgModel or Seed',
  'player_model_creator':'OPEN/OBFUSCATED','player_slot60_dynamic_type':'RUNTIME_OPEN',
  'npc_row31_is_user_player_pixels':'UNPROVEN','actor_generation':'OPEN',
  'receiver_pixel_coverage':'OPEN','runtime':'NOT_TESTED','ptde_pixel':'OPEN'}

def negative_mutations(data):
 p=PE(data);sites=sorted(set(PREIMAGES)|set(E8)|set(VT)|set(x-8 for x in RTTI))
 unrejected=[]
 for r in sites:
  mutation=bytearray(data);mutation[p.ofs(r,1)]^=1
  try:verify(mutation,False)
  except (AssertionError,UnicodeDecodeError,ValueError,IndexError,struct.error):continue
  unrejected.append(hex(r))
 mutation=bytearray(data);mutation[-1]^=1
 try:verify(mutation,True)
 except AssertionError:pass
 else:unrejected.append('whole_file_sha')
 return {'total':len(sites)+1,'unrejected':unrejected,'status':'PASS' if not unrejected else 'FAIL'}

if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('exe',type=Path);ap.add_argument('--mutations',action='store_true');ap.add_argument('--json',type=Path)
 a=ap.parse_args();raw=a.exe.read_bytes();res=verify(raw)
 if a.mutations:res['mutations']=negative_mutations(raw)
 txt=json.dumps(res,ensure_ascii=False,indent=2)+'\n'
 if a.json:a.json.write_text(txt,encoding='utf-8')
 print(txt)
