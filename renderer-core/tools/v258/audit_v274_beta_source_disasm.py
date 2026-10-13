#!/usr/bin/env python3
"""V274 external binary disassembly audit. Needs GNU objdump and original EXEs."""
import argparse,hashlib,json,subprocess
from pathlib import Path
from audit_v273_ptde_common_blend_scalar import validate as ptde_validate
DSR_SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
def segment(exe,start,end):
 r=subprocess.run(["objdump","-d","-M","intel",f"--start-address={start}",f"--stop-address={end}",str(exe)],capture_output=True,text=True,check=True)
 return r.stdout.lower()
def require(text,*bits):
 for bit in bits:
  if bit not in text:raise ValueError("instruction mismatch: "+bit)
def main():
 p=argparse.ArgumentParser()
 p.add_argument("--ptde",type=Path,required=True)
 p.add_argument("--dsr",type=Path,required=True)
 p.add_argument("--out",type=Path,required=True)
 a=p.parse_args()
 ptde_validate(a.ptde.read_bytes())
 if hashlib.sha256(a.dsr.read_bytes()).hexdigest()!=DSR_SHA:raise ValueError("wrong original DSR")
 ptext=segment(a.ptde,"0xf9521d","0xf95235")
 require(ptext,"f95223:","[esi+0x8]","f95228:","[esp+0x2c]")
 dtext=segment(a.dsr,"0x14021639c","0x140216455")
 require(dtext,"14021639c:","[rbp+0x20]","14021643b:","[rdi+0x114]","14021644e:","[rbp+0x70]")
 alt1=segment(a.dsr,"0x14021745c","0x14021746a")
 alt2=segment(a.dsr,"0x140217a33","0x140217a44")
 require(alt1,"140217463:","[rbp+0x4c]")
 require(alt2,"140217a3b:","[rbp-0x34]")
 report={
 "original_source_cut":"CONFIRMED",
 "PTDE":"source+8 -> common interior scalar t -> c84.w=c85.w (V273)",
 "DSR":"source+0x114 -> descriptor+0x50, no scalar arithmetic at this cut",
 "DSR_alternate_builders":"two synthetic zero tuples, not authenticated LightBank",
 "ptde_dsr_same_draw_t_equality":"OPEN",
 "original_PTDE_and_DSR_pixel":"OPEN",
 }
 a.out.write_text(json.dumps(report,indent=2)+"\n",encoding="utf8")
 print("V274_ORIGINAL_EXECUTABLE_BETA_SOURCE_CUT_PASS")
if __name__=="__main__":main()
