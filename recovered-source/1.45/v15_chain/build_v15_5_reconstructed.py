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

BASE=Path('base_v154.addon64')
OUT=Path('v155.addon64')
EXPECTED_BASE='4ddf2b270b09efcc57c4cf680ac0da78ebc3f0a5f58fd7d242b87b90aa6168cc'
EXPECTED_OUT='bf552c8606e27ab1bf3535b27509e91d8692fe553993a0e80b767cd30c1dd283'
b=bytearray(BASE.read_bytes())
if sha(b)!=EXPECTED_BASE: raise SystemExit('base sha mismatch')
patches=[
(0x1b88f6,0x112300,0x112260),(0x1b8a42,0x112300,0x112260),
(0x1b8990,0x112304,0x112264),(0x1b89ac,0x112304,0x112264),
(0x1b8fc3,0x112308,0x112268),(0x1b8fd4,0x112309,0x112269),(0x1b8fe6,0x11230a,0x11226a),
(0x1b8ff8,0x11230b,0x11226b),(0x1b900a,0x11230c,0x11226c),(0x1b901c,0x11230d,0x11226d),
(0x1b902e,0x11230e,0x11226e),(0x1b9040,0x11230f,0x11226f),(0x1b9052,0x112310,0x112270),
(0x1b9064,0x112311,0x112271),(0x1b9076,0x112312,0x112272),(0x1b9088,0x112313,0x112273),
(0x1b909a,0x112314,0x112274),(0x1b90ac,0x112315,0x112275),(0x1b90be,0x112316,0x112276),
]
for rva,old,new in patches:
    patch(b,rva,struct.pack('<I',old),struct.pack('<I',new),f'BSS relocate {hex(rva)}')
check(b,EXPECTED_OUT); OUT.write_bytes(b)