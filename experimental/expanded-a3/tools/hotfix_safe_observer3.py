#!/usr/bin/env python3
from pathlib import Path
import argparse, struct, hashlib, json

BASE_SHA='81f0012dc967f2480d1cf42a4f3fec1acb6798fa2ae55864d7e367fd41475ad8'
CHAIN_CALL_RVA=0x1C1D8B
WRAPPER_RVA=0x1C1E00
EXE_BASE_GLOBAL_RVA=0x1076B8
SAFE_READ8_RVA=0x2A60
A3_OBSERVER_RVA=0x1BFC50
EXPECTED_CHAIN_CALL=bytes.fromhex('e8c0deffff')

def sha(b): return hashlib.sha256(b).hexdigest()
def rel32(src_next_rva,target_rva):
    d=target_rva-src_next_rva
    if not -(1<<31)<=d<(1<<31): raise ValueError('rel32 out of range')
    return struct.pack('<i',d)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--input',type=Path,required=True)
    ap.add_argument('--wrapper-bin',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--audit',type=Path,required=True)
    a=ap.parse_args()

    b=bytearray(a.input.read_bytes())
    if sha(b)!=BASE_SHA:
        raise SystemExit(f'base SHA mismatch: {sha(b)}')

    lf=struct.unpack_from('<I',b,0x3c)[0]
    n=struct.unpack_from('<H',b,lf+6)[0]
    osz=struct.unpack_from('<H',b,lf+20)[0]
    opt=lf+24
    sec=opt+osz
    secs=[]
    for i in range(n):
        o=sec+i*40
        name=bytes(b[o:o+8]).split(b'\0')[0].decode()
        vs,va,rs,rp=struct.unpack_from('<IIII',b,o+8)
        ch=struct.unpack_from('<I',b,o+36)[0]
        secs.append((name,va,vs,rs,rp,ch))

    def off(rva):
        for name,va,vs,rs,rp,ch in secs:
            if va<=rva<va+max(vs,rs):
                return rp+(rva-va)
        raise KeyError(hex(rva))

    def section(rva):
        for row in secs:
            if row[1]<=rva<row[1]+max(row[2],row[3]):
                return row
        raise KeyError(hex(rva))

    co=off(CHAIN_CALL_RVA)
    if bytes(b[co:co+5])!=EXPECTED_CHAIN_CALL:
        raise SystemExit('chain call preimage mismatch')

    raw=bytearray(a.wrapper_bin.read_bytes())
    if len(raw)!=0x84:
        raise SystemExit(f'unexpected wrapper size {len(raw)}')

    # wrapper placeholders:
    # 0x1b = RIP disp32 for shipping EXE-base global
    # 0x5e = rel32 for shipping safe-read8 helper
    # 0x7b = rel32 for existing A3 observer
    struct.pack_into('<i',raw,0x1b,EXE_BASE_GLOBAL_RVA-(WRAPPER_RVA+0x1f))
    struct.pack_into('<i',raw,0x5e,SAFE_READ8_RVA-(WRAPPER_RVA+0x62))
    struct.pack_into('<i',raw,0x7b,A3_OBSERVER_RVA-(WRAPPER_RVA+0x7f))

    wo=off(WRAPPER_RVA)
    pre=bytes(b[wo:wo+len(raw)])
    if any(x not in (0x00,0xCC) for x in pre):
        raise SystemExit('wrapper cave not pristine zero/INT3')

    name,va,vs,rs,rp,ch=section(WRAPPER_RVA)
    if not (ch & 0x20000000):
        raise SystemExit(f'wrapper section {name} is not executable')

    b[wo:wo+len(raw)]=raw
    new_call=b'\xE8'+rel32(CHAIN_CALL_RVA+5,WRAPPER_RVA)
    b[co:co+5]=new_call

    checksum_off=opt+64
    struct.pack_into('<I',b,checksum_off,0)
    total=0
    i=0
    while i+1<len(b):
        word=0 if checksum_off<=i<checksum_off+4 else b[i]|(b[i+1]<<8)
        total=(total+word)&0xffffffff
        total=(total&0xffff)+(total>>16)
        i+=2
    if i<len(b):
        total=(total+b[i])&0xffffffff
        total=(total&0xffff)+(total>>16)
    total=(total&0xffff)+(total>>16)
    total=total+(total>>16)
    checksum=(total&0xffff)+len(b)
    struct.pack_into('<I',b,checksum_off,checksum)

    a.output.write_bytes(b)
    audit={
      'schema':'dsrrl.a3.safe_observer_hotfix3.v1',
      'status':'CONSTRUCTION_PASS_DIAGNOSTIC',
      'base_sha256':BASE_SHA,
      'output_sha256':sha(b),
      'output_size':len(b),
      'pe_checksum':f'0x{checksum:08X}',
      'root_cause_status':'HIGH_CONFIDENCE',
      'mechanism':'Guard selected r14/r15 descriptor tuple with shipping 1.45 safe-read8 helper before entering A3 observer.',
      'patches':[
        {'rva':hex(CHAIN_CALL_RVA),'old':EXPECTED_CHAIN_CALL.hex(),'new':new_call.hex(),'target_rva':hex(WRAPPER_RVA)},
        {'rva':hex(WRAPPER_RVA),'size':len(raw),'new':raw.hex()}
      ],
      'invariants':['RAX Hotfix1 retained','single-selector Hotfix2 retained','four producer hooks unchanged','draw callsites unchanged','U/L payloads unchanged','EnvDiffuse OFF']
    }
    a.audit.write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps(audit,indent=2))

if __name__=='__main__':
    main()
