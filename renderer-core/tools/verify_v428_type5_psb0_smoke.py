#!/usr/bin/env python3
"""Original DSR x64 / PTDE x86 V428 static binary smoke; no EXE changes.
Full 58-preimage source verifier and JSON are in the V428 handoff archive.
"""
import hashlib,struct,sys
from pathlib import Path
DSR='a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b'
PTDE='88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d'
def pe(file,sha):
 d=Path(file).read_bytes();assert hashlib.sha256(d).hexdigest()==sha
 h=struct.unpack_from('<I',d,0x3c)[0];assert d[h:h+4]==b'PE\0\0'
 o=h+24;m=struct.unpack_from('<H',d,o)[0]
 base=struct.unpack_from('<Q' if m==0x20b else '<I',d,o+(24 if m==0x20b else 28))[0]
 off=o+struct.unpack_from('<H',d,h+20)[0];sections=[]
 for i in range(struct.unpack_from('<H',d,h+6)[0]):
  j=off+40*i;v,r,rs,p=struct.unpack_from('<IIII',d,j+8);sections.append((base+r,rs,p))
 def read(va,n):
  for a,size,p in sections:
   if a<=va and va+n<=a+size:return d[p+va-a:p+va-a+n]
  raise AssertionError(hex(va))
 return read
def exact(f,addr,h):assert f(addr,len(h)//2)==bytes.fromhex(h),hex(addr)
def edge(f,addr,target):
 b=f(addr,5);assert b[0]==0xe8 and addr+5+struct.unpack_from('<i',b,1)[0]==target
if __name__=='__main__':
 if len(sys.argv)!=3:raise SystemExit('usage: verify_v428_psb0_smoke.py DSR.exe PTDE_DATA.exe')
 d,p=pe(sys.argv[1],DSR),pe(sys.argv[2],PTDE)
 for a,h in {0x1401e0902:'458b8e30220000',0x1401e0909:'4d8d8630120000',
 0x1401e0931:'e8da00af00',0x1401e0959:'ff9080000000',
 0x140cd0a50:'41b904000000',0x140cd0a5f:'ff5070',
 0x140cd0a70:'0f2803',0x140cd0a77:'0f2900',0x140cd0a90:'ff5078',
 0x140064a91:'488d9330120000',0x14006496d:'ff9080000000',
 0x1401c6a1d:'0f298740120000'}.items():exact(d,a,h)
 for a,h in {0xf94261:'8b7710',0xf94266:'81c650100000',0x4318b8:'81c6d0110000'}.items():exact(p,a,h)
 for a,t in {0x1401c69fe:0x140563b80,0x1401e0931:0x140cd0a10,0x140064946:0x140cd0a10,0x140064a9b:0x140064900}.items():edge(d,a,t)
 print('V428 static binary smoke PASS; count/receiver/shader/pixel OPEN')
