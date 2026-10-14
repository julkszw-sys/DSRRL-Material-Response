#!/usr/bin/env python3
"""V276: inspect TRUE FINAL output of compiled production R19 materializer.

Never substitute the stock donor as the R19 result in a real finding.
Original DSR DXBC and final owner-local outputs stay private.
"""
import argparse, hashlib, json, struct
from pathlib import Path
RX = {
 33:("7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85",1345,1478,2112,2343,2498,2552,2895),
 34:("857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db",1254,1387,2031,2262,2417,2471,2814),
 35:("9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8",914,1047,1684,1915,2070,2124,2467),
}
def parse(binary):
 if binary[:4]!=b"DXBC" or len(binary)<36 or struct.unpack_from("<I",binary,24)[0]!=len(binary):
  raise ValueError("invalid DXBC")
 count=struct.unpack_from("<I",binary,28)[0]
 if not 1<=count<64:raise ValueError("invalid chunk count")
 words=None
 for k in range(count):
  off=struct.unpack_from("<I",binary,32+4*k)[0]
  if off+8>len(binary):raise ValueError("chunk overflow")
  length=struct.unpack_from("<I",binary,off+4)[0]
  if length>len(binary)-off-8:raise ValueError("chunk length overflow")
  if binary[off:off+4]==b"SHEX":
   if words is not None or length%4:raise ValueError("SHEX duplicate/misaligned")
   words=list(struct.unpack_from(f"<{length//4}I",binary,off+8))
 if words is None or len(words)<2000 or words[1]!=len(words):raise ValueError("SHEX invalid")
 ins={};p=2
 while p<len(words):
  n=(words[p]>>24)&127
  if n==0 or p+n>len(words):raise ValueError("malformed DXBC instruction")
  ins[p]=n;p+=n
 return words,ins
def hits(a,a_ins,b,b_ins,at,n):
 if at not in a_ins or at+n>len(a):raise ValueError("noninstruction original anchor")
 signature=a[at:at+n]
 return [k for k in b_ins if k+n<=len(b) and b[k:k+n]==signature]
def inspect(rx,stock,final):
 sha,exp,early,mad1,mad2,prefog,fog,glow=RX[rx]
 if hashlib.sha256(stock).hexdigest()!=sha:raise ValueError("original RX SHA mismatch")
 a,ai=parse(stock);b,bi=parse(final)
 checks={}
 for name,at in {"gamma_EXP":exp,"early_use":early,"late_MAD1":mad1,
                  "late_MAD2":mad2,"preFog_color":prefog}.items():
  found=hits(a,ai,b,bi,at,ai[at])
  checks[name]={"original_word":at,"opcode":a[at]&2047,
                "final_word":found[0] if len(found)==1 else None,
                "match_count":len(found)}
 f=hits(a,ai,b,bi,fog,212)
 g=hits(a,ai,b,bi,glow,5)
 ga=hits(a,ai,b,bi,glow+10,5)
 if not(len(f)==len(g)==len(ga)==1 and f[0]+212<=g[0]<ga[0]):
  raise ValueError("original DSR Fog/LS/Glow not retained in true final R19")
 points=[checks[x]["final_word"] for x in ("gamma_EXP","early_use","late_MAD1","late_MAD2")]
 survival=all(x is not None for x in points) and points==sorted(points)
 writes=[]
 if survival:
  first,second=points[2:4]
  for k,n in bi.items():
   if not first<k<second or n<3:continue
   tok=b[k+1]
   if (tok>>12)&0xff==0 and b[k+2]==3 and ((tok>>4)&15)&7:
    writes.append({"word":k,"opcode":b[k]&2047,"mask":(tok>>4)&15})
 return {"rx":rx,"final_sha256":hashlib.sha256(final).hexdigest(),
         "final_bytes":len(final),"shex_words":len(b),
         "stock_Fog_LS_word":f[0],"stock_Glow_rgb_word":g[0],
         "stock_Glow_alpha_word":ga[0],
         "second_material_chain_exact_subcuts":checks,
         "late_pair_byte_identical_and_ordered":survival,
         "possible_temp_r3_overwrites_between_mads":writes,
         "material_branch_cb85_and_debug_cb102":"UNMEASURED",
         "per_component_preFog_pixel_liveout":"OPEN_UNTIL_DEF_USE",
         "PTDE_visible_pixel":"OPEN"}
if __name__=="__main__":
 p=argparse.ArgumentParser()
 p.add_argument("--stock",nargs=3,required=True,type=Path)
 p.add_argument("--final",nargs=3,required=True,type=Path)
 p.add_argument("--out",required=True,type=Path)
 a=p.parse_args()
 result=[inspect(rx,a.stock[i].read_bytes(),a.final[i].read_bytes())
         for i,rx in enumerate((33,34,35))]
 a.out.write_text(json.dumps({"schema":"dsrrl.v276.true_final_r19_second_lane.v1",
                                "receivers":result},indent=2)+"\n")
 print("V276_TRUE_FINAL_R19_GAMMA_AND_MAD_SURVIVAL_ORACLE_PASS")
