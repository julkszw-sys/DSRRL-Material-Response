#!/usr/bin/env python3
from __future__ import annotations
import argparse,re,struct
from pathlib import Path

TARGET=re.compile(r"^\d+_FRPG_Phn_DifSpc.*_HemEnvPntSS(?:SS)?\.fpo$")
SAMPLE_OPS=set(range(0x45,0x4b))
CUSTOM=53

def shader_words(p:Path)->list[int]:
    b=p.read_bytes()
    if b[:4]!=b"DXBC": raise SystemExit(f"{p.name}: not DXBC")
    count=struct.unpack_from("<I",b,28)[0]
    hit=None
    for i in range(count):
        off=struct.unpack_from("<I",b,32+4*i)[0]
        tag=b[off:off+4]
        size=struct.unpack_from("<I",b,off+4)[0]
        if tag in (b"SHEX",b"SHDR"):
            if hit is not None: raise SystemExit(f"{p.name}: multiple code chunks")
            payload=b[off+8:off+8+size]
            hit=list(struct.unpack("<%dI"%(len(payload)//4),payload))
    if hit is None or hit[1]!=len(hit): raise SystemExit(f"{p.name}: bad code chunk")
    return hit

def sample_slots(words:list[int])->list[int]:
    out=[]; at=2
    while at<len(words):
        tok=words[at]; op=tok&0x7ff
        ln=words[at+1] if op==CUSTOM else (tok>>24)&0x7f
        if ln<=0 or at+ln>len(words): raise SystemExit("bad instruction stream")
        if op in SAMPLE_OPS and ln==11:
            out.append(words[at+8])
        at+=ln
    return out

def main()->int:
    ap=argparse.ArgumentParser()
    ap.add_argument("--shader-dir",required=True)
    a=ap.parse_args()
    targets=[p for p in sorted(Path(a.shader_dir).glob("*.fpo")) if TARGET.fullmatch(p.name)]
    if len(targets)!=48: raise SystemExit(f"expected 48 fixed Spc bodies, got {len(targets)}")
    single=blend=0
    for p in targets:
        slots=sample_slots(shader_words(p))
        for required in (0,1):
            if slots.count(required)!=1:
                raise SystemExit(f"{p.name}: t{required} count={slots.count(required)}")
        has3=slots.count(3)==1
        has4=slots.count(4)==1
        if has3!=has4:
            raise SystemExit(f"{p.name}: incomplete blend-B t3/t4 pair")
        if slots.count(3)>1 or slots.count(4)>1:
            raise SystemExit(f"{p.name}: duplicate blend-B endpoint")
        if has3:
            blend+=1
        else:
            single+=1
    if (single,blend)!=(24,24):
        raise SystemExit(f"topology mismatch single={single} blended={blend}")
    print("FIXED_SPC_ENDPOINT_AUDIT_PASS bodies=48 base_t0_t1=48 blend_t3_t4=24 single=24 blended=24")
    return 0

if __name__=="__main__": raise SystemExit(main())
