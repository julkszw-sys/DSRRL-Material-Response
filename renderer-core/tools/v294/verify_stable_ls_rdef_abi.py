#!/usr/bin/env python3
"""V294 owner-SHA-pinned DSR Stable LightScattering constant-buffer ABI.
Reads external retail DXBC only, never carries shader bytes in repository.
"""
import argparse,hashlib,json,struct
from pathlib import Path
PIN={33:"35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24",
34:"d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837",
35:"7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701"}
FIELDS={12:"gFC_FogCol",13:"gFC_LsBeta1PlusBeta2",
14:"gFC_LsTerrainReflectance",15:"gFC_LsOneOverBeta1PlusBeta2",
16:"gFC_LsHGg",17:"gFC_LsBetaDash1",18:"gFC_LsBetaDash2",
19:"gFC_LsSunColor",20:"gFC_LsLightDir"}
def u32(b,p):
 if p<0 or p+4>len(b):raise ValueError("u32 outside source")
 return struct.unpack_from("<I",b,p)[0]
def name(b,base,offset):
 p=base+offset
 if p<base or p>=len(b):raise ValueError("string offset")
 e=b.find(bytes([0]),p)
 if e<0 or e-p>128:raise ValueError("string termination")
 return b[p:e].decode("ascii")
def rdef(b):
 if len(b)<48 or b[:4]!=b"DXBC" or u32(b,24)!=len(b):raise ValueError("DXBC envelope")
 n=u32(b,28)
 if not 2<=n<=32 or 32+4*n>len(b):raise ValueError("DXBC chunk count")
 found=[]
 for i in range(n):
  off=u32(b,32+4*i)
  if off+8>len(b):raise ValueError("chunk offset")
  sz=u32(b,off+4)
  if sz>len(b)-off-8:raise ValueError("chunk overrun")
  if b[off:off+4]==b"RDEF":found.append((off+8,sz))
 if len(found)!=1:raise ValueError("RDEF uniqueness")
 base,size=found[0]; n=u32(b,base);table=u32(b,base+4)
 if not 1<=n<=16 or table+24*n>size:raise ValueError("RDEF CB table")
 for i in range(n):
  cb=base+table+i*24
  if name(b,base,u32(b,cb))!="$Globals":continue
  count=u32(b,cb+4);vp=u32(b,cb+8);cbsz=u32(b,cb+12)
  if not 9<=count<=512 or vp+40*count>size:raise ValueError("CB variables")
  fields={}
  for j in range(count):
   v=base+vp+40*j
   nm=name(b,base,u32(b,v));start=u32(b,v+4);length=u32(b,v+8)
   if not length or start+length>cbsz:raise ValueError("CB variable range")
   if start%16==0 and length==16:
    key=start//16
    if key in fields:raise ValueError("ambiguous CB register")
    fields[key]=nm
  return fields,cbsz
 raise ValueError("Globals unavailable")
def audit(b,rx):
 if hashlib.sha256(b).hexdigest()!=PIN[rx]:raise ValueError("stock RX SHA identity")
 fields,size=rdef(b)
 if any(fields.get(k)!=v for k,v in FIELDS.items()):raise ValueError("Fog or LS CB semantic mismatch")
 return {"rx":rx,"sha256":PIN[rx],"globals_size":size,"fog_cb":12,
         "ls_beta_prefog_cb":13,"scattering_cb":[14,15,16,17,18,19,20]}
def main():
 p=argparse.ArgumentParser();p.add_argument("rx33");p.add_argument("rx34");p.add_argument("rx35");p.add_argument("--report")
 a=p.parse_args()
 results=[audit(Path(path).read_bytes(),rx) for rx,path in zip(PIN,(a.rx33,a.rx34,a.rx35))]
 if a.report:Path(a.report).write_text(json.dumps(results,indent=2)+"\n")
 print("V294_STOCK_STABLE_RDEF_FOG_LS_ABI_3_RX_PASS")
if __name__=="__main__":main()
