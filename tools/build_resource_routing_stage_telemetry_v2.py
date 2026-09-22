#!/usr/bin/env python3
"""Expose preserved V8/V12 resource-routing stage latches over Recovery V1."""
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path

EXPECTED_BASIS_SHA256="45c7023cb6d2c1680712ca32dae6af25c3c406e7ce233d2555a441be67653928"
EXPECTED_OUTPUT_SHA256="e15747f4920bd5f40db9019e4e45110ad4655b4defab1ad3027b039fc022d98b"

LOADS=[
(0x705F,0x112F80),(0x707F,0x112F84),(0x709F,0x112F88),
(0x70BF,0x112F8C),(0x70DF,0x112F90),(0x70FF,0x112F94),
(0x711F,0x112F98),(0x713F,0x112F9C),(0x715F,0x112FA0),
]
LABELS={
0x101218:" CS=",0x101200:" CN=",0x1011E8:" CD=",
0x1011D8:" DG=",0x1011C8:" DS=",0x1011B0:" DIF=",
0x1011A0:" NG=",0x101188:" NS=",0x101170:" NRM=",
}
LABEL_CAP={0x101218:8,0x101200:24,0x1011E8:24,0x1011D8:16,0x1011C8:16,
0x1011B0:24,0x1011A0:16,0x101188:24,0x101170:24}

def sha(b): return hashlib.sha256(b).hexdigest()
def secs(b):
 p=struct.unpack_from("<I",b,0x3c)[0]; n=struct.unpack_from("<H",b,p+6)[0]
 o=struct.unpack_from("<H",b,p+20)[0]; s=p+24+o
 out=[]
 for i in range(n):
  q=s+i*40; vs,va,rs,rp=struct.unpack_from("<IIII",b,q+8); out.append((vs,va,rs,rp))
 return out
def off(b,r):
 for vs,va,rs,rp in secs(b):
  if va<=r<va+max(vs,rs): return rp+r-va
 raise ValueError(hex(r))
def main():
 ap=argparse.ArgumentParser(); ap.add_argument("basis",type=Path); ap.add_argument("output",type=Path); a=ap.parse_args()
 b=bytearray(a.basis.read_bytes())
 if sha(b)!=EXPECTED_BASIS_SHA256: raise SystemExit("unknown basis")
 for r,t in LOADS:
  q=off(b,r); d=t-(r+6); b[q:q+8]=b"\x8b\x15"+struct.pack("<i",d)+b"\x90\x90"
 for r,s in LABELS.items():
  q=off(b,r); raw=s.encode()+b"\0"; c=LABEL_CAP[r]
  b[q:q+c]=raw+b"\0"*(c-len(raw))
 if sha(b)!=EXPECTED_OUTPUT_SHA256: raise SystemExit("unexpected output SHA")
 a.output.write_bytes(b); print("PASS",sha(b))
if __name__=="__main__": main()
