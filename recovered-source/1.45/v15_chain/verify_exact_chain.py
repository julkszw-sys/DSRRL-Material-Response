#!/usr/bin/env python3
from __future__ import annotations
import hashlib, subprocess, shutil, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
V151=ROOT/'v15_1'
CHAIN=Path(__file__).resolve().parent
EXPECTED={
 'v151_reconstructed.addon64':'7f10f908d598f8d6b952e2c78bce6eb97a8b84a297f956a069c439b7977b65cc',
 'v153.addon64':'4641d2f9d66e879cc86c05aa403a6f9fcfe96c5f9486c15cdae4339f15d50d68',
 'v154.addon64':'4ddf2b270b09efcc57c4cf680ac0da78ebc3f0a5f58fd7d242b87b90aa6168cc',
 'v155.addon64':'bf552c8606e27ab1bf3535b27509e91d8692fe553993a0e80b767cd30c1dd283',
 'v156.addon64':'9f112214a7cfc058775df26c58d4e8afaaa40fa22015613520bcaafcc9d5d21b',
}
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def run(script,cwd): subprocess.run([sys.executable,str(script)],cwd=str(cwd),check=True)
def chk(p):
    got=sha(p); exp=EXPECTED[p.name]
    if got!=exp: raise SystemExit(f'FAIL {p.name} {got} != {exp}')
    print('PASS',p.name,got)

def main():
    # Expected external inputs next to V15.1 builder:
    # base_v12.addon64 and PTDE_GI_ENVSPEC_PACK_RGBA.bin.
    run(V151/'build_v15_1_reconstructed.py',V151)
    chk(V151/'v151_reconstructed.addon64')
    shutil.copy2(V151/'v151_reconstructed.addon64',CHAIN/'base_v151.addon64')
    stages=[('build_v15_3_reconstructed.py','v153.addon64','base_v153.addon64'),('build_v15_4_reconstructed.py','v154.addon64','base_v154.addon64'),('build_v15_5_reconstructed.py','v155.addon64','base_v155.addon64'),('build_v15_6_reconstructed.py','v156.addon64',None)]
    for i,(script,out,nextbase) in enumerate(stages):
        run(CHAIN/script,CHAIN); p=CHAIN/out; chk(p)
        if nextbase: shutil.copy2(p,CHAIN/nextbase)
    print('PASS V15.1->V15.6 exact chain')
if __name__=='__main__': main()