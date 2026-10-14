#!/usr/bin/env python3
"""V279 original final R19: prove preexisting SpecRGB x COLOR0 material route.
Read only owner-derived shader DXBC, no redistribution of game bytes."""
import hashlib,json,struct,sys
from pathlib import Path
SHA={33:"1d9fed8a55689e09bacf55a66a88f2c3474f7c3f9a5604fe5151b0506b06caab",34:"8f86b1aec73b3b9ec77c10ce6de2308d0b0d4533a1430373ea92b297ee387f97",35:"b5f94ccec4e01e7d98c4329c2b28aecfd65e103729be969a53f486b182c55b70"}
PATH={33:(183,1089,1100,1108,1137,1238,1288,1296,1305),34:(186,998,1009,1017,1046,1147,1197,1205,1214),35:(176,658,669,677,706,807,857,865,874)}
REG={33:(6,7,8,10,11),34:(7,6,7,9,10),35:(6,5,6,8,9)}
def decode(data):
 assert data[:4]==b"DXBC" and struct.unpack_from("<I",data,24)[0]==len(data)
 n=struct.unpack_from("<I",data,28)[0]
 off=next(struct.unpack_from("<I",data,32+i*4)[0] for i in range(n) if data[struct.unpack_from("<I",data,32+i*4)[0]:][:4]==b"SHEX")
 z=struct.unpack_from("<I",data,off+4)[0]//4
 w=struct.unpack_from(f"<{z}I",data,off+8)
 records={};i=2
 while i<len(w):
  step=(w[i]>>24)&127
  assert step and i+step<=len(w)
  records[i]=tuple(w[i:i+step]);i+=step
 return records,off
def check(rx,data):
 assert hashlib.sha256(data).hexdigest()==SHA[rx]
 ins,_=decode(data)
 color,spec,c101,iff,factor,vertex,add,mad,mul=PATH[rx]
 cr,sr,mr,fr,tr=REG[rx]
 assert all(p in ins for p in PATH[rx])
 assert ins[color]==(0x07000038,0x001000f2,1,0x00100e46,1,0x00101e46,cr)
 assert ins[c101]==(0x08000038,0x00100072,sr,0x00100246,sr,0x00208246,12,0)
 assert ins[iff]==(0x0400001f,0x0020800a,0,85)
 assert ins[vertex]==(0x07000038,0x00100072,mr,0x00100246,1,0x00100246,mr)
 assert ins[add]==(0x08000000,0x00100072,tr,0x80100ff6,0x41,2,0x00100246,mr)
 assert ins[mad]==(0x09000032,0x00100072,tr,0x00100556,fr,0x00100246,tr,0x00100ff6,2)
 assert ins[mul]==(0x07002038,0x00100072,tr,0x00100006,sr,0x00100246,tr)
 assert len(ins[factor])==9 and ins[factor][2]==fr and ins[factor][-2:]==(0x00100596,sr)
 assert len(ins[spec])==11 and ins[spec][-3]==10
 # No RGB overwrite of the first COLOR0-tinted r1 prior to the
 # independently authored IF0 material color merge.
 for at,w in ins.items():
  if color<at<vertex and len(w)>=3:
   d=w[1]
   assert not ((d>>12)&255==0 and ((d>>20)&3)==1 and w[2]==1
       and ((d>>2)&3)==0 and ((d>>4)&7)!=0),(rx,at)
 return {"rx":rx,"sha256":SHA[rx],"t10_spec_word":spec,"color0_mult_word":color,
  "material_color_mult_word":vertex,"t10_derived_factor":factor,
  "tinted_spec_mad_word":mad,"spec_followup_mul_word":mul,
  "existing_color0_to_t10_spec":"EXACT_STATIC_OPERATOR_PASS",
  "v278_extra_color_mul":"UNSAFE_DOUBLE_TINT_RISK","PTDE_pixel":"OPEN"}
def main():
 if len(sys.argv)!=6:raise SystemExit("usage: verify.py final33.dxbc final34.dxbc final35.dxbc report.json --falsify")
 records=[]
 for rx,path in zip(SHA,sys.argv[1:4]):
  b=Path(path).read_bytes()
  records.append(check(rx,b))
  if sys.argv[5]=="--falsify":
   _,off=decode(b)
   for at in PATH[rx]:
    mutant=bytearray(b)
    mutant[off+8+at*4]^=1
    try:check(rx,bytes(mutant))
    except AssertionError:pass
    else:raise ValueError("unverified mutation passed")
 Path(sys.argv[4]).write_text(json.dumps({"schema":"dsrrl.v279.actual_final_r19_existing_local_color_tint.v1","records":records},indent=2)+"\n")
 print("V279_FINAL_R19_COLOR0_SPEC_COMPOSITION_THREE_RX_PASS")
 if sys.argv[5]=="--falsify":print("V279_27_MUTATION_NEGATIVES_PASS")
if __name__=="__main__":main()
