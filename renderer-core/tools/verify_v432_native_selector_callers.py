#!/usr/bin/env python3
"""V432 static native selector identity: original DSR EXE, not runtime/actor proof."""
import hashlib,struct,sys
from pathlib import Path
SHA='a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b'
def main(path):
 d=Path(path).read_bytes();assert hashlib.sha256(d).hexdigest()==SHA
 o=struct.unpack_from('<I',d,0x3c)[0]; assert d[o:o+4]==b'PE\0\0'
 assert struct.unpack_from('<H',d,o+24)[0]==0x20b
 assert struct.unpack_from('<Q',d,o+48)[0]==0x140000000
 s=o+24+struct.unpack_from('<H',d,o+20)[0];sec=[]
 for i in range(struct.unpack_from('<H',d,o+6)[0]):
  k=s+40*i;v,r,sz,raw=struct.unpack_from('<IIII',d,k+8);sec.append((r,sz,raw))
 def get(a,n):
  for r,size,raw in sec:
   if r<=a and a+n<=r+size:return d[raw+a-r:raw+a-r+n]
  raise AssertionError(hex(a))
 op={0x20df00:'488db788000000',0x20e011:'488bce',0x20e014:'e807da0100',
     0x20ead5:'4d8dbd88000000',0x20eb4c:'498bcf',0x20eb7a:'e8a1ce0100',
     0x20fa2a:'488bf1',0x20faf5:'4c8da688000000',0x20fb6b:'498bcc',0x20fb99:'e882be0100',
     0x20e9ff:'488b2a',0x20fa23:'488b2a',0x20eb35:'410fb7464e',
     0x20eb43:'664139464c',0x20fb58:'410fb7474e',0x20fb62:'664139474c',
     0x22119b:'e830d8feff',0x221412:'e8b9d5feff',
     0x2219f6:'e835d7feff',0x222033:'e8b8d9feff',
     0x221898:'488d8f20010000'}
 for a,h in op.items():assert get(a,len(h)//2)==bytes.fromhex(h),hex(a)
 edges={0x20e014:0x22ba20,0x20eb7a:0x22ba20,0x20fb99:0x22ba20,
        0x22119b:0x20e9d0,0x221412:0x20e9d0,0x2219f6:0x20f130,
        0x222033:0x20f9f0}
 for src,dst in edges.items():
  b=get(src,5); assert b[0]==0xe8 and src+5+struct.unpack_from('<i',b,1)[0]==dst,hex(src)
 print('V432 PASS',len(op),'exact preimages',len(edges),'native edges; actor, packet join and pixel OPEN')
if __name__=='__main__':
 if len(sys.argv)!=2:raise SystemExit('usage: verify_v432_native_selector_callers.py ORIGINAL_DSR.exe')
 main(sys.argv[1])
