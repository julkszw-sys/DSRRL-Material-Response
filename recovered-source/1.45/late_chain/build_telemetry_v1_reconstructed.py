#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib
INPUT_SHA='6df899936098ac3a22751224958090c5860294e80769e8cc6afd8e0ae316da0a'
OUTPUT_SHA='f869e518bd6fd319f7039bd1cb33766abcf4ddc40a86ac922a159f495f75d7a1'
PATCHES=[
    (0x5b6f, bytes.fromhex('f048ff05a90f1000'), bytes.fromhex('e9c14a1000909090')),
    (0x107835, bytes.fromhex('000000'), bytes.fromhex('4c8d15')),
    (0x10783c, bytes.fromhex('0000000000'), bytes.fromhex('41bb3cb210')),
    (0x107842, bytes.fromhex('0000000000'), bytes.fromhex('4d29dab801')),
    (0x10784a, bytes.fromhex('0000000000000000'), bytes.fromhex('f0490fc182207710')),
    (0x107853, bytes.fromhex('0000000000000000000000'), bytes.fromhex('4885c07531498b8a082011')),
    (0x10785f, bytes.fromhex('0000000000000000000000'), bytes.fromhex('4885c974254d8b8a102011')),
    (0x10786b, bytes.fromhex('0000000000000000000000000000'), bytes.fromhex('4d85c974194c895424204c8d0517')),
    (0x10787c, bytes.fromhex('0000'), bytes.fromhex('ba03')),
    (0x107881, bytes.fromhex('00000000000000000000000000'), bytes.fromhex('41ffd14c8b5424204981c27767')),
    (0x107890, bytes.fromhex('0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'), bytes.fromhex('41ffe25b445352524c20312e34352054454c454d455452595d205355425355524620524f5554453320414354495645')),
    (0x10f919, bytes.fromhex('000000000000000000000000000000000000000000000000000000000000000000000000000000000000'), bytes.fromhex('5b445352524c20312e34352054454c454d455452595d205350454352474220743130204143544956450a')),
    (0x19e534, bytes.fromhex('000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'), bytes.fromhex('5b445352524c20312e34352054454c454d455452595d204e4f524d414c207432204143544956450a5b445352524c20312e34352054454c454d455452595d2044494646555345207430204143544956450a')),
    (0x1b78c9, bytes.fromhex('00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'), bytes.fromhex('5b445352524c20312e34352054454c454d455452595d20454e5653504543207431322b743134204143544956450a')),
]
def sha(b): return hashlib.sha256(b).hexdigest()
def main():
    ap=argparse.ArgumentParser(description='Exact historical Safe Island RC2 -> TELEMETRY_ONLY V1. Telemetry-only; no renderer math/resource-bind semantic change.')
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
