#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib
INPUT_SHA='f869e518bd6fd319f7039bd1cb33766abcf4ddc40a86ac922a159f495f75d7a1'
OUTPUT_SHA='d02af39ae6c0dc38525dac930ded3030a3440491028072cb8db111316c4a4442'
PATCHES=[
    (0x643f, bytes.fromhex('488b'), bytes.fromhex('0fb6')),
    (0x6442, bytes.fromhex('a206'), bytes.fromhex('bab2')),
    (0x645f, bytes.fromhex('488b158a061000'), bytes.fromhex('8b153bbf100090')),
    (0x649f, bytes.fromhex('488b159a061000'), bytes.fromhex('8b15efbe100090')),
    (0x657f, bytes.fromhex('488b15ca051000'), bytes.fromhex('8b155fb1100090')),
    (0x100518, bytes.fromhex('000000000000'), bytes.fromhex('204641494c3d')),
    (0x100528, bytes.fromhex('000000000000'), bytes.fromhex('20423132483d')),
    (0x100538, bytes.fromhex('000000000000'), bytes.fromhex('20423132433d')),
    (0x100548, bytes.fromhex('0000000000'), bytes.fromhex('205355423d')),
    (0x100558, bytes.fromhex('0000000000'), bytes.fromhex('20454e563d')),
    (0x100570, bytes.fromhex('00000000000000'), bytes.fromhex('2050433130313d')),
    (0x100588, bytes.fromhex('000000000000'), bytes.fromhex('20504449463d')),
    (0x1005a0, bytes.fromhex('000000000000'), bytes.fromhex('2042494e443d')),
    (0x1005b0, bytes.fromhex('0000000000'), bytes.fromhex('20444f4e3d')),
    (0x1005c8, bytes.fromhex('0000000000'), bytes.fromhex('2053454c3d')),
    (0x1005d8, bytes.fromhex('0000000000'), bytes.fromhex('20554e4d3d')),
    (0x1005e8, bytes.fromhex('0000000000'), bytes.fromhex('204449463d')),
    (0x100600, bytes.fromhex('000000000000'), bytes.fromhex('20433130313d')),
    (0x100618, bytes.fromhex('0000000000'), bytes.fromhex('204e524d3d')),
    (0x100620, bytes.fromhex('0000000000'), bytes.fromhex('205350433d')),
    (0x100628, bytes.fromhex('00000000'), bytes.fromhex('2054303d')),
    (0x100630, bytes.fromhex('000000000000'), bytes.fromhex('20433130303d')),
    (0x100640, bytes.fromhex('00000000'), bytes.fromhex('544f543d')),
]
def sha(b): return hashlib.sha256(b).hexdigest()
def main():
    ap=argparse.ArgumentParser(description='Exact historical TELEMETRY_ONLY V1 -> TELEMETRY V2 labeled periodic activation fields. Telemetry-only.')
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
