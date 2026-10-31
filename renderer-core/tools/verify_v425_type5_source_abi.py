#!/usr/bin/env python3
"""V425 minimum reproducibility gate for original PTDE/DSR type5 CPU operators."""
import hashlib,struct,sys
from pathlib import Path
SHAS=('88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d','a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b')
class PE:
 def __init__(self,f,expected):
  self.d=Path(f).read_bytes();assert hashlib.sha256(self.d).hexdigest()==expected
  d=self.d;o=struct.unpack_from('<I',d,0x3c)[0];assert d[o:o+4]==b'PE\0\0'
  opt=o+24;self.x64=struct.unpack_from('<H',d,opt)[0]==0x20b
  self.base=struct.unpack_from('<Q' if self.x64 else '<I',d,opt+(24 if self.x64 else 28))[0]
  sc=opt+struct.unpack_from('<H',d,o+20)[0];self.sections=[]
  for i in range(struct.unpack_from('<H',d,o+6)[0]):
   x=sc+i*40;r,rs,pos=struct.unpack_from('<III',d,x+12)
   self.sections.append((self.base+r,rs,pos))
 def get(self,va,n):
  for base,size,pos in self.sections:
   if base<=va and va+n<=base+size:return self.d[pos+va-base:pos+va-base+n]
  raise ValueError(hex(va))
 def call(self,va):
  b=self.get(va,5);assert b[0]==0xe8
  return va+5+struct.unpack_from('<i',b,1)[0]
 def f32(self,va):return struct.unpack('<f',self.get(va,4))[0]
 def f64(self,va):return struct.unpack('<d',self.get(va,8))[0]
def verify(ptde,dsr):
 p,d=PE(ptde,SHAS[0]),PE(dsr,SHAS[1])
 assert p.base==0x400000 and d.base==0x140000000
 for pe,va,h in [(p,0xff1a26,'8b4010'),(p,0xff1a2c,'6683780804'),(p,0xff1ac2,'0fbf570a'),(p,0xff1aca,'f20f100d50962401'),(d,0x140563590,'4889742410'),(d,0x14056451a,'0fbf4206'),(d,0x140564568,'0f5405e178d400'),(d,0x140564573,'e84e14b800'),(d,0x1401c697b,'41b805000000')]:
  assert pe.get(va,len(h)//2)==bytes.fromhex(h),hex(va)
 for pe,origin,target in [(p,0xf957f4,0xff1a20),(p,0xff1a6d,0x4bbc60),(p,0xff1a8b,0x4bbc60),(p,0xff1aaa,0x4bbc60),(d,0x140565142,0x140563590),(d,0x1405635ba,0x140564870),(d,0x1405635f8,0x140564510),(d,0x1401c69c3,0x140563460),(d,0x1401c69fe,0x140563b80),(d,0x140564573,0x1410e59c6)]:
  assert pe.call(origin)==target,hex(origin)
 assert 2.199999<d.f32(0x1412aa960)<2.200001
 assert d.f32(0x141615c80)==255.
 assert abs(d.f32(0x1412dc48c)-.01)<1e-8
 assert p.f64(0x1249600)==255.
 assert d.get(0x1410e59c6,2)==b'\xff\x25'
 print('V425 SHA/PREIMAGE/DIRECT-CALL/GAMMA ABI PASS; GPU and pixel OPEN')
if __name__=='__main__':
 if len(sys.argv)!=3:raise SystemExit('Usage: verify_v425_source_abi.py PTDE_DATA.exe DSR.exe')
 verify(*sys.argv[1:])
