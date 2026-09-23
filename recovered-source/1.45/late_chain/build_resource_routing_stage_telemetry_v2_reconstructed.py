#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib
INPUT_SHA='45c7023cb6d2c1680712ca32dae6af25c3c406e7ce233d2555a441be67653928'
OUTPUT_SHA='e15747f4920bd5f40db9019e4e45110ad4655b4defab1ad3027b039fc022d98b'
PATCHES=[
    (0x6461, bytes.fromhex('3b'), bytes.fromhex('1b')),
    (0x647f, bytes.fromhex('488b15b2061000'), bytes.fromhex('8b15ffbe100090')),
    (0x64a1, bytes.fromhex('ef'), bytes.fromhex('e3')),
    (0x64bf, bytes.fromhex('488b1512061000'), bytes.fromhex('8b15c7be100090')),
    (0x64df, bytes.fromhex('488b1512061000'), bytes.fromhex('8b15abbe100090')),
    (0x64ff, bytes.fromhex('488b15fa051000'), bytes.fromhex('8b158fbe100090')),
    (0x651f, bytes.fromhex('488b15e2051000'), bytes.fromhex('8b1573be100090')),
    (0x653f, bytes.fromhex('488b15ca051000'), bytes.fromhex('8b1557be100090')),
    (0x655f, bytes.fromhex('488b15e2051000'), bytes.fromhex('8b153bbe100090')),
    (0x100571, bytes.fromhex('50433130313d'), bytes.fromhex('4e524d3d0000')),
    (0x100589, bytes.fromhex('504449463d'), bytes.fromhex('4e533d0000')),
    (0x1005a1, bytes.fromhex('42494e443d'), bytes.fromhex('4e473d0000')),
    (0x1005b2, bytes.fromhex('4f4e'), bytes.fromhex('4946')),
    (0x1005c9, bytes.fromhex('53454c3d'), bytes.fromhex('44533d00')),
    (0x1005d9, bytes.fromhex('554e4d3d'), bytes.fromhex('44473d00')),
    (0x1005e9, bytes.fromhex('4449463d'), bytes.fromhex('43443d00')),
    (0x100602, bytes.fromhex('3130313d'), bytes.fromhex('4e3d0000')),
    (0x100619, bytes.fromhex('4e524d3d'), bytes.fromhex('43533d00')),
]
def sha(b): return hashlib.sha256(b).hexdigest()
def main():
    ap=argparse.ArgumentParser(description='Exact historical Resource Routing Recovery V1 -> Stage Telemetry V2. Telemetry-only over recovered routing.')
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
