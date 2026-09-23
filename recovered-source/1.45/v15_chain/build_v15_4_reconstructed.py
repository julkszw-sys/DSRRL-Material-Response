from pathlib import Path
import struct, hashlib

def sha(b): return hashlib.sha256(b).hexdigest()

def sections(data):
    e=struct.unpack_from('<I',data,0x3c)[0]
    n=struct.unpack_from('<H',data,e+6)[0]
    opt=struct.unpack_from('<H',data,e+20)[0]
    sec=e+24+opt
    out=[]
    for i in range(n):
        o=sec+i*40
        name=data[o:o+8].rstrip(b'\\0').decode(errors='ignore')
        vs,va,rs,rp=struct.unpack_from('<IIII',data,o+8)
        out.append((name,va,vs,rp,rs))
    return out

def r2o(data,rva):
    for name,va,vs,rp,rs in sections(data):
        if va <= rva < va+max(vs,rs): return rp+(rva-va)
    raise ValueError(hex(rva))

def patch(data,rva,old,new,label):
    o=r2o(data,rva)
    got=bytes(data[o:o+len(old)])
    if got!=old: raise SystemExit(f'{label}: expected {old.hex()} got {got.hex()} at {hex(rva)}')
    data[o:o+len(new)]=new

def check(data,expected):
    got=sha(data)
    if got!=expected: raise SystemExit(f'output sha mismatch: {got} != {expected}')
    print(got)

BASE=Path('base_v153.addon64')
OUT=Path('v154.addon64')
EXPECTED_BASE='4641d2f9d66e879cc86c05aa403a6f9fcfe96c5f9486c15cdae4339f15d50d68'
EXPECTED_OUT='4ddf2b270b09efcc57c4cf680ac0da78ebc3f0a5f58fd7d242b87b90aa6168cc'
b=bytearray(BASE.read_bytes())
if sha(b)!=EXPECTED_BASE: raise SystemExit('base sha mismatch')
patch(b,0x1b71a8,bytes.fromhex('488b9688221100'),bytes.fromhex('e9150100009090'),'formatter threadstate bypass')
check(b,EXPECTED_OUT); OUT.write_bytes(b)