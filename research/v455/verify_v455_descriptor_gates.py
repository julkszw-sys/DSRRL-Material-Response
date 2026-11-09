#!/usr/bin/env python3
"""V455 original EXE exact gate verification, no runtime hooks or patching."""
import sys,struct,hashlib,json
from pathlib import Path
SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
W={
0x32d448:"488bf1",0x32d45a:"f681a502000020",0x32d463:"f681a802000002",0x32d46c:"f681aa02000001",
0x32d483:"488b9fe8010000",0x32d495:"ff5010",0x32d498:"85c0",0x32d49a:"7525",
0x32d4a5:"83f817",0x32d4aa:"b900fabf00",0x32d4af:"0fa3c1",0x32d4b2:"722f",
0x32d4ba:"e89196ffff",0x32d4c1:"4438beb0040000",0x32d4d3:"83f802",0x32d4de:"e80d9affff",
0x32d4e3:"c686b004000001",0x32d50e:"488b96e0040000",0x32d518:"e83396ffff",
0x326b74:"488bb170030000",0x326b7b:"483bf2",0x326b7e:"750d",
0x326b80:"80b9b104000000",0x326b87:"0f8439030000",0x326b8d:"80b98403000000",
0x326b94:"0f852c030000",0x326b9f:"807a5a00",0x326ba3:"0f841d030000",
0x326ba9:"48899170030000",0x326be6:"48837b6000",0x326bfb:"0fbe570a",
0x326c02:"e819f7ffff",0x326c0a:"e811b50b00",0x326c17:"0fbe8a93000000",
0x326c22:"838a8400000020",0x326c29:"888293000000",
0x326ef8:"4883b97003000000",0x326f0c:"80b98403000000",
0x326f24:"ff5010",0x326f27:"83f802",0x326f73:"498bce",0x326f76:"e8351afdff",
0x326f83:"0fbe8a93000000",0x326f8e:"838a8400000020",0x326f95:"888293000000"}
CALL={0x32d4ba:0x326b50,0x32d4de:0x326ef0,0x32d518:0x326b50,0x326c02:0x326320,0x326c0a:0x3e2120,0x326f76:0x2f89b0}
def main(path):
 b=Path(path).read_bytes();assert hashlib.sha256(b).hexdigest()==SHA,"EXE SHA mismatch"
 pe=struct.unpack_from("<I",b,0x3c)[0];assert b[pe:pe+4]==b"PE\\0\\0".replace(b"\\0",bytes((0,)))
 n=struct.unpack_from("<H",b,pe+6)[0];opt=struct.unpack_from("<H",b,pe+20)[0];sections=[]
 for i in range(n):
  x=pe+24+opt+i*40;vs,rva,sz,off=struct.unpack_from("<IIII",b,x+8);sections.append((rva,sz,off))
 def data(r,l):
  for a,n,o in sections:
   if a<=r and r+l<=a+n:return b[o+r-a:o+r-a+l]
  raise AssertionError(hex(r))
 for r,h in W.items():assert data(r,len(bytes.fromhex(h)))==bytes.fromhex(h),hex(r)
 for r,t in CALL.items():
  op=data(r,5);assert op[0]==0xe8 and r+5+struct.unpack("<i",op[1:])[0]==t,hex(r)
 print(json.dumps({"status":"STATIC_PASS","opcode_witnesses":len(W),"direct_call_edges":len(CALL),"runtime_actor":"OPEN","PTDE_pixel":"OPEN"},indent=2))
if __name__=="__main__":main(sys.argv[1])
