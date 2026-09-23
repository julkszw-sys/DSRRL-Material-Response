#!/usr/bin/env python3
from pathlib import Path
import hashlib
LABEL='Spec Bridge RC -> telemetry'
SOURCE_SHA='5e26eb50ff7c6632350c1e6c752e406c1e858cd06c6d847d44d719a3ec252abc'
TARGET_SHA='633d50b92e12e406acbca300220030034cfe7db08744737ceb76e224dccd3e09'
SOURCE_SIZE=1112576
TARGET_SIZE=1112576
WINDOWS=[
 (0x350,bytes.fromhex('c012'),bytes.fromhex('4413')),
 (0x10eb8e,bytes.fromhex('4889f1e88d050000'),bytes.fromhex('e92d0d0000909090')),
 (0x10f8c0,bytes.fromhex('0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'),bytes.fromhex('4839da0f8443000000803d30e0ffff000f8536000000c60523e0ffff01524883ec28488b0d1fddffff488b0520ddffff4885c00f840e000000ba030000004c8d0514000000ffd04883c4285a4889f1e80ff8ffffe97df2ffff5b445352524c5d5b505444455f535045435d205431305f505444455f42494e4420636f6e6669726d6564')),
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
