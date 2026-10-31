#!/usr/bin/env python3
"""V430 SHA-pinned owner-source read-only RX34 MTD material regression smoke."""
import hashlib,struct,sys,zipfile,zlib
from pathlib import Path
ZIP_SHA='57314b36812dfa2867512052278a08dc0da8a41ea0414c1fbbe4ccdf45838076'
P_SHA='eefeefa39946779ab2a45fde28a8d7aa67e2b8ccc749e7b48b51e002059c0c92'
D_SHA='ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b'
def mtds(z,p):
 c=z.read(p);assert c[:4]==b'DCX\0';a=zlib.decompress(c[76:]);assert a[:4]==b'BND3';out={}
 for i in range(struct.unpack_from('<I',a,16)[0]):
  flag,size,off,idx,no,un=struct.unpack_from('<IIIIII',a,32+i*24)
  assert flag==64 and idx==i and size==un
  name=a[no:a.index(b'\0',no)].decode('shift_jis','replace')
  out[name]=a[off:off+size]
 return out
def scalar(blob,name):
 p=blob.index(name.encode()+b'\xa3')
 t=blob.index(b'\xca\0\0\0',p,p+100)
 assert struct.unpack_from('<I',blob,t+4)[0]==1
 return struct.unpack_from('<f',blob,t+8)[0]
def path(blob):
 start=blob.index(b'N:\\FRPG\\data\\Material\\spx\\')
 return blob[start:blob.index(b'\0',start)].decode()
def check(archive):
 assert hashlib.sha256(Path(archive).read_bytes()).hexdigest()==ZIP_SHA
 with zipfile.ZipFile(archive) as z:
  d=mtds(z,'DSR/mtd/Mtd.mtdbnd.dcx')
  p=mtds(z,'PTDE/PTDE_EXTRACTED/mtd/dvdbnd1_E101C004_C_DB_.mtd.dcx')
  patch=mtds(z,'DSR/mtd/MtdPatch.mtdbnd.dcx')
 assert (len(p),len(d),len(set(p)&set(d)),len(set(d)-set(p)),len(set(p)-set(d)))==(435,580,435,145,0)
 assert 'P_Metal[DSB].mtd' not in patch
 a,b=d['P_Metal[DSB].mtd'],p['P_Metal[DSB].mtd']
 assert hashlib.sha256(a).hexdigest()==D_SHA and hashlib.sha256(b).hexdigest()==P_SHA
 assert path(a)==path(b) and path(a).endswith('FRPG_Phn_ColDifSpcBmp.spx')
 assert (scalar(a,'g_SpecularMapColorPower'),scalar(b,'g_SpecularMapColorPower'))==(1.,2.5)
 assert (scalar(a,'g_SpecularPower'),scalar(b,'g_SpecularPower'))==(1.,8.5)
 assert sum(path(d[k])==path(p[k]) for k in p)==300
 print('V430 STATIC MTD PASS: RX34 exact DSR SHA; PTDE 2.5/8.5 vs DSR 1.0/1.0; 300 unchanged SPX; runtime uniform/pixel OPEN')
if __name__=='__main__':
 if len(sys.argv)!=2:raise SystemExit('usage: verify_v430_mtd_smoke.py DSRRL_FINAL_EVIDENCE_PART02.zip')
 check(sys.argv[1])
