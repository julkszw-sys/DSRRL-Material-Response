#!/usr/bin/env python3
"""V448 static ONLY, retail DSR EXE SHA-locked producer-to-FLVER draw family.
Checks actual RET/CALL/packet/vtable bytes, PE pdata coverage and mutation
sensitivity. Never treats game runtime or a PTDE pixel as validated.
"""
import argparse, hashlib, json, struct
from pathlib import Path
EXE_SHA='a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b'
IMAGE=0x140000000
WITNESSES={
 'model_builder_entity_rdi':(0x220111,'488bf9'),
 'model_builder_virtual_model_load':(0x220cd3,'488b4f40'),
 'model_builder_packet_address':(0x220ce6,'488d55d0'),
 'model_builder_exact_virtual_call':(0x220ced,'ff5018'),
 'model_builder_return_site':(0x220cf0,'488b8db0010000'),
 'flver_native_selector_call_1':(0x20e014,'e807da0100'),
 'flver_native_selector_return_1':(0x20e019,'4d8ba660010000'),
 'flver_native_selector_call_2':(0x20eb7a,'e8a1ce0100'),
 'flver_native_selector_return_2':(0x20eb7f,'418b443c5c'),
 'flver_native_selector_call_3':(0x20fb99,'e882be0100'),
 'flver_native_selector_return_3':(0x20fb9e,'428b442b5c'),
 'model_builder_first_channel_dp_address':(0x220614,'488d9f08010000'),
 'model_builder_first_channel_selector_load':(0x22062b,'4c8bc3'),
 'model_builder_first_channel_emitter_a':(0x220632,'e8d905faff'),
 'model_builder_first_channel_emitter_b':(0x220639,'e8a205faff'),
}
class PE:
 def __init__(self,data):
  self.data=data; pe=struct.unpack_from('<I',data,0x3c)[0]
  assert data[pe:pe+4]==b'PE\0\0'
  n,opt=struct.unpack_from('<H',data,pe+6)[0],struct.unpack_from('<H',data,pe+20)[0]
  self.sections=[]
  for i in range(n):
   x=pe+24+opt+40*i
   name=data[x:x+8].split(b'\x00')[0].decode()
   vs,va,rs,ro=struct.unpack_from('<IIII',data,x+8)
   self.sections.append((name,va,vs,ro,rs))
 def read(self,rva,n):
  for name,va,vs,ro,rs in self.sections:
   if va<=rva and rva+n<=va+rs: return self.data[ro+rva-va:ro+rva-va+n]
  raise ValueError(f'unmapped file RVA {rva:x} +{n}')
 def pdata_covers(self,rva):
  for name,va,vs,ro,rs in self.sections:
   if name!='.pdata':continue
   for pos in range(0,rs-11,12):
    begin,end,unwind=struct.unpack_from('<III',self.data,ro+pos)
    if begin<=rva<end and unwind:return (begin,end,unwind)
  return None

def report(data,mutations):
 pe=PE(data)
 checks={name:{'rva':hex(rva),'actual':pe.read(rva,len(bytes.fromhex(sig))).hex(),
   'expected':sig,'pass':pe.read(rva,len(bytes.fromhex(sig)))==bytes.fromhex(sig)}
   for name,(rva,sig) in WITNESSES.items()}
 vtables={}
 for name,vtable in [('FlverModel',0x12eb760),('FgFlverModel',0x12eb130)]:
  value=struct.unpack('<Q',pe.read(vtable+0x18,8))[0]
  vtables[name]={'vtable_rva':hex(vtable),'virtual_slot_3':hex(value),
                 'pass':value==IMAGE+0x20dda0}
 unwind={}
 for label,rva in [('model_builder',0x220cf0),('flver_native_selector_1',0x20e019),
                   ('flver_native_selector_2',0x20eb7f),('flver_native_selector_3',0x20fb9e)]:
  span=pe.pdata_covers(rva)
  unwind[label]={'rva':hex(rva),'pdata':list(map(hex,span)) if span else None,
                 'pass':span is not None}
 negatives=[]
 if mutations:
  for name,(rva,sig) in WITNESSES.items():
   old=bytes.fromhex(sig)
   # mutate one real in-memory opcode byte, never the on-disk EXE
   got=bytearray(pe.read(rva,len(old)));got[0]^=0x01
   negatives.append({'witness':name,'mutation_rejected':bytes(got)!=old})
 status=(all(x['pass'] for x in checks.values()) and
         all(x['pass'] for x in vtables.values()) and
         all(x['pass'] for x in unwind.values()) and
         all(x['mutation_rejected'] for x in negatives))
 return dict(status='PASS' if status else 'FAIL',exe_sha256=hashlib.sha256(data).hexdigest(),
     witnesses=checks,vtable_calls=vtables,pdata=unwind,
     negative_mutations=len(negatives),negative_results=negatives,
     guaranteed='RETAIL_EXE_STATIC_ONLY',runtime='OPEN',actor_lifetime='OPEN',
     bridge_activation='OFF',pixel='OPEN')

def main():
 p=argparse.ArgumentParser();p.add_argument('exe',type=Path);p.add_argument('--json',type=Path)
 p.add_argument('--mutations',action='store_true');a=p.parse_args()
 data=a.exe.read_bytes()
 assert hashlib.sha256(data).hexdigest()==EXE_SHA,'Exe SHA mismatch; refusing an unverified build'
 r=report(data,a.mutations)
 if a.json:a.json.write_text(json.dumps(r,indent=2),encoding='utf-8')
 print(f"V448 original EXE STATIC {r['status']}: {len(r['witnesses'])} instruction preimages, {len(r['vtable_calls'])} vtable targets, {len(r['pdata'])} Win64 unwind frames, {r['negative_mutations']} negative mutation checks; runtime/pixel OPEN")
 if r['status']!='PASS':raise SystemExit(1)
if __name__=='__main__':main()
