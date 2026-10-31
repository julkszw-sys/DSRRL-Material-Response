#!/usr/bin/env python3
"""V427 static original PE GPU ABI smoke check; no runtime or binary modifications."""
import hashlib,struct,sys
from pathlib import Path
SHAS={'PTDE':'88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d','DSR':'a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b'}
class PE:
 def __init__(self,path,name):
  d=Path(path).read_bytes();assert hashlib.sha256(d).hexdigest()==SHAS[name]
  self.d=d;e=struct.unpack_from('<I',d,0x3c)[0];assert d[e:e+4]==b'PE\0\0'
  o=e+24;m=struct.unpack_from('<H',d,o)[0]
  self.base=struct.unpack_from('<Q' if m==0x20b else '<I',d,o+(24 if m==0x20b else 28))[0]
  p=o+struct.unpack_from('<H',d,e+20)[0];self.s=[]
  for i in range(struct.unpack_from('<H',d,e+6)[0]):
   k=p+40*i;v,r,rs,fo=struct.unpack_from('<IIII',d,k+8);self.s.append((self.base+r,rs,fo))
 def read(self,va,n):
  for base,size,off in self.s:
   if base<=va and va+n<=base+size:return self.d[off+va-base:off+va-base+n]
  raise AssertionError(hex(va))
 def check(self,va,h):
  assert self.read(va,len(h)//2)==bytes.fromhex(h),hex(va)
 def call(self,va,dest):
  x=self.read(va,5);assert x[0]==0xE8 and va+5+struct.unpack_from('<i',x,1)[0]==dest,hex(va)
def main(ptde,dsr):
 p=PE(ptde,'PTDE');d=PE(dsr,'DSR')
 for va,h in [(0xf94261,'8b7710'),(0xf94266,'81c650100000'),(0xf9426d,'b856000000'),(0xf94285,'8d8ea0050000'),(0x4318b8,'81c6d0110000'),(0x4317ec,'8b81b4010000'),(0x4316dc,'8b8178010000')]:p.check(va,h)
 for va,h in [(0x1400bad93,'4c8d4c2430'),(0x1400bad98,'41b801000000'),(0x1400bada3,'ff5040'),(0x1401eac72,'41b801000000'),(0x1401eac80,'ff5050'),(0x1401eb4b0,'ba0b000000'),(0x1401eb4bd,'ba0c000000'),(0x1401eb6af,'ba0d000000'),(0x1401eb763,'ba0e000000')]:d.check(va,h)
 for a,b in [(0xf94272,0x422d40),(0x4318a2,0x431650),(0x4318bf,0x431760),(0x424b34,0xf94260)]:p.call(a,b)
 for a,b in [(0x1401eb4b8,0x1401eac30),(0x1401eb4d9,0x1400badc0),(0x1401eb6b7,0x1401eac30),(0x1401eb6c9,0x1400badc0)]:d.call(a,b)
 assert 0x1050+0x40+0x56*16==0x15F0
 assert 0x11D0-0x1050==0x180
 print('V427 static ABI smoke PASS: D3D9 PS constants & DSR PS resources/samplers; pointer alias and shader/pixel OPEN')
if __name__=='__main__':
 if len(sys.argv)!=3:raise SystemExit('usage: verify_v427_gpu_abi_smoke.py PTDE_DATA.exe DarkSoulsRemastered.exe')
 main(*sys.argv[1:])
