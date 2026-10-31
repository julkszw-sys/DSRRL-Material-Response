#!/usr/bin/env python3
"""V424 original PTDE/DSR static LightBank ABI verifier. No runtime/patch."""
from pathlib import Path
import hashlib,struct,sys
P_SHA="88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d"
D_SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
class PE:
 def __init__(self,f,expected):
  self.d=Path(f).read_bytes()
  assert hashlib.sha256(self.d).hexdigest()==expected,"exact retail SHA mismatch"
  o=struct.unpack_from("<I",self.d,0x3c)[0]
  assert self.d[o:o+4]==b"PE\0\0"
  opt=o+24;self.wide=struct.unpack_from("<H",self.d,opt)[0]==0x20b
  self.base=struct.unpack_from("<Q" if self.wide else "<I",self.d,opt+(24 if self.wide else 28))[0]
  n=struct.unpack_from("<H",self.d,o+6)[0];s=opt+struct.unpack_from("<H",self.d,o+20)[0];self.secs=[]
  for i in range(n):
   j=s+40*i;label=self.d[j:j+8].split(b"\0")[0]
   vsize,rva,rawsize,pos=struct.unpack_from("<IIII",self.d,j+8)
   self.secs.append((label,self.base+rva,rawsize,pos))
 def read(self,va,n):
  for _,start,size,pos in self.secs:
   if start<=va and va+n<=start+size:
    return self.d[pos+va-start:pos+va-start+n]
  raise ValueError(hex(va))
 def calls(self,target):
  out=[]
  for label,va,size,pos in self.secs:
   if label!=b".text":continue
   b=self.d[pos:pos+size]
   for i in range(len(b)-4):
    if b[i]!=0xe8:continue
    dest=va+i+5+struct.unpack_from("<i",b,i+1)[0]
    if dest==target:out.append(va+i)
  return out
def must(p,a,hexbytes):
 b=bytes.fromhex(hexbytes)
 assert p.read(a,len(b))==b,f"instruction preimage drift: {a:#x}"
if __name__=="__main__":
 if len(sys.argv)!=3:raise SystemExit("usage: verify_v424.py PTDE_DATA.exe DarkSoulsRemastered.exe")
 p=PE(sys.argv[1],P_SHA);d=PE(sys.argv[2],D_SHA)
 for a,h in {
  0xf957b2:"83f80b77108b4d0c69c0d80000008b5408148b7a2c85",
  0xf95d1a:"83f80b770d69c0d80000008b4438148b482c894c242485c9",
  0xf94cd2:"83f80b77108b4d0c69c0d80000008b5408148b7a3485ff",
  0xf95243:"83f80b770d69c0d80000008b4c38148b4934894c242485c97518",
  0xff1a20:"558bec83e4f08b401083ec246683780804538b5d0856570f85ff0300",
  0x4bbc60:"558bec83e4f083ec100fbf080fbf5002894c240451db44240889542408",
  0xff1090:"558bec83e4f08b4d088b491083ec2c5685c07d05",
 }.items():must(p,a,h)
 for a,h in {
  0x140569b40:"83fa0b77214183f80c771b4863c24869c0",
  0x140565600:"4883ec28488b41184c8bca4585c079",
  0x1401c86d3:"e868143a004885c0750f8d500b488b",
  0x1401c0e40:"410f2800660f7f8220170000c3",
  0x1401c0e10:"4883ec1833c04489042448894424048944240c0f280424660f7f82401700004883c418"
 }.items():must(d,a,h)
 for target,calls in {
  0xf92c00:[0xf5578a,0xf5cf42,0xf9876c],
  0xff1a20:[0xf94d1b,0xf94dcd,0xf957f4,0xf958a4],
  0x4bbc60:[0xff1a6d,0xff1a8b,0xff1aaa],
  0xff1290:[0xf92d42]
 }.items():assert set(calls).issubset(set(p.calls(target))),hex(target)
 assert not p.calls(0xd31590) and not p.calls(0xd315b0)
 assert set([0x1400bef2b,0x1401c86f7,0x14050030a]).issubset(d.calls(0x140565600))
 assert 11*0xd8+0x14==0x95c and 5*8+4==0x2c and 6*8+4==0x34
 q=struct.unpack("<d",p.read(0x1249740,8))[0]
 assert abs(q-0.01745329238474369)<1e-13
 print("V424 STATIC PASS: PTDE active type5/type6/format4 angular decode; DSR type2 and CPU metadata. Shader/pixel OPEN")
