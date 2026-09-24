#!/usr/bin/env python3
"""Minimal crash hotfix for A3 build 75d90c5e.

Root cause: the A3 PRE stub called a3_capture_pre_ps after the shipping 1.45
legacy PRE helper, but failed to preserve the live RAX return value. Shipping
1.45 consumes that RAX immediately after the callsite.

This tool requires the exact crashing A3 preimage and changes only:
  * PRE callsite 0x65E7 -> a reserved executable code cave at 0x1C1D00
  * 32 bytes of INT3 padding at 0x1C1D00 -> fixed ABI-preserving PRE stub
  * PE checksum
"""
from pathlib import Path
import argparse, hashlib, json, struct

BASE_SHA="75d90c5ed02511f66a37e9265b5be72b94ef4a5e03dbfe939b5fe392700c40f0"
CALLSITE=0x65E7
CAVE=0x1C1D00
LEGACY_PRE=0x1C0BF0
CAPTURE_PRE=0x1BFFA0
EXPECTED_CALL=bytes.fromhex("e856b51b00")

def sha256(b): return hashlib.sha256(b).hexdigest()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--input",type=Path,required=True)
    ap.add_argument("--output",type=Path,required=True)
    ap.add_argument("--audit",type=Path,required=True)
    a=ap.parse_args()
    b=bytearray(a.input.read_bytes())
    if sha256(b)!=BASE_SHA: raise SystemExit("wrong A3 preimage")

    lf=struct.unpack_from("<I",b,0x3c)[0]
    ns=struct.unpack_from("<H",b,lf+6)[0]
    osz=struct.unpack_from("<H",b,lf+20)[0]
    opt=lf+24; sec=opt+osz
    sections=[]
    for i in range(ns):
        o=sec+i*40
        name=bytes(b[o:o+8]).split(b"\0")[0].decode()
        vs,va,rs,rp=struct.unpack_from("<IIII",b,o+8)
        sections.append((name,va,vs,rs,rp))
    def rvaoff(rva):
        for _,va,vs,rs,rp in sections:
            if va<=rva<va+max(vs,rs): return rp+(rva-va)
        raise SystemExit(f"unmapped RVA 0x{rva:X}")
    def relcall(src,target):
        return b"\xE8"+struct.pack("<i",target-(src+5))

    co=rvaoff(CALLSITE)
    if bytes(b[co:co+5])!=EXPECTED_CALL: raise SystemExit("PRE callsite mismatch")
    cave=rvaoff(CAVE)
    if any(x!=0xCC for x in b[cave:cave+32]): raise SystemExit("code cave is not pristine INT3 padding")

    stub=bytearray.fromhex("4883ec28")
    stub+=relcall(CAVE+len(stub),LEGACY_PRE)
    stub+=bytes.fromhex("48894424204889d9")
    stub+=relcall(CAVE+len(stub),CAPTURE_PRE)
    stub+=bytes.fromhex("488b4424204883c428c3")
    if len(stub)!=32: raise SystemExit("stub width changed")

    b[cave:cave+32]=stub
    new_call=relcall(CALLSITE,CAVE)
    b[co:co+5]=new_call

    checksum_off=opt+64
    struct.pack_into("<I",b,checksum_off,0)
    total=0;i=0
    while i+1<len(b):
        word=0 if checksum_off<=i<checksum_off+4 else b[i]|(b[i+1]<<8)
        total=(total+word)&0xffffffff
        total=(total&0xffff)+(total>>16);i+=2
    if i<len(b):
        total=(total+b[i])&0xffffffff;total=(total&0xffff)+(total>>16)
    total=(total&0xffff)+(total>>16);total=total+(total>>16)
    checksum=(total&0xffff)+len(b)
    struct.pack_into("<I",b,checksum_off,checksum)

    a.output.write_bytes(b)
    audit={
      "schema":"dsrrl.a3.rax_hotfix1.v1","status":"CONSTRUCTION_PASS_DIAGNOSTIC",
      "base_sha256":BASE_SHA,"output_sha256":sha256(b),"output_size":len(b),
      "pe_checksum":f"0x{checksum:08X}",
      "root_cause":"legacy PRE returns live RAX consumed by shipping 1.45; A3 capture call clobbered it",
      "patches":[
        {"rva":hex(CALLSITE),"old":EXPECTED_CALL.hex(),"new":new_call.hex(),"target_rva":hex(CAVE)},
        {"rva":hex(CAVE),"old":"cc"*32,"new":stub.hex(),"size":32}
      ]
    }
    a.audit.write_text(json.dumps(audit,indent=2)+"\n")
    print(json.dumps(audit,indent=2))

if __name__=="__main__": main()
