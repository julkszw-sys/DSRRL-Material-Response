#!/usr/bin/env python3
from pathlib import Path
import hashlib, struct, importlib.util
BASE_SHA='bb23eefb0d4a1c48330c6001d1c3108c1a0610a798e31a1caf4b37c7f0d325e4'
OUT_SHA='6711b90ddd3c92fcf5450eed55f8b0b447bdf51fe9ef5a1784adf61886fed6d4'
TARGETS=[
 (0xbbb40,19872,1315,1320,'ab3fa2075958f9302b7ac2249cf25fb28063f60d6d0628bc17f0b6769dd8466c','06a016524da0e3872e609baff91c0d04be865c1788cd16028f20d20757d7ee5f'),
 (0xc08e0,19572,1224,1229,'b38c8a7311915018a31c11eb2f52f9401f9ba93a099b3d06700fda5945116cc5','4cd09acb1de6010db002d8297d616fba70b771bb3a3e751082669c20e6b5b6bd'),
 (0xc5560,18076,888,893,'c1eb9f7e6c765080081c330e6d4c3ffe857bce2725867073309086e1a5e65f7a','0fd2c1fc914cba12b7fed654c722613d6e0f56f64103446b7494412c14f0d852'),
]
def sha(b): return hashlib.sha256(b).hexdigest()
def checksum_module():
 p=Path('/mnt/data/dxbc_checksum.py'); spec=importlib.util.spec_from_file_location('dxbc_checksum',p); m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m
C=checksum_module()
def patch_receiver(d, dest_word, resource_word):
 if d[:4]!=b'DXBC': raise ValueError('not DXBC')
 b=bytearray(d)
 n=struct.unpack_from('<I',b,28)[0]; offs=struct.unpack_from('<%dI'%n,b,32)
 shex=None
 for co in offs:
  if b[co:co+4] in (b'SHEX',b'SHDR'): shex=co+8; break
 if shex is None: raise ValueError('no SHEX/SHDR')
 # Historical semantic patch: second V14 sample dest XYZ -> YZW and resource t10 -> t1.
 o1=shex+dest_word*4; o2=shex+resource_word*4
 if struct.unpack_from('<I',b,o1)[0] != 0x00100072: raise ValueError(f'dest guard {struct.unpack_from("<I",b,o1)[0]:08x}')
 if struct.unpack_from('<I',b,o2)[0] != 10: raise ValueError(f'resource guard {struct.unpack_from("<I",b,o2)[0]}')
 struct.pack_into('<I',b,o1,0x001000e2)
 struct.pack_into('<I',b,o2,1)
 b[4:20]=b'\0'*16; b[4:20]=C.dxbc_checksum(bytes(b))
 return bytes(b)
def rebuild(source: bytes)->bytes:
 if sha(source)!=BASE_SHA: raise ValueError(f'base SHA {sha(source)}')
 out=bytearray(source)
 for off,size,dw,rw,oldsha,newsha in TARGETS:
  old=bytes(out[off:off+size])
  if sha(old)!=oldsha: raise ValueError(f'receiver old SHA at {off:#x}: {sha(old)}')
  new=patch_receiver(old,dw,rw)
  if sha(new)!=newsha: raise ValueError(f'receiver new SHA at {off:#x}: {sha(new)}')
  out[off:off+size]=new
 result=bytes(out)
 if sha(result)!=OUT_SHA: raise ValueError(f'output SHA {sha(result)}')
 return result
if __name__=='__main__':
 import argparse
 ap=argparse.ArgumentParser(); ap.add_argument('source'); ap.add_argument('output'); ns=ap.parse_args()
 out=rebuild(Path(ns.source).read_bytes()); Path(ns.output).write_bytes(out)
 print('MR1.2R M10 fail-open reconstructed: EXACT PASS',sha(out))
