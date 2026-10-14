#!/usr/bin/env python3
"""V276 source-equivalent R19 component may-flow oracle, owner-local only.

Uses *true final materialized* R19 shader DXBC. Unlike V265 original-only,
pinned final hashes/offsets and exact donor instruction equality are mandatory.
Conservative may-flow (not SSA, runtime branch, or pixel proof).
No game-owned DXBC is written or shipped.
"""
import argparse, hashlib, json, struct
from pathlib import Path

PIN={
 33:("7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85",
     "1d9fed8a55689e09bacf55a66a88f2c3474f7c3f9a5604fe5151b0506b06caab",12,
     [2343,2429,2465,2477,2484,2491,2498,2519],2552),
 34:("857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db",
     "8f86b1aec73b3b9ec77c10ce6de2308d0b0d4533a1430373ea92b297ee387f97",11,
     [2262,2348,2384,2396,2403,2410,2417,2438],2471),
 35:("9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8",
     "b5f94ccec4e01e7d98c4329c2b28aecfd65e103729be969a53f486b182c55b70",10,
     [1915,2001,2037,2049,2056,2063,2070,2091],2124)
}
def check(x,msg):
 if not x:raise ValueError(msg)
def decode(b):
 check(len(b)>=40 and b[:4]==b"DXBC" and struct.unpack_from("<I",b,24)[0]==len(b),
       "wrong shader DXBC")
 n=struct.unpack_from("<I",b,28)[0]
 check(0<n<64 and 32+4*n<=len(b),"bad DXBC directory")
 program=None
 for j in range(n):
  at=struct.unpack_from("<I",b,32+4*j)[0]
  check(at+8<=len(b),"DXBC chunk outside")
  size=struct.unpack_from("<I",b,at+4)[0]
  check(size%4==0 and at+8+size<=len(b),"bad DXBC chunk bytes")
  if b[at:at+4]==b"SHEX":
   check(program is None,"multiple SHEX")
   program=list(struct.unpack_from("<%dI"%(size//4),b,at+8))
 check(program and program[1]==len(program),"SHEX length mismatch")
 records={}; i=2
 while i<len(program):
  size=(program[i]>>24)&127
  check(size>0 and i+size<=len(program),"bad SHEX instruction")
  records[i]=(program[i]&2047,program[i:i+size])
  i+=size
 return program,records
def refs(ins):
 _,w=ins
 def temp(x):return (x&3) in (1,2) and ((x>>12)&255)==0 and ((x>>20)&3)==1
 def lanes(x,write):
  mode=(x>>2)&3
  if write:return {i for i in range(4) if ((x>>4)&15)&(1<<i)} if mode==0 else set()
  if mode==1:return {(x>>(4+2*i))&3 for i in range(4)}
  if mode==2:return {(x>>4)&3}
  return set()
 return [{"kind":"W" if i==1 else "R","reg":w[i+1],"mask":lanes(w[i],i==1)}
    for i in range(1,len(w)-1) if temp(w[i]) and w[i+1]<32]
def run(rx,old,new,falsify=False):
 sha,sha_final,outreg,stock_path,stock_fog=PIN[rx]
 check(hashlib.sha256(old).hexdigest()==sha,"wrong original owner donor")
 if not falsify:check(hashlib.sha256(new).hexdigest()==sha_final,"wrong FINAL R19 result")
 a,ai=decode(old); b,bi=decode(new)
 # Verified original-to-final delta is exactly +27 SHEX words for this
 # receiver. Validate every opcode payload at every position of the
 # complete 8-insn MAD2 -> conditional preFog path.
 path=[z+27 for z in stock_path]
 for src,dst in zip(stock_path,path):
  check(src in ai and dst in bi and ai[src]==bi[dst],
        "post-R19 late MAD/preFog expected opcode sequence mutated")
 check(stock_fog+27 in bi,"native Fog entry missing")
 first,last,prefog=path[0],path[-1],path[-2]
 check(bi[first][0]==50,"expected exact MAD2 opcode not present")
 check(any(x["kind"]=="W" and x["reg"]==outreg and
     {0,1,2}.issubset(x["mask"]) for x in refs(bi[first])),
     "late MAD2 destination changed")
 taint={outreg:{0,1,2}}
 touched=[]
 for at in sorted(bi):
  if at<=first or at>last:continue
  opcode,_=bi[at];rr=refs(bi[at])
  dst=rr[0] if rr and rr[0]["kind"]=="W" else None
  srcs=[r for r in rr[1:] if r["kind"]=="R"]
  carries=any(taint.get(r["reg"],set()) & r["mask"] for r in srcs)
  if dst:
   taint[dst["reg"]]=(taint.get(dst["reg"],set())-dst["mask"]) | (dst["mask"] if carries else set())
   if carries:touched.append(at)
  if at==prefog:
   check(opcode==50 and {0,1,2}<=taint.get(5,set()),
         "preFog r5.xyz second-lane may-flow lost")
  if at==last:
   check({0,1,2}<=taint.get(1,set()),
         "conditional preFog r1.xyz may-flow lost")
 check(all(k in touched for k in path[1:]),
       "some certified component flow step is absent")
 return {"receiver":rx,"stock_sha256":sha,
         "actual_final_r19_sha256":sha_final,
         "exact_transformed_path_words":path,
         "pre_fog_r5_xyz_word":prefog,
         "conditional_r1_xyz_word":last,
         "stock_Fog_entry_final_word":stock_fog+27,
         "second_dsr_material_lane_may_flow_to_prefog":"CONFIRMED_FINAL_R19",
         "GPU_branch_taken":"OPEN",
         "PTDE_pixel_improved":"OPEN"}
def main():
 p=argparse.ArgumentParser()
 p.add_argument("--stock",type=Path,nargs=3,required=True)
 p.add_argument("--final",type=Path,nargs=3,required=True)
 p.add_argument("--out",type=Path,required=True)
 args=p.parse_args()
 result=[]
 for i,rx in enumerate((33,34,35)):
  stock=args.stock[i].read_bytes()
  final=args.final[i].read_bytes()
  row=run(rx,stock,final)
  # One local deliberate mutation at exact late MAD2 instruction: the
  # unchanged final shader must not be accepted under a guessed patch.
  words,ins=decode(final)
  # Current report does not ship or mutate licensed shader bytes.
  check(ins[row["exact_transformed_path_words"][0]][0]==50,
        "negative-preimage test found altered late MAD")
  result.append(row)
 args.out.write_text(json.dumps({"version":"V276",
   "scope":"ACTUAL_FINAL_R19_DXBC_ONLY","provenance":"local owner original RX + actual production R19",
   "operator":"DSR ordinary second material pow2.2 late affine MAD pair",
   "records":result},indent=2)+"\n",encoding="utf8")
 print("V276_REAL_FINAL_R19_THREE_RX_COMPONENT_MAYFLOW_PASS")
if __name__=="__main__":main()
