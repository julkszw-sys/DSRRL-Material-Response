#!/usr/bin/env python3
from __future__ import annotations
import argparse,re,struct
from collections import Counter
from pathlib import Path

TARGET=re.compile(r"^FRPG_Phn_DifSpc.*Mul.*_HemEnvPntSS(?:SS)?\.fpo$")
ADD=2
LRP=18
TEXLD=66

def rnum(tok:int)->int:
    return tok & 0x7ff

def rtype(tok:int)->int:
    return ((tok >> 28) & 7) | ((tok >> 8) & 0x18)

def is_sampler(tok:int,n:int)->bool:
    return (tok & 0x80000000)!=0 and rtype(tok)==10 and rnum(tok)==n

def is_const(tok:int,n:int)->bool:
    return (tok & 0x80000000)!=0 and rtype(tok)==2 and rnum(tok)==n

def main()->int:
    ap=argparse.ArgumentParser()
    ap.add_argument("--shader-dir",required=True)
    a=ap.parse_args()
    files=[p for p in sorted(Path(a.shader_dir).glob("*.fpo"))
           if TARGET.fullmatch(p.name)]
    if len(files)!=24:
        raise SystemExit(f"expected 24 PTDE blended fixed Spc bodies, got {len(files)}")

    weights=Counter()
    for p in files:
        b=p.read_bytes()
        if len(b)<4 or len(b)%4:
            raise SystemExit(f"{p.name}: invalid token length")
        words=list(struct.unpack("<%dI"%(len(b)//4),b))
        found=None
        for i in range(1,len(words)-18):
            # ps_3_0: instruction length field counts parameter DWORDs.
            if (words[i]&0xffff)!=TEXLD or ((words[i]>>24)&0xf)!=3:
                continue
            if not is_sampler(words[i+3],3):
                continue

            j=i+4
            if (words[j]&0xffff)!=ADD or ((words[j]>>24)&0xf)!=3:
                continue
            if not is_const(words[j+3],156):
                continue

            k=j+4
            if (words[k]&0xffff)!=TEXLD or ((words[k]>>24)&0xf)!=3:
                continue
            if not is_sampler(words[k+3],0):
                continue

            l=k+4
            if (words[l]&0xffff)!=LRP or ((words[l]>>24)&0xf)!=4:
                continue

            tex_b=words[i+1]&0x7ff
            add_dst=words[j+1]&0x7ff
            tex_a=words[k+1]&0x7ff
            lrp_weight=words[l+2]
            lrp_b=words[l+3]&0x7ff
            lrp_a=words[l+4]&0x7ff

            # lrp dst, w, B, A == w*B + (1-w)*A.
            if add_dst!=tex_b or lrp_b!=add_dst or lrp_a!=tex_a:
                continue

            found=rnum(lrp_weight)
            break

        if found is None:
            raise SystemExit(f"{p.name}: missing s3 -> add c156 -> s0 -> lrp shape")
        weights[found]+=1

    print(
        "PTDE_FIXED_SPC_BLEND_PREOP_AUDIT_PASS "
        "bodies=24 homologous_c156_preop=24 weight_registers="+
        ",".join(f"v{k}:{weights[k]}" for k in sorted(weights)))
    return 0

if __name__=="__main__":
    raise SystemExit(main())
