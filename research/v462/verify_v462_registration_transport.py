#!/usr/bin/env python3
"""V462 original retail DSR: SHA-locked typed FrpgModel handle create command.
No EXE modification. No in-game actor identity or pixel claim.
The portable V461 PE mapper is a reproducible research dependency.
"""
import argparse,hashlib,importlib.util,json,struct
from pathlib import Path
original='a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b'
src=Path(__file__).resolve().parents[1]/'v461'/'verify_v461_owner_handle.py'
spec=importlib.util.spec_from_file_location('v461_re',src)
prior=importlib.util.module_from_spec(spec)
spec.loader.exec_module(prior)
W={int(a,16):bytes.fromhex(v) for a,v in (s.split(':') for s in """157d20:498b4a30
157d24:e837240601
157d2d:448bc0
157d30:ba01000000
157d35:8bf8
157d3a:498b5958
157d3e:488bcb
157d41:e8cab90501
157d58:488913
157d5b:49c70008000000
157d74:e837b90501
157ddd:418bd7
157df6:e8e52a0000
157e00:8bc7
21c550:488b5318
21c554:e897b7f3ff
21c559:894310
21caa1:8b5310
11b3710:443b4110
11b3714:44894110
11b3718:895114
11b371b:7507
11b371d:0fbaea10
11b3721:895114
11b36b0:488b01
11b36c1:482b5108
11b36d1:48c1ea02
11b36d5:48c1e211
11b36d9:0b5114
11b36dc:8910
11b36e7:0fbae210
11b36ed:8b4110
11b36f0:418900
11b36f8:c7411400000000
11be7c1:4d8b4610
11be7c5:498b4018
11be7c9:8b10
11be7d3:440fb7d2
11be7d7:45895014
11be7e2:8bc2
11be7e4:48c1e811
11be7e8:488d0481
11be7ec:49894008
11be7f0:0fbae210
11be7f6:418b01
11be7f9:41894010
11be80e:4d8b4610
11be812:418b5014
11be816:83fa01
11be81b:418b5810
11be831:498b5610
11be84e:488b01
11be851:448bc3
11be854:498b5608
11be858:ff5008
11be85b:488bf8
11be865:8bc3
11be867:25ffffff00
11be86c:4863c8
11be86f:498b4708
11be873:48893cc8
11be87e:418b4010
11be896:4863c8
11be899:498b4708
11be89d:488b3cc8
223ad9:418bf8
223b06:448bc7
223b0f:e8ecbdffff
21f92e:e87d34fdff
1f2dd6:e8355bfc00
11b8917:48895108
11b891e:44894110""".splitlines())}
E={0x157d24:0x11ba160,0x157d41:0x11b3710,0x157d74:0x11b36b0,
   0x157df6:0x15a8e0,0x21c554:0x157cf0,0x223b0f:0x21f900,
   0x21f92e:0x1f2db0,0x1f2dd6:0x11b8910}
def verify(b,sha=True):
 if sha:assert hashlib.sha256(b).hexdigest()==original,'wrong retail EXE SHA'
 p=prior.PE(b)
 for r,v in W.items():assert p.read(r,len(v))==v,('witness',hex(r))
 for r,v in E.items():assert p.direct(r)==v,('call',hex(r))
 assert struct.unpack('<Q',p.read(0x12cc310,8))[0]==0x140223ac0
 assert p.read(0x157d29,17)==bytes.fromhex('488b4b08448bc0ba010000008bf84c8b09')
 for n in (0,1,10,67):
  for cached in (False,True):
   header=(n<<17)|1|(0x10000 if cached else 0)
   assert (header>>17)==n and (header&0xffff)==1 and bool(header&0x10000)==cached
 return {'status':'STATIC_PASS','exe_sha256':original,'opcode_witnesses':len(W),
         'direct_E8_edges':len(E),'typed_seed_factory_slot':'0x12CC308+0x8 -> 0x223AC0',
         'producer':'0x157D24 allocator EAX to producer FrpgModel+0x10 and serialized handle +0x10',
         'consumer':'0x11BE7C1..858 command type1, handle cached via bit16, seed virtual factory R8D',
         'receiver':'0x223AC0 -> 0x21F900 -> 0x11B8910 writes entity+0x10',
         'registry':'0x11BE873 entity pointer stored at table[handle&0xFFFFFF]',
         'runtime_six_RX34_handle_values':'NOT_CAPTURED',
         'PlayerIns_EnemyIns':'OPEN','GPU_pixel':'OPEN','PTDE_pixel':'OPEN'}
def negatives(b):
 p=prior.PE(b);places=set(W)|set(E)|{0x12cc310};bad=[]
 for r in sorted(places):
  q=bytearray(b);q[p.off(r)]^=1
  try:verify(q,False)
  except AssertionError:continue
  bad.append(hex(r))
 q=bytearray(b);q[-1]^=1
 try:verify(q,True)
 except AssertionError:pass
 else:bad.append('SHA')
 return {'tests':len(places)+1,'unexpected_passes':bad,'status':'PASS' if not bad else 'FAIL'}
if __name__=='__main__':
 a=argparse.ArgumentParser()
 a.add_argument('exe',type=Path);a.add_argument('--mutations',action='store_true')
 args=a.parse_args();b=args.exe.read_bytes();r=verify(b)
 if args.mutations:r['negative_controls']=negatives(b)
 print(json.dumps(r,indent=2,ensure_ascii=False))
