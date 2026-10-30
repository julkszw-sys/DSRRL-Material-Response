#!/usr/bin/env python3
"""V423 original DSR/PTDE structural bank-table and binding ABI check.
Requires original owner-provided exes; does not ship or modify them.
For exhaustive 23/4 preimages and 30 edges see V423 full handoff.
"""
import hashlib,struct,sys
from pathlib import Path
class PE:
 def __init__(self,f):
  d=Path(f).read_bytes();self.d=d
  assert d[:2]==b'MZ'
  o=struct.unpack_from('<I',d,0x3c)[0];assert d[o:o+4]==b'PE\0\0'
  n=struct.unpack_from('<H',d,o+6)[0];h=struct.unpack_from('<H',d,o+20)[0];t=o+24
  self.sec=[]
  for i in range(n):
   x=t+h+i*40
   z,r,s,p=struct.unpack_from('<IIII',d,x+8)
   self.sec.append((r,s,p))
 def get(self,r,n):
  for a,s,p in self.sec:
   if a<=r and r+n<=a+s:return self.d[p+r-a:p+r-a+n]
  raise AssertionError(hex(r))
 def sha(self):return hashlib.sha256(self.d).hexdigest()
def run(dsr,ptde):
 d,p=PE(dsr),PE(ptde)
 assert d.sha()=='a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b'
 assert p.sha()=='88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d'
 checks=[
  (p,0x931590,'33c083f90c7d1683fa0c77118b44240469c9d80000008b4c01148b04d1'),
  (p,0x9315b0,'33c083f90c7d1783fa0c77128b44240469c9d80000008b4c01148b44d104'),
  (d,0x569b40,'83fa0b77214183f80c771b4863c24869c0b00100004963d04803d2'),
  (d,0x1eb479,'663b8b482300000f85b30000000fb7834a230000'),
  (d,0x1eb539,'66898b48230000488d442420410fb7076689834a230000'),
  (d,0x1c0e40,'410f2800660f7f8220170000c3')]
 for pe,r,hexbytes in checks:
  b=bytes.fromhex(hexbytes);assert pe.get(r,len(b))==b,hex(r)
 assert 0x1b0==2*0xd8 and 16==2*8
 print('V423 STATIC ABI PASS; PTDE active caller and GPU/pixel remain OPEN')
if __name__=='__main__':
 if len(sys.argv)!=3:raise SystemExit('usage: v423_source_abi_check.py DSR.exe PTDE_DATA.exe')
 run(*sys.argv[1:])
