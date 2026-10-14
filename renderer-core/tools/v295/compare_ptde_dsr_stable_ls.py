#!/usr/bin/env python3
"""V295 paired PTDE/DSR Stable LightScattering operator-local CPU differential.
Inputs: one owner PTDE FPO archive and three original DSR RX33/34/35 DXBC.
NO shader blobs shipped, and NO runtime/pixel equivalence claim.
"""
import argparse,hashlib,json,math,random,struct,sys,zipfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"v293"))
from ptde_stable_ls_cpu_oracle import cut,execute,NAMES
from dsr_stable_ls_tail_cpu import decode_instructions,execute as dsr_execute

SOURCE={33:"35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24",
34:"d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837",
35:"7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701"}
POST={33:2682,34:2601,35:2254}
TAIL_SHA="d91051cb7545eddfa64feda8cd024e1ecc94f0d383d6dfb79462c99f65233f14"
def words(b):
 if b[:4]!=b"DXBC" or struct.unpack_from("<I",b,24)[0]!=len(b):
  raise ValueError("untrusted DSR envelope")
 n=struct.unpack_from("<I",b,28)[0]
 if not 1<=n<=24:raise ValueError("invalid chunk count")
 locations=[struct.unpack_from("<I",b,32+4*i)[0] for i in range(n)]
 chunks=[p for p in locations if b[p:p+4]==b"SHEX"]
 if len(chunks)!=1:raise ValueError("no unique SHEX")
 p=chunks[0];size=struct.unpack_from("<I",b,p+4)[0]
 if size%4 or p+8+size>len(b):raise ValueError("SHEX overflow")
 w=struct.unpack_from("<"+"I"*(size//4),b,p+8)
 if w[1]!=len(w):raise ValueError("wrong SHEX size")
 return w
def sha(b):return hashlib.sha256(b).hexdigest()
def differential(ptde_archive,dsr_paths,per_receiver=1800,seed=0x289493):
 rng=random.Random(seed)
 results=[]
 with zipfile.ZipFile(ptde_archive) as archive:
  for rx,path in zip((33,34,35),dsr_paths):
   matches=[n for n in archive.namelist() if n.endswith(NAMES[rx])]
   if len(matches)!=1:raise ValueError("missing or ambiguous PTDE FPO")
   ptde=cut(archive.read(matches[0]),rx)
   data=Path(path).read_bytes()
   if sha(data)!=SOURCE[rx]:raise ValueError("unauthorized original DSR shader RX"+str(rx))
   w=words(data);end=POST[rx];start=end-129
   segment=struct.pack("<129I",*w[start:end])
   if sha(segment)!=TAIL_SHA:raise ValueError("DSR LS tail changed")
   seq=decode_instructions(w,start,end)
   biggest=0.;mean=0.;gamma_different=0
   for j in range(per_receiver):
    cos=rng.uniform(-.8,.8)
    atten=[rng.uniform(.05,.95) for _ in range(3)]+[1.]
    scene=[rng.uniform(.02,.78) for _ in range(3)]+[1.]
    cb={i:[rng.uniform(.05,.9) for _ in range(4)] for i in range(14,20)}
    cb[16]=[rng.uniform(.07,.8),rng.uniform(1.,2.),rng.uniform(.03,.4),.1]
    cb[19][3]=rng.uniform(.05,.95)
    pstate={(0,0):(*scene[:3],cos),(0,1):(0.,0.,0.,0.),
            (0,2):(0.,0.,0.,0.),
            (0,3):(*[math.log2(x) for x in atten[:3]],0.),
            (2,0):(1.,1.,1.,1.)}
    for i in range(14,20):pstate[(2,i+91)]=cb[i]
    p=execute(ptde,pstate)
    dstate={("r",0):[0.,cos,cos*cos+1.,0.],
            ("r",1):scene,("r",2):atten,
            ("r",3):[atten[i]*cb[14][i] for i in range(4)],
            **{("cb",0,i):cb[i] for i in cb}}
    d=dsr_execute(seq,dstate)[("r",0)][:3]
    error=max(abs(a-b) for a,b in zip(p,d))
    biggest=max(biggest,error);mean+=error
    if any(abs(v-v**2.2)>1.e-4 for v in p if v>0):
     gamma_different+=1
   if biggest>=1.e-6 or gamma_different<per_receiver*0.9:
    raise AssertionError("core mismatch or insensitivity to post-LS gamma")
   results.append(dict(rx=rx,matched_trials=per_receiver,max_abs_error=biggest,
                       mean_max_channel_error=mean/per_receiver,
                       postgamma_sensitive_trials=gamma_different,
                       dsr_tail_sha256=TAIL_SHA,ptde_operator_ops=19,
                       dsr_tail_ops=15))
 return {"scope":"synthetic_equal_semantic_operator_inputs",
         "not_claimed":"runtime CB equality or PTDE pixel equivalence",
         "fog":"unchanged_stock_DSR","seed":seed,"receivers":results}
if __name__=="__main__":
 p=argparse.ArgumentParser()
 p.add_argument("ptde_owner_zip")
 p.add_argument("dsr_rx33");p.add_argument("dsr_rx34");p.add_argument("dsr_rx35")
 p.add_argument("--trials",type=int,default=1800);p.add_argument("--report")
 a=p.parse_args()
 result=differential(a.ptde_owner_zip,[a.dsr_rx33,a.dsr_rx34,a.dsr_rx35],a.trials)
 if a.report:Path(a.report).write_text(json.dumps(result,indent=2)+"\n")
 print("V295_PTDE_DSR_LS_OPERATOR_LOCAL_NUMERICAL_MATCH_PASS",json.dumps(result))
