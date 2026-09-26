#!/usr/bin/env python3
from __future__ import annotations
import argparse,re,struct
from pathlib import Path

TARGET=re.compile(r"^FRPG_Phn_DifSpc.*HemEnv(?:Lerp)?PntSS(?:SS)?\.fpo$")
PS30=0xffff0300
OP_COMMENT=0xfffe
OP_END=0xffff
OP_POW=32
C102_TOKEN=0xa0000066

def words(p:Path):
    b=p.read_bytes()
    if len(b)%4: raise SystemExit(f"{p.name}: unaligned bytecode")
    return list(struct.unpack("<%dI"%(len(b)//4),b))

def pow_tokens(p:Path):
    w=words(p)
    if not w or w[0]!=PS30: raise SystemExit(f"{p.name}: not ps_3_0")
    out=[];i=1
    while i<len(w):
        t=w[i];op=t&0xffff
        if op==OP_END: break
        ln=(((t>>16)&0x7fff)+1) if op==OP_COMMENT else (((t>>24)&0xf)+1)
        if ln<=0 or i+ln>len(w): raise SystemExit(f"{p.name}: bad instruction length")
        if op==OP_POW: out.append(w[i:i+ln])
        i+=ln
    return out

def main()->int:
    ap=argparse.ArgumentParser()
    ap.add_argument("--ptde-dir",required=True)
    a=ap.parse_args()
    targets=[p for p in sorted(Path(a.ptde_dir).glob("*.fpo")) if TARGET.fullmatch(p.name)]
    if len(targets)!=96: raise SystemExit(f"expected 96 fixed PTDE references, got {len(targets)}")
    total=0
    for p in targets:
        ps=pow_tokens(p)
        expected=4 if "PntSSSS" in p.name else 2
        if len(ps)!=expected:
            raise SystemExit(f"{p.name}: expected {expected} POW, got {len(ps)}")
        for inst in ps:
            if len(inst)!=4 or inst[3]!=C102_TOKEN:
                raise SystemExit(f"{p.name}: POW exponent is not c102")
        total+=len(ps)
    if total!=288:
        raise SystemExit(f"expected 288 POW sites, got {total}")
    print(f"PTDE_FIXED_SPECULAR_POW_AUDIT_PASS shaders={len(targets)} pow_sites={total} exponent=c102")
    return 0

if __name__=="__main__": raise SystemExit(main())
