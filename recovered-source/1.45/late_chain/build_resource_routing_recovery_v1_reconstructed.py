#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib
INPUT_SHA='d02af39ae6c0dc38525dac930ded3030a3440491028072cb8db111316c4a4442'
OUTPUT_SHA='45c7023cb6d2c1680712ca32dae6af25c3c406e7ce233d2555a441be67653928'
PATCHES=[
    (0x193fd6, bytes.fromhex('e8251b02000f84bc'), bytes.fromhex('83f9170f83beffff')),
    (0x193fdf, bytes.fromhex('ffff'), bytes.fromhex('9090')),
]
def sha(b): return hashlib.sha256(b).hexdigest()
def main():
    ap=argparse.ArgumentParser(description='Exact historical Telemetry V2 -> Resource Routing Recovery V1. Restores V12 ordinary DifSpcBmp local receiver gate 0..22 at common Diffuse PREPARE.')
    ap.add_argument('basis',type=Path); ap.add_argument('output',type=Path); a=ap.parse_args()
    data=bytearray(a.basis.read_bytes())
    if sha(data)!=INPUT_SHA: raise SystemExit(f'refusing unknown basis {sha(data)}')
    for off,old,new in PATCHES:
        got=bytes(data[off:off+len(old)])
        if got!=old: raise SystemExit(f'guard mismatch at {off:#x}')
        if len(old)!=len(new): raise SystemExit('length-changing patch forbidden')
        data[off:off+len(new)]=new
    out=bytes(data)
    if sha(out)!=OUTPUT_SHA: raise SystemExit(f'output mismatch {sha(out)}')
    a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_bytes(out)
    print('PASS',OUTPUT_SHA)
if __name__=='__main__': main()
