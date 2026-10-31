#!/usr/bin/env python3
"""V429 smoke verification on user-owned source zip; no compiled shaders copied."""
import hashlib,struct,sys,zipfile,zlib
from pathlib import Path
ARC='57314b36812dfa2867512052278a08dc0da8a41ea0414c1fbbe4ccdf45838076'
SAMPLE='FRPG_Phn_Dif________________HemEnvLerp.fpo'
PREF='PTDE/PTDE_FRPG_PHN_EXTRACTED/dvdbnd1_dcx_0000631D9390/'
MEMBER='DSR/shader/FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx'
def u32(b,i):return struct.unpack_from('<I',b,i)[0]
def find(z):
 c=z.read(MEMBER)
 assert hashlib.sha256(c).hexdigest()=='ad180732ac79d5d98783aa504c789c2bab15e515c3b0f66b237d8b8c69113394'
 b=zlib.decompress(c[76:]);assert b[:4]==b'BND3'
 assert len(b)==struct.unpack_from('>I',c,28)[0] and u32(b,16)==1989
 entries={}
 for i in range(u32(b,16)):
  f,size,off,idx,no,un=struct.unpack_from('<IIIIII',b,32+24*i)
  assert f==0x40 and idx==i and size==un
  name=b[no:b.index(b'\0',no)].decode('shift_jis','replace')
  d=b[off:off+size];assert d[:4]==b'DXBC'
  entries[name]=d
 return entries
if __name__=='__main__':
 if len(sys.argv)!=2:raise SystemExit('usage: verify_v429_shader_census_smoke.py DSRRL_FINAL_EVIDENCE_PART02.zip')
 path=Path(sys.argv[1]);assert hashlib.sha256(path.read_bytes()).hexdigest()==ARC
 with zipfile.ZipFile(path) as z:
  d=find(z);p=[n[len(PREF):] for n in z.namelist() if n.startswith(PREF) and n.endswith('.fpo')]
  assert len(d)==1989 and len({hashlib.sha256(x).digest() for x in d.values()})==1192
  assert len(p)==641 and all(n in d for n in p)
  a=d[SAMPLE];b=z.read(PREF+SAMPLE)
  assert hashlib.sha256(a).hexdigest()=='0d6d431986318f95e2f0ce376540062aa98c963bf4fe0a308cf72ed6fd8022c9'
  assert hashlib.sha256(b).hexdigest()=='d02573e70b90d3bb82e5b1dd060dd2ddc342a1869392157fb7da964e8da62997'
  s=None
  for i in range(u32(a,28)):
   o=u32(a,32+i*4)
   if a[o:o+4]==b'SHEX':s=a[o+8:o+8+u32(a,o+4)];break
  assert s
  w=struct.unpack_from('<%dI'%(len(s)//4),s)
  assert (w[1160]&0x7ff,w[1171]&0x7ff,w[1179]&0x7ff)==(50,14,14)
  assert (w[1167],w[1170],w[1176],w[1186])==(86,86,86,86)
  pw=struct.unpack_from('<%dI'%(len(b)//4),b)
  assert (pw[352]&0xffff,pw[355])==(5,0xa0e40056)
 print('V429 SMOKE PASS: 1989 DXBC, 1192 unique, 641 exact-name PTDE pairs; c86 MUL vs MAD/DIV; pixel OPEN')
