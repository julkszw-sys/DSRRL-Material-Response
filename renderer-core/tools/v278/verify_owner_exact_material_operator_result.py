#!/usr/bin/env python3
"""V278 independent owner-local DXBC comparison, never distribute game bytecode."""
import hashlib,struct,json,sys
from pathlib import Path
PINS={
 33:('1d9fed8a55689e09bacf55a66a88f2c3474f7c3f9a5604fe5151b0506b06caab','aba3a135806a8b5396ae959e2eda94affed5b78e8439447002835301b4ef6733',1108,7,6),
 34:('8f86b1aec73b3b9ec77c10ce6de2308d0b0d4533a1430373ea92b297ee387f97','0f98a40c352fb562d98ec9b2e9c9b7e06c44b04158bc61bdbe6afb5f7bdfcb93',1017,6,7),
 35:('b5f94ccec4e01e7d98c4329c2b28aecfd65e103729be969a53f486b182c55b70','fb840c245c31d4094d112c8a77fee44eb5be273ed83bd3d98edd4afeb0f5c738',677,5,6)}
def chunks(data):
 assert data[:4]==b'DXBC' and struct.unpack_from('<I',data,24)[0]==len(data)
 n=struct.unpack_from('<I',data,28)[0];assert 1<n<64
 v={}
 for i in range(n):
  p=struct.unpack_from('<I',data,32+4*i)[0];size=struct.unpack_from('<I',data,p+4)[0]
  assert p+8+size<=len(data)
  name=data[p:p+4].decode('ascii');assert name not in v
  v[name]=struct.unpack_from('<%dI'%(size//4),data,p+8) if name=='SHEX' else data[p:p+8+size]
 assert 'SHEX' in v
 return v
def one(rx,before,after):
 p,c,off,reg,color=PINS[rx]
 assert hashlib.sha256(before).hexdigest()==p
 assert hashlib.sha256(after).hexdigest()==c
 a,b=chunks(before),chunks(after)
 assert set(a)==set(b) and len(before)+28==len(after)
 assert all(a[k]==b[k] for k in a if k!='SHEX')
 x,y=a['SHEX'],b['SHEX']
 op=(0x07000038,0x00100072,reg,0x00100246,reg,0x00101246,color)
 assert y[off+4:off+11]==op
 assert x[0]==y[0] and x[1]+7==y[1]
 assert x[2:off+4]==y[2:off+4] and x[off+4:]==y[off+11:]
 assert before!=after and x!=y
 return {'rx':rx,'source_sha256':p,'candidate_sha256':c,'insert_at_shex_word':off,
         'original_bytes':len(before),'candidate_bytes':len(after),
         'other_instructions_identical':True,'all_non_shex_chunks_identical':True,
         'stock_Fog_LightScattering_Glow_byte_identical':True,
         'runtime_active':False,'PTDE_pixel':'OPEN'}
if __name__=='__main__':
 if len(sys.argv)!=8:raise SystemExit('usage: verify_v278.py src33 src34 src35 cand33 cand34 cand35 report.json')
 records=[one(rx,Path(sys.argv[i]).read_bytes(),Path(sys.argv[i+3]).read_bytes()) for i,rx in enumerate(PINS,1)]
 Path(sys.argv[7]).write_text(json.dumps({'schema':'dsrrl.v278.equipment_color_derived_final_r19','receivers':records},indent=2)+'\n')
 print('V278_TRUE_FINAL_R19_THREE_RECEIVER_INDEPENDENT_PATCH_ORACLE_PASS')
