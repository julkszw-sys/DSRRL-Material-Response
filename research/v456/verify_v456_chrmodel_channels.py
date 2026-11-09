#!/usr/bin/env python3
"""V456 original EXE read-only essential opcode/call-edge witness."""
import hashlib,struct,sys
SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
W={0x326c22:"838a8400000020",0x326c29:"888293000000",0x326c4a:"838a8400000040",0x326c51:"888294000000",0x326c72:"818a8400000080000000",0x326c7c:"888295000000",0x326c93:"81888400000000010000",0x326c9d:"66898896000000",0x326f8e:"838a8400000020",0x326f95:"888293000000",0x326fb6:"838a8400000040",0x326fbd:"888294000000",0x326fde:"818a8400000080000000",0x326fe8:"888295000000"}
C={0x326c0a:0x3e2120,0x326c32:0x3e21c0,0x326c5a:0x3e2260,0x326f76:0x2f89b0,0x326f9e:0x2f8a50,0x326fc6:0x2f8af0}
def verify(b):
 assert hashlib.sha256(b).hexdigest()==SHA
 p=struct.unpack_from("<I",b,0x3c)[0];assert b[p:p+4]==bytes((80,69,0,0))
 n=struct.unpack_from("<H",b,p+6)[0];o=struct.unpack_from("<H",b,p+20)[0]
 s=[struct.unpack_from("<IIII",b,p+24+o+40*i+8) for i in range(n)]
 def rd(r,n):
  for _,a,z,f in s:
   if a<=r and r+n<=a+z:return b[f+r-a:f+r-a+n]
  raise AssertionError(hex(r))
 for r,h in W.items():assert rd(r,len(bytes.fromhex(h)))==bytes.fromhex(h),hex(r)
 for r,d in C.items():
  v=rd(r,5);assert v[0]==232 and r+5+struct.unpack("<i",v[1:])[0]==d,hex(r)
 print("V456 STATIC PASS",len(W),"opcodes",len(C),"edges; runtime/pixel OPEN")
if __name__=="__main__":verify(open(sys.argv[1],"rb").read())
