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

BASE=Path('base_v155.addon64')
OUT=Path('v156.addon64')
EXPECTED_BASE='bf552c8606e27ab1bf3535b27509e91d8692fe553993a0e80b767cd30c1dd283'
EXPECTED_OUT='9f112214a7cfc058775df26c58d4e8afaaa40fa22015613520bcaafcc9d5d21b'
b=bytearray(BASE.read_bytes())
if sha(b)!=EXPECTED_BASE: raise SystemExit('base sha mismatch')
patch(b,0x1b71a8,bytes.fromhex('e9150100009090'),bytes.fromhex('488b9688221100'),'restore per-thread A/B semantic store')
patches=[
(0x1baea2,0x112308,0x112268),(0x1baead,0x112309,0x112269),(0x1baeb9,0x11230a,0x11226a),
(0x1baec5,0x11230b,0x11226b),(0x1baed1,0x11230c,0x11226c),(0x1baedd,0x11230d,0x11226d),
(0x1baee9,0x11230e,0x11226e),(0x1baef5,0x11230f,0x11226f),(0x1baf01,0x112310,0x112270),
(0x1baf0d,0x112311,0x112271),(0x1baf19,0x112312,0x112272),(0x1baf25,0x112313,0x112273),
(0x1baf31,0x112314,0x112274),(0x1baf3d,0x112315,0x112275),(0x1baf49,0x112316,0x112276),
]
for rva,old,new in patches:
    patch(b,rva,struct.pack('<I',old),struct.pack('<I',new),f'uninit BSS relocate {hex(rva)}')
check(b,EXPECTED_OUT); OUT.write_bytes(b)