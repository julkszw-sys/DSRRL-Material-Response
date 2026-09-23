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

BASE=Path('base_v151.addon64')
OUT=Path('v153.addon64')
EXPECTED_BASE='7f10f908d598f8d6b952e2c78bce6eb97a8b84a297f956a069c439b7977b65cc'
EXPECTED_OUT='4641d2f9d66e879cc86c05aa403a6f9fcfe96c5f9486c15cdae4339f15d50d68'
b=bytearray(BASE.read_bytes())
if sha(b)!=EXPECTED_BASE: raise SystemExit('base sha mismatch')
# V15.1 active wrapper calls -> stock V12 calls. Opcode E8 is unchanged; patch rel32 only.
patch(b,0x19c070,bytes.fromhex('4cb40100'),bytes.fromhex('9cf3ffff'),'PRE rel32')
patch(b,0x19c085,bytes.fromhex('87cd0100'),bytes.fromhex('07ffffff'),'POST rel32')
patch(b,0x19c0f8,bytes.fromhex('f4b20100'),bytes.fromhex('74eaffff'),'PREPARE rel32')
# Exact historical diagnostic banner delta.
old=b'active exact-slot bind diagnostic'
new=b'V15.3 state-layout pixel-inert\x00\x00\x00'
if len(old)!=len(new): raise SystemExit((len(old),len(new)))
pos=bytes(b).find(old)
if pos<0: raise SystemExit('banner not found')
b[pos:pos+len(old)]=new
check(b,EXPECTED_OUT); OUT.write_bytes(b)