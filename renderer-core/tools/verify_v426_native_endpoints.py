#!/usr/bin/env python3
"""V426 static original PTDE vs DSR type5 four-endpoint smoke check."""
import hashlib,struct,sys
from pathlib import Path
def pe(path,sha):
 b=Path(path).read_bytes()
 assert hashlib.sha256(b).hexdigest()==sha
 o=struct.unpack_from('<I',b,0x3c)[0];assert b[o:o+4]==b'PE\0\0'
 t=o+24;x64=struct.unpack_from('<H',b,t)[0]==0x20b
 base=struct.unpack_from('<Q' if x64 else '<I',b,t+24 if x64 else t+28)[0]
 s=t+struct.unpack_from('<H',b,o+20)[0]
 sec=[]
 for i in range(struct.unpack_from('<H',b,o+6)[0]):
  j=s+40*i
  _,r,size,raw=struct.unpack_from('<IIII',b,j+8)
  sec.append((base+r,size,raw))
 def read(va,n):
  for a,size,raw in sec:
   if a<=va and va+n<=a+size:return b[raw+va-a:raw+va-a+n]
  raise ValueError(hex(va))
 return read
def check(read,origin,target):
 b=read(origin,5);assert b[0]==0xe8
 assert origin+5+struct.unpack('<i',b[1:])[0]==target,hex(origin)
def main(p,d):
 P=pe(p,'88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d')
 D=pe(d,'a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b')
 for origin in (0xff1930,0xff1953,0xff1978,0xff199d):check(P,origin,0x4bbf70)
 for origin in (0x140563d4e,0x140563d65,0x140563d7d,0x140563d95):
  check(D,origin,0x140564510)
 for origin in (0x140563bcc,0x140563be3):check(D,origin,0x140564510)
 check(P,0xf95876,0xff1600)
 check(D,0x1401c69fe,0x140563b80)
 assert P(0xff1644,4)==bytes.fromhex('0fbf5046')
 assert P(0xff1654,4)==bytes.fromhex('0fbf5040')
 assert P(0xff16cf,4)==bytes.fromhex('0fbf5048')
 assert D(0x140563bbb,4)==bytes.fromhex('488d5740')
 assert D(0x140563bd1,4)==bytes.fromhex('488d5748')
 assert D(0x140564568,7)==bytes.fromhex('0f5405e178d400')
 print('V426 native endpoint ABI PASS; shader and pixel OPEN')
if __name__=='__main__':
 if len(sys.argv)!=3:raise SystemExit('usage: verify_v426_native_endpoints.py PTDE_DATA.exe DSR.exe')
 main(*sys.argv[1:])
