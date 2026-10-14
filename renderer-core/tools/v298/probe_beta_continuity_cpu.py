#!/usr/bin/env python3
"""Owner-source six-PS3 PTDE LightScattering probe interpolation CPU test."""
import argparse,hashlib,json,math,random,struct,sys,zipfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"v293"))
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"v296"))
from ptde_stable_ls_cpu_oracle import instructions,reg,execute
from verify_stable_lerp_ls_shared_operator import FPO,PTDE
def evaluate(archive,pairs=800):
 ops={}
 with zipfile.ZipFile(archive) as z:
  for key,(suffix,expected) in FPO.items():
   matches=[n for n in z.namelist() if n.endswith(suffix)]
   if len(matches)!=1:raise ValueError("FPO ambiguity")
   b=z.read(matches[0])
   if hashlib.sha256(b).hexdigest()!=expected:raise ValueError("FPO identity")
   ins=list(instructions(b))
   def uses(e,k):return any(reg(t)==(2,k) for t in e[2])
   a=next(i for i,e in enumerate(ins) if uses(e,107))
   b=next(i for i in range(a,len(ins)) if ins[i][1]==18 and uses(ins[i],110))
   seq=ins[a:b+1]
   flat=[v for e in seq for v in e[3]]
   if len(flat)!=76 or hashlib.sha256(struct.pack("<76I",*flat)).hexdigest()!=PTDE:
    raise ValueError("nonmatching scattering")
   ops[key]=seq
 rng=random.Random(0x298);maxaff=0.;family=0.;gamma=0;mxgamma=0.
 for _ in range(pairs):
  A=[rng.uniform(.04,.65) for _ in range(3)]
  B=[rng.uniform(.04,.65) for _ in range(3)]
  cos=rng.uniform(-.8,.8);ext=[rng.uniform(.1,.91) for _ in range(3)]
  cb={i:[rng.uniform(.05,.8) for _ in range(4)] for i in range(14,20)}
  cb[16]=[rng.uniform(.1,.6),rng.uniform(1,2),rng.uniform(.05,.4),.1]
  cb[19][3]=rng.uniform(.05,.9) # c110.w held fixed, not probe beta
  def f(scene,op):
   st={(0,0):(*scene,cos),(0,1):(0.,)*4,(0,2):(0.,)*4,
       (0,3):(*[math.log2(x) for x in ext],0.),(2,0):(1.,)*4}
   for i in range(14,20):st[(2,i+91)]=cb[i]
   return execute(op,st)
  core=ops['stable',33];left=f(A,core);right=f(B,core)
  for t in (0.,.25,.5,.75,1.):
   v=f([(1-t)*x+t*y for x,y in zip(A,B)],core)
   exp=[(1-t)*x+t*y for x,y in zip(left,right)]
   maxaff=max(maxaff,max(abs(x-y) for x,y in zip(v,exp)))
   for op in ops.values():family=max(family,max(abs(x-y) for x,y in zip(v,f([(1-t)*x+t*y for x,y in zip(A,B)],op))))
   gap=max(abs(x-x**2.2) for x in v)
   if gap>1e-4:gamma+=1
   mxgamma=max(mxgamma,gap)
 if maxaff>=1e-6 or family!=0 or gamma<.95*pairs*5:raise AssertionError("scattering continuity failed")
 return {"owner_families":6,"paired_probe_trials":pairs,"beta_samples":pairs*5,
         "affine_rgb_max_error":maxaff,"family_rgb_max_error":family,
         "postgamma_sensitive":gamma,"postgamma_max_gap":mxgamma,
         "scattering_c110_w":"fixed separate from EnvSpec beta",
         "runtime":"OFF","pixel":"OPEN","fog":"STOCK_DSR"}
if __name__=="__main__":
 p=argparse.ArgumentParser();p.add_argument("owner_ptde_fpo_archive");p.add_argument("--report")
 a=p.parse_args();r=evaluate(a.owner_ptde_fpo_archive)
 if a.report:Path(a.report).write_text(json.dumps(r,indent=2)+"\n")
 print("V298_SOURCE_PAIRED_BETA_CPU_PASS",json.dumps(r))
