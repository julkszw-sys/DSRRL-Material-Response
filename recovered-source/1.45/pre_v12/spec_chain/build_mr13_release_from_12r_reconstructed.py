#!/usr/bin/env python3
from pathlib import Path
import hashlib
LABEL='MR1.2R -> late MR1.3 release'
SOURCE_SHA='6711b90ddd3c92fcf5450eed55f8b0b447bdf51fe9ef5a1784adf61886fed6d4'
TARGET_SHA='d29173139f0ec14b2718f8c22774053fb9aeadd400088f72b3693f3156c890b2'
SOURCE_SIZE=1107456
TARGET_SIZE=1107456
WINDOWS=[
 (0xdc72,bytes.fromhex('31'),bytes.fromhex('33')),
 (0xdcba,bytes.fromhex('31'),bytes.fromhex('33')),
 (0xbbb44,bytes.fromhex('9dd7fafd31bdca0c2f3942fafa431d09'),bytes.fromhex('3a441983ad483663021a8d2bda2009c4')),
 (0xbbd4c,bytes.fromhex('0a'),bytes.fromhex('07')),
 (0xbdce0,bytes.fromhex('0a'),bytes.fromhex('07')),
 (0xc08e4,bytes.fromhex('d530082aa233a68e96ac8e289836c7bd'),bytes.fromhex('83c88b54558612190bb546c8e44e1046')),
 (0xc0aec,bytes.fromhex('0a'),bytes.fromhex('07')),
 (0xc2a98,bytes.fromhex('0a'),bytes.fromhex('07')),
 (0x10040a,bytes.fromhex('30'),bytes.fromhex('33')),
 (0x10044a,bytes.fromhex('30'),bytes.fromhex('33')),
 (0x10048a,bytes.fromhex('30'),bytes.fromhex('33')),
 (0x1004f2,bytes.fromhex('30'),bytes.fromhex('33')),
 (0x10065a,bytes.fromhex('30'),bytes.fromhex('33')),
 (0x10068a,bytes.fromhex('30'),bytes.fromhex('33')),
 (0x1006da,bytes.fromhex('30'),bytes.fromhex('33')),
 (0x103894,bytes.fromhex('30'),bytes.fromhex('33')),
]
APPEND=bytes.fromhex('')
def sha(x): return hashlib.sha256(x).hexdigest()
def rebuild(source:bytes)->bytes:
 if sha(source)!=SOURCE_SHA or len(source)!=SOURCE_SIZE: raise ValueError(f'source mismatch {len(source)} {sha(source)}')
 x=bytearray(source)
 for off,old,new in WINDOWS:
  got=bytes(x[off:off+len(old)])
  if got!=old: raise ValueError(f'guard mismatch at {off:#x}')
  x[off:off+len(new)]=new
 if TARGET_SIZE>SOURCE_SIZE: x+=APPEND
 elif TARGET_SIZE<SOURCE_SIZE: del x[TARGET_SIZE:]
 out=bytes(x)
 if len(out)!=TARGET_SIZE or sha(out)!=TARGET_SHA: raise ValueError(f'target mismatch {len(out)} {sha(out)}')
 return out
if __name__=='__main__':
 import argparse
 p=argparse.ArgumentParser();p.add_argument('source');p.add_argument('output');n=p.parse_args()
 out=rebuild(Path(n.source).read_bytes());Path(n.output).write_bytes(out)
 print(f'{LABEL}: EXACT PASS {sha(out)} windows={len(WINDOWS)} append={len(APPEND)}')
