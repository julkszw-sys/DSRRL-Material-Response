#!/usr/bin/env python3
from __future__ import annotations
import argparse, re, struct
from pathlib import Path

TARGET=re.compile(r"^\d+_FRPG_Phn_DifSpc.*Mul.*_HemEnvPntSS(?:SS)?\.fpo$")
SAMPLE_OPS=set(range(0x45,0x4b))
CUSTOM=53
ADD=0
MAD=50

def words_for(p:Path)->list[int]:
    b=p.read_bytes()
    if b[:4]!=b"DXBC":
        raise SystemExit(f"{p.name}: not DXBC")
    count=struct.unpack_from("<I",b,28)[0]
    out=None
    for i in range(count):
        off=struct.unpack_from("<I",b,32+4*i)[0]
        tag=b[off:off+4]
        size=struct.unpack_from("<I",b,off+4)[0]
        if tag in (b"SHEX",b"SHDR"):
            if out is not None:
                raise SystemExit(f"{p.name}: multiple code chunks")
            payload=b[off+8:off+8+size]
            out=list(struct.unpack("<%dI"%(len(payload)//4),payload))
    if out is None or out[1]!=len(out):
        raise SystemExit(f"{p.name}: invalid code chunk")
    return out

def decode(words:list[int]):
    out=[]; at=2
    while at<len(words):
        tok=words[at]; op=tok&0x7ff
        ln=words[at+1] if op==CUSTOM else (tok>>24)&0x7f
        if ln<=0 or at+ln>len(words):
            raise SystemExit("bad instruction stream")
        out.append((at,op,ln,words[at:at+ln]))
        at+=ln
    return out

def diff_add(ins,a,b):
    at,op,ln,w=ins
    return (op==ADD and ln==8 and
            w[1]==0x00100072 and w[2]==b and
            w[3]==0x80100246 and w[4]==0x00000041 and
            w[5]==a and w[6]==0x00100246 and w[7]==b)

def blend_mad(ins,a,b):
    at,op,ln,w=ins
    if not (op==MAD and ln==9 and
            w[1]==0x00100072 and w[2]==a and
            w[5]==0x00100246 and w[6]==b and
            w[7]==0x00100246 and w[8]==a):
        return None
    tok,reg=w[3],w[4]
    if ((tok>>12)&0xff)!=1 or ((tok>>20)&0x3)!=1:
        return None
    return tok,reg,at

def main()->int:
    ap=argparse.ArgumentParser()
    ap.add_argument("--shader-dir",required=True)
    a=ap.parse_args()
    files=[p for p in sorted(Path(a.shader_dir).glob("*.fpo")) if TARGET.fullmatch(p.name)]
    if len(files)!=24:
        raise SystemExit(f"expected 24 blended fixed Spc bodies, got {len(files)}")

    regs={}
    for p in files:
        words=words_for(p); ins=decode(words)
        samples={}
        for i,x in enumerate(ins):
            at,op,ln,w=x
            if op in SAMPLE_OPS and ln==11 and w[8] in (0,1,3,4):
                if w[8] in samples:
                    raise SystemExit(f"{p.name}: duplicate t{w[8]}")
                samples[w[8]]=(i,w[4])
        if set(samples)!={0,1,3,4}:
            raise SystemExit(f"{p.name}: incomplete endpoint set")

        i1,r1=samples[1]; i4,r4=samples[4]
        i0,r0=samples[0]; i3,r3=samples[3]
        if i4!=i1+1 or i3!=i0+1:
            raise SystemExit(f"{p.name}: endpoint sample order mismatch")
        if i4+2>=len(ins) or not diff_add(ins[i4+1],r1,r4):
            raise SystemExit(f"{p.name}: missing spec B-A")
        spec=blend_mad(ins[i4+2],r1,r4)
        if spec is None:
            raise SystemExit(f"{p.name}: missing spec lerp MAD")

        diff=None
        for j in range(i3+1,min(i3+5,len(ins)-1)):
            if diff_add(ins[j],r0,r3):
                diff=blend_mad(ins[j+1],r0,r3)
                if diff is not None:
                    break
        if diff is None:
            raise SystemExit(f"{p.name}: missing diffuse lerp MAD")
        if spec[:2]!=diff[:2]:
            raise SystemExit(f"{p.name}: spec/diff blend operand mismatch")
        regs[spec[1]]=regs.get(spec[1],0)+1

    print("FIXED_SPC_BLEND_LERP_AUDIT_PASS bodies=24 same_weight_spec_diff=24 registers="+
          ",".join(f"v{k}:{regs[k]}" for k in sorted(regs)))
    return 0

if __name__=="__main__":
    raise SystemExit(main())
