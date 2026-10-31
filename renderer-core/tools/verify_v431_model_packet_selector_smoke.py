#!/usr/bin/env python3
"""V431 exact native selector model/packet ABI smoke. Retail owner EXE required."""
from pathlib import Path
import hashlib,struct,sys
BASE=0x140000000
SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
def main(f):
 b=Path(f).read_bytes();assert hashlib.sha256(b).hexdigest()==SHA
 p=struct.unpack_from('<I',b,0x3c)[0];assert b[p:p+4]==b'PE\0\0'
 n=struct.unpack_from('<H',b,p+6)[0];q=p+24+struct.unpack_from('<H',b,p+20)[0];sections=[]
 for i in range(n):
  x=q+i*40;vs,rva,size,raw=struct.unpack_from('<IIII',b,x+8);sections.append((rva,size,raw))
 def rd(rva,size):
  for r,s,p in sections:
   if r<=rva and rva+size<=r+s:return b[p+rva-r:p+rva-r+size]
  raise AssertionError(hex(rva))
 def u64(r):return struct.unpack('<Q',rd(r,8))[0]
 def check(r,h):assert rd(r,len(h)//2)==bytes.fromhex(h),hex(r)
 def call(r,t):
  ins=rd(r,5);assert ins[0]==0xe8 and r+5+struct.unpack_from('<i',ins,1)[0]==t,hex(r)
 def rtti(v):
  col=u64(v-8)-BASE;sig,off,cd,td,base,selfrva=struct.unpack('<6I',rd(col,24))
  assert sig==1 and col==selfrva
  return rd(td+16,140).split(b'\0')[0].decode()
 for v,t in [(0x12eca10,'FrpgModelDrawEntity'),(0x12e5c08,'FrpgDrawParam'),(0x136c0d0,'FrpgFxClusterAppearance_Model'),(0x136c208,'FrpgFxParticleAppearance_Model')]:
  assert t in rtti(v),t
 assert u64(0x12eca60)==BASE+0x220000
 for v in [0x12eb130,0x12eb760]:assert u64(v+0x18)==BASE+0x20dda0
 for r,h in {
  0x21f934:'488d05d5d00c01',0x21f93e:'488b4320',0x21f942:'48894740',
  0x21f9be:'488d8f08010000',0x220014:'8bb29c000000',0x220023:'857118',
  0x220047:'0fbae60c',0x220054:'f7c6000000c0',0x220063:'40f6c611',
  0x220070:'0fbae612',0x22007d:'f7c600006000',0x2201a6:'488975d0',
  0x22024c:'488d8f10010000',0x220265:'6689551c',0x220273:'f30f108714010000',
  0x22028b:'f30f114520',0x2202a5:'66894d1e',0x220812:'f7869c00000000000001',
  0x220831:'488b4f40',0x220835:'488d55d0',0x220cd3:'488b4f40',
  0x220ce6:'488d55d0',0x220ced:'ff5018'}.items():check(r,h)
 for r,t in [(0x21f9c5,0x1d7490),(0x22008c,0x2200d0),(0x220839,0x20fee0),(0x223b0f,0x21f900),(0x4fd3de,0x21f900),(0x4fe7fe,0x21f900)]:
  call(r,t)
 print('V431 ORIGINAL DSR STATIC SELECTOR ABI PASS. Live RX34 packet/actor and PTDE pixel OPEN.')
if __name__=='__main__':
 if len(sys.argv)!=2:raise SystemExit('usage: verify_v431_model_packet_selector_smoke.py ORIGINAL_DSR.exe')
 main(sys.argv[1])
