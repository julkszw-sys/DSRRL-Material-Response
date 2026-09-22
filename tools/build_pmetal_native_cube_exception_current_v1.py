#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path

EXPECTED_BASIS_SHA256="e15747f4920bd5f40db9019e4e45110ad4655b4defab1ad3027b039fc022d98b"
EXPECTED_OUTPUT_SHA256="3cb07d4f547fd0947ea132acb37b4b8c2410f426e55e012a3d053809222c5533"

HOOK_RVA=0x19ABD3
CAVE_RVA=0x10B2BF
FAIL_RVA=0x19AB9D
RESUME_RVA=0x19ABE7
EXPECTED=bytes.fromhex("8b491483f9170f83beffffff9090909090909090")

def sha(b): return hashlib.sha256(b).hexdigest()

def sections(b):
    pe=struct.unpack_from("<I",b,0x3c)[0]
    n=struct.unpack_from("<H",b,pe+6)[0]
    opt=struct.unpack_from("<H",b,pe+20)[0]
    base=pe+24+opt
    out=[]
    for i in range(n):
        o=base+i*40
        vs,va,rs,rp=struct.unpack_from("<IIII",b,o+8)
        out.append((va,vs,rs,rp))
    return out

def off(b,rva):
    for va,vs,rs,rp in sections(b):
        if va<=rva<va+max(vs,rs):
            return rp+(rva-va)
    raise ValueError(hex(rva))

def rel32(src_rva,insn_len,dst_rva):
    return struct.pack("<i",dst_rva-(src_rva+insn_len))

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("basis",type=Path)
    ap.add_argument("output",type=Path)
    a=ap.parse_args()
    original=a.basis.read_bytes()
    if sha(original)!=EXPECTED_BASIS_SHA256:
        raise SystemExit("refusing unknown basis")

    b=bytearray(original)
    ho=off(b,HOOK_RVA)
    co=off(b,CAVE_RVA)
    if bytes(b[ho:ho+len(EXPECTED)])!=EXPECTED:
        raise SystemExit("hook guard mismatch")
    if any(b[co:co+64]):
        raise SystemExit("code cave is not empty")

    b[ho:ho+5]=b"\xE9"+rel32(HOOK_RVA,5,CAVE_RVA)

    cave=bytearray()
    cave+=bytes.fromhex("8b4110")
    cave+=bytes.fromhex("3d59010000")
    src=CAVE_RVA+len(cave)
    cave+=b"\x0f\x84"+rel32(src,6,FAIL_RVA)
    cave+=bytes.fromhex("8b4914")
    cave+=bytes.fromhex("83f917")
    src=CAVE_RVA+len(cave)
    cave+=b"\x0f\x83"+rel32(src,6,FAIL_RVA)
    src=CAVE_RVA+len(cave)
    cave+=b"\xe9"+rel32(src,5,RESUME_RVA)
    b[co:co+len(cave)]=cave

    out=bytes(b)
    if sha(out)!=EXPECTED_OUTPUT_SHA256:
        raise SystemExit("unexpected output SHA")
    a.output.write_bytes(out)
    print("PASS",sha(out))

if __name__=="__main__":
    main()
