#!/usr/bin/env python3
from __future__ import annotations
import argparse,glob,re,struct
from pathlib import Path

WATER_RE=re.compile(r"^\d+_FRPG_Water_(?:Env|Reflect).+\.fpo$")
EXPECTED=12
OP_CUSTOM=53
OP_MAX=52
OP_LOG=47
OP_MUL=56
OP_EXP=25
CB0_11_X=(0x0020800a,0x00000000,0x0000000b)

def shader_words(path:Path):
    b=path.read_bytes()
    if len(b)<36 or b[:4]!=b"DXBC": raise SystemExit(f"{path.name}: invalid DXBC")
    count=struct.unpack_from("<I",b,28)[0]
    found=None
    for i in range(count):
        off=struct.unpack_from("<I",b,32+i*4)[0]
        if off+8>len(b): raise SystemExit(f"{path.name}: invalid chunk")
        tag=b[off:off+4]; size=struct.unpack_from("<I",b,off+4)[0]
        if tag in (b"SHEX",b"SHDR"):
            if found is not None: raise SystemExit(f"{path.name}: duplicate code chunk")
            found=list(struct.unpack("<%dI"%(size//4),b[off+8:off+8+size]))
    if found is None: raise SystemExit(f"{path.name}: no code chunk")
    return found

def instructions(words):
    out=[]; at=2
    while at<len(words):
        token=words[at]; op=token&0x7ff
        length=words[at+1] if op==OP_CUSTOM else ((token>>24)&0x7f)
        if not length or at+length>len(words):
            raise SystemExit("invalid instruction stream")
        out.append((op,words[at:at+length]))
        at+=length
    return out

def has_cb011(inst):
    for i in range(1,len(inst)-2):
        if tuple(inst[i:i+3])==CB0_11_X:
            return True
    return False

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--shader-dir",required=True)
    a=ap.parse_args()
    paths=[p for p in sorted(Path(a.shader_dir).glob("*.fpo")) if WATER_RE.fullmatch(p.name)]
    hits=[]
    for p in paths:
        ins=instructions(shader_words(p))
        for i,(op,words) in enumerate(ins):
            if op!=OP_MUL or not has_cb011(words): continue
            if i<2 or i+1>=len(ins): continue
            if [ins[i-2][0],ins[i-1][0],ins[i][0],ins[i+1][0]] != [OP_MAX,OP_LOG,OP_MUL,OP_EXP]:
                continue
            hits.append(p.name)
            break
    if len(hits)!=EXPECTED:
        raise SystemExit(f"expected {EXPECTED} native g_SpecularPower lowerings, got {len(hits)}")
    print(f"DSR_NATIVE_SPECULAR_POWER_LOWERING_PASS shaders={len(hits)} sequence=MAX,LOG,MUL,EXP exponent=cb0[11].x")
    return 0

if __name__=="__main__": raise SystemExit(main())
