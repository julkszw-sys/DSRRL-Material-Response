#!/usr/bin/env python3
"""Optional original-owner DSR binder falsification: retail RX33-35 pre-Fog / RT1.

Invoke locally, never upload extracted game shaders. Outputs non-asset SHA and
test results only. The GitHub CI cannot and does not redistribute retail DXBC.
"""
import argparse,hashlib,json,struct,zipfile,zlib
from pathlib import Path

RX={
33:("7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85",2552,2895),
34:("857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db",2471,2814),
35:("9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8",2124,2467),
}
OWNER="DSR/shader/FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx"

def originals(path):
 with zipfile.ZipFile(path) as z:dcx=z.read(OWNER)
 if dcx[:4]!=b"DCX\0":raise ValueError("wrong DCX")
 bnd=zlib.decompress(dcx[76:])
 if bnd[:4]!=b"BND3":raise ValueError("wrong BND3")
 hashes={v[0]:k for k,v in RX.items()};found={};p=0
 while (p:=bnd.find(b"DXBC",p))>=0:
  if p+32<=len(bnd):
   n=struct.unpack_from("<I",bnd,p+24)[0]
   if 256<=n<=200000 and p+n<=len(bnd):
    data=bnd[p:p+n];digest=hashlib.sha256(data).hexdigest()
    if digest in hashes:
     rx=hashes[digest]
     if rx in found:raise ValueError("duplicate RX")
     found[rx]=data
  p+=4
 if set(found)!=set(RX):raise ValueError("missing pinned retail owner RX")
 return found

def shader_words(d):
 if d[:4]!=b"DXBC" or struct.unpack_from("<I",d,24)[0]!=len(d):raise ValueError("bad DXBC")
 count=struct.unpack_from("<I",d,28)[0]
 if not 1<=count<=64:raise ValueError("bad chunk count")
 chunks=[]
 for i in range(count):
  off=struct.unpack_from("<I",d,32+i*4)[0]
  if off+8>len(d):raise ValueError("bad chunk")
  n=struct.unpack_from("<I",d,off+4)[0]
  if off+8+n>len(d):raise ValueError("bad length")
  if d[off:off+4] in (b"SHEX",b"SHDR"):chunks.append(d[off+8:off+8+n])
 if len(chunks)!=1 or len(chunks[0])%4:raise ValueError("wrong shader chunk")
 b=chunks[0];w=list(struct.unpack(f"<{len(b)//4}I",b))
 if w[1]!=len(w):raise ValueError("wrong words")
 return w

def boundaries(w):
 i=2;out={}
 while i<len(w):
  n=(w[i]>>24)&127
  if not n or i+n>len(w):raise ValueError("bad instruction")
  out[i]=n;i+=n
 return out

def proof(original,candidate,fog,glow):
 s=boundaries(original);d=boundaries(candidate)
 if fog not in s or fog+212 not in s or glow not in s or glow+10 not in s:return False
 if original[glow:glow+3]!=[0x05000036,0x00102072,1]:return False
 if original[glow+10:glow+13]!=[0x05000036,0x00102082,1]:return False
 def hits(start,n):
  target=original[start:start+n]
  return [i for i in d if candidate[i:i+n]==target]
 f=hits(fog,212);r=hits(glow,5);a=hits(glow+10,5)
 if not (len(f)==len(r)==len(a)==1 and f[0]+212<=r[0]<a[0]):return False
 writes=sum(1 for i,n in d.items() if (candidate[i]&2047)==0x36 and n==5 and i+2<len(candidate) and candidate[i+1] in (0x00102072,0x00102082) and candidate[i+2]==1)
 return writes==2

def run(owner):
 report={}
 for rx,d in originals(owner).items():
  sha,f,g=RX[rx];stock=shader_words(d)
  if not proof(stock,stock,f,g):raise ValueError("stock donor unexpectedly rejected")
  for off in (f+7,g+4,g+14):
   changed=stock.copy();changed[off]^=1
   if proof(stock,changed,f,g):raise ValueError(f"RX{rx} accepted mutation {off}")
  report[rx]={"exact_shader_sha256":sha,"fog_word":f,"glow_rgb_word":g,"glow_alpha_word":g+10,
   "fog_212_sha256":hashlib.sha256(struct.pack("<212I",*stock[f:f+212])).hexdigest(),
   "stock_positive":True,"three_mutations_rejected":True}
 return report
if __name__=="__main__":
 p=argparse.ArgumentParser()
 p.add_argument("--part02",required=True,type=Path)
 p.add_argument("--out",type=Path,required=True)
 args=p.parse_args()
 results=run(args.part02)
 args.out.write_text(json.dumps({"version":"V271","runtime":"OPEN","pixel":"OPEN","retail":results},indent=2,sort_keys=True)+"\n")
 print("V271_RETAIL_DSR_3_POSITIVE_9_NEGATIVE_PASS")
