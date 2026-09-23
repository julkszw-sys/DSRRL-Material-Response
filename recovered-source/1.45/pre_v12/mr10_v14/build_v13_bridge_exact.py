#!/usr/bin/env python3
from pathlib import Path
import argparse, subprocess, hashlib, struct, tempfile, shutil
SOURCE_SHA='8ef94ff97124ff7ab5e1626cce714b9d5c1dca8423d9c1ae6ade2983c5da6940'
# Verify/update source hash below from owner archival file before build.
TARGET_SHA='1992ab7c5fee340b1f5ea07cf13b60811afde50b0257f34b3f6dff1c87e4b365'
TARGET_NAME='ZZ_DSRRL_PTDE_EnvSpec_Source_Bridge_V13.addon64'
HISTORICAL_COFF_TIMESTAMP=0x6aac589f

def sha(b): return hashlib.sha256(b).hexdigest()
def run(cmd):
 p=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
 if p.returncode: raise RuntimeError('command failed: '+' '.join(cmd)+'\n'+p.stdout+'\n'+p.stderr)
 return p

def build(source:Path,output:Path):
 sb=source.read_bytes(); actual=sha(sb)
 if actual!=SOURCE_SHA: raise ValueError(f'source SHA mismatch {actual} != {SOURCE_SHA}')
 clang=shutil.which('clang++'); lld=shutil.which('lld-link')
 if not clang or not lld: raise RuntimeError('clang++ and lld-link required')
 with tempfile.TemporaryDirectory() as td:
  td=Path(td); obj=td/'v13.obj'; dll=td/TARGET_NAME
  run([clang,'-target','x86_64-pc-windows-msvc','-c',str(source),'-o',str(obj),'-O2','-fno-exceptions','-fno-rtti','-fno-stack-protector','-fno-asynchronous-unwind-tables','-fno-unwind-tables','-fno-builtin'])
  run([lld,'/dll','/noentry','/nodefaultlib','/subsystem:windows','/opt:ref','/opt:icf','/out:'+str(dll),str(obj)])
  b=bytearray(dll.read_bytes()); pe=struct.unpack_from('<I',b,0x3c)[0]
  struct.pack_into('<I',b,pe+8,HISTORICAL_COFF_TIMESTAMP)
  out=bytes(b)
 if sha(out)!=TARGET_SHA: raise ValueError(f'output SHA mismatch {sha(out)} != {TARGET_SHA}')
 output.write_bytes(out); return out
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('source');ap.add_argument('output');ns=ap.parse_args()
 out=build(Path(ns.source),Path(ns.output));print('V13 bridge exact source build PASS',sha(out))
