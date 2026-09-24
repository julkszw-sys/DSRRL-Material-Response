#!/usr/bin/env python3
"""Normalize the historical V2.11 MSVC link timestamp for byte-exact reproduction.
The original build predates reproducible PE timestamps. The linker timestamp is present
both in IMAGE_FILE_HEADER.TimeDateStamp and IMAGE_DEBUG_DIRECTORY.TimeDateStamp.
No rendering/code/data bytes are changed.
"""
import argparse, hashlib, struct
from pathlib import Path
EXPECTED_TS=1789474613  # 2026-09-15 12:16:53 UTC
EXPECTED_SHA='1cfa2154059575d2dbf3f34124e7d51c4c63e8ab8f22347ae56755bc1e5f697a'

def sections(b,e):
    n=struct.unpack_from('<H',b,e+6)[0]; osz=struct.unpack_from('<H',b,e+20)[0]; st=e+24+osz
    out=[]
    for i in range(n):
        q=st+i*40; vs,va,rs,raw=struct.unpack_from('<IIII',b,q+8); out.append((va,max(vs,rs),raw))
    return out

def rva_to_off(rva,secs):
    for va,span,raw in secs:
        if va <= rva < va+span: return raw+(rva-va)
    raise ValueError(f'RVA not mapped: 0x{rva:x}')

def normalize(data,ts=EXPECTED_TS):
    b=bytearray(data); assert b[:2]==b'MZ'; e=struct.unpack_from('<I',b,0x3c)[0]; assert b[e:e+4]==b'PE\0\0'
    struct.pack_into('<I',b,e+8,ts)
    opt=e+24; magic=struct.unpack_from('<H',b,opt)[0]; assert magic==0x20b
    dd=opt+112; dbg_rva,dbg_sz=struct.unpack_from('<II',b,dd+6*8)
    if dbg_rva and dbg_sz:
        off=rva_to_off(dbg_rva,sections(b,e))
        assert dbg_sz%28==0
        for q in range(off,off+dbg_sz,28): struct.pack_into('<I',b,q+4,ts)
    return bytes(b)

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('input'); ap.add_argument('output'); ap.add_argument('--timestamp',type=int,default=EXPECTED_TS); ap.add_argument('--expect-exact',action='store_true'); a=ap.parse_args()
    out=normalize(Path(a.input).read_bytes(),a.timestamp); Path(a.output).write_bytes(out); h=hashlib.sha256(out).hexdigest(); print(h)
    if a.expect_exact and h!=EXPECTED_SHA: raise SystemExit(f'normalized SHA mismatch: {h} != {EXPECTED_SHA}')
if __name__=='__main__':main()
