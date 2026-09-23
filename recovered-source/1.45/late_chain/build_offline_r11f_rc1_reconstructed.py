#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib
INPUT_SHA='41690c6212157eb772ae0c75c055d0bb7a842709f65f02c3689b87714151e1f7'
OUTPUT_SHA='a5dc6f817e75e157bd92ee945830d0dd3028ba887f544ebf6cff9976e149c900'
PATCHES=[
    (0x1b58ae, bytes.fromhex('c16c3fd75bcf34f3'), bytes.fromhex('6c660c9256e2b127')),
    (0x1b58c6, bytes.fromhex('cc075da6da1ad10c'), bytes.fromhex('9e53bfc0c3c8663e')),
    (0x1b58de, bytes.fromhex('9440ee4a3ca580fe'), bytes.fromhex('d39cab130ebba238')),
    (0x1b58f2, bytes.fromhex('e7'), bytes.fromhex('fb')),
    (0x1b58f4, bytes.fromhex('74d07a2ce4c3'), bytes.fromhex('b22c4f223e4b')),
    (0x1b5ad0, bytes.fromhex('47'), bytes.fromhex('31')),
    (0x1b5ad2, bytes.fromhex('42'), bytes.fromhex('31')),
    (0x1b5ad4, bytes.fromhex('41'), bytes.fromhex('46')),
    (0x1b7590, bytes.fromhex('1c'), bytes.fromhex('1a')),
    (0x1b75a0, bytes.fromhex('1c'), bytes.fromhex('1a')),
]
def sha(b): return hashlib.sha256(b).hexdigest()
def main():
    ap=argparse.ArgumentParser(description='Exact historical 41690 -> offline R11F carrier RC1. Renderer routing/DXBC are preserved; only carrier format/path/hash bytes change.')
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
