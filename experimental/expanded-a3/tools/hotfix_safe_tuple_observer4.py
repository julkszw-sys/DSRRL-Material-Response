#!/usr/bin/env python3
from pathlib import Path
import argparse, struct, hashlib, json

BASE_SHA='052dc7d277d67a006e6df57c5da86d94438858479056a7418190d9bff8b07b0b'
EXPECTED_OUTPUT_SHA='a17031743c48dfa27c8dabd07673320ba4b22b61c3d55287ad0f436db406327b'
OBSERVER_TUPLE_RVA=0x1BFD7F
OBSERVER_RESUME_RVA=0x1BFD8B
OBSERVER_FAILOPEN_RVA=0x1BFE18
TRAMPOLINE_RVA=0x1C1F00
SAFE_READ8_RVA=0x2A60
EXPECTED_RAW_TUPLE=bytes.fromhex('440fb7474c0fb7574e8b4f50')
TRAMPOLINE_TEMPLATE=bytes.fromhex(
    '4989c6'
    '488d4f4c'
    '488d542420'
    'e800000000'
    '84c0'
    '7422'
    '4c89f0'
    '4c8b542420'
    '450fb7c2'
    '4c89d2'
    '48c1ea10'
    '0fb7d2'
    '49c1ea20'
    '4489d1'
    'e900000000'
    '4c89f0'
    'e900000000'
)

def sha(b): return hashlib.sha256(b).hexdigest()

def rel32(src_next_rva,target_rva):
    d=target_rva-src_next_rva
    if not -(1<<31)<=d<(1<<31): raise ValueError('rel32 out of range')
    return struct.pack('<i',d)

def pe_layout(b):
    lf=struct.unpack_from('<I',b,0x3c)[0]
    n=struct.unpack_from('<H',b,lf+6)[0]
    osz=struct.unpack_from('<H',b,lf+20)[0]
    opt=lf+24
    sec=opt+osz
    rows=[]
    for i in range(n):
        o=sec+i*40
        name=bytes(b[o:o+8]).split(b'\0')[0].decode(errors='replace')
        vs,va,rs,rp=struct.unpack_from('<IIII',b,o+8)
        ch=struct.unpack_from('<I',b,o+36)[0]
        rows.append((name,va,vs,rs,rp,ch))
    return opt,rows

def main():
    ap=argparse.ArgumentParser(description='HF4: make the existing A3 selector observer perform its tuple read through shipping 1.45 safe-read8.')
    ap.add_argument('--input',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--audit',type=Path,required=True)
    a=ap.parse_args()

    b=bytearray(a.input.read_bytes())
    if sha(b)!=BASE_SHA:
        raise SystemExit(f'base SHA mismatch: {sha(b)}')

    opt,secs=pe_layout(b)
    def off(rva):
        for name,va,vs,rs,rp,ch in secs:
            if va<=rva<va+max(vs,rs): return rp+(rva-va)
        raise KeyError(hex(rva))
    def section(rva):
        for row in secs:
            if row[1]<=rva<row[1]+max(row[2],row[3]): return row
        raise KeyError(hex(rva))

    po=off(OBSERVER_TUPLE_RVA)
    if bytes(b[po:po+len(EXPECTED_RAW_TUPLE)])!=EXPECTED_RAW_TUPLE:
        raise SystemExit('observer tuple-read preimage mismatch')

    tr=bytearray(TRAMPOLINE_TEMPLATE)
    struct.pack_into('<i',tr,0x0D,SAFE_READ8_RVA-(TRAMPOLINE_RVA+0x11))
    struct.pack_into('<i',tr,0x33,OBSERVER_RESUME_RVA-(TRAMPOLINE_RVA+0x37))
    struct.pack_into('<i',tr,0x3B,OBSERVER_FAILOPEN_RVA-(TRAMPOLINE_RVA+0x3F))

    co=off(TRAMPOLINE_RVA)
    pre=bytes(b[co:co+len(tr)])
    if any(x not in (0x00,0xCC) for x in pre):
        raise SystemExit('HF4 trampoline cave is not pristine zero/INT3')
    name,va,vs,rs,rp,ch=section(TRAMPOLINE_RVA)
    if not (ch & 0x20000000):
        raise SystemExit(f'HF4 trampoline section {name} is not executable')

    b[co:co+len(tr)]=tr
    entry=b'\xE9'+rel32(OBSERVER_TUPLE_RVA+5,TRAMPOLINE_RVA)+b'\x90'*7
    b[po:po+len(EXPECTED_RAW_TUPLE)]=entry

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

    out_sha=sha(b)
    if out_sha!=EXPECTED_OUTPUT_SHA:
        raise SystemExit(f'output SHA mismatch: {out_sha}')
    a.output.write_bytes(b)

    audit={
      'schema':'dsrrl.a3.safe_tuple_hotfix4.v1',
      'status':'CONSTRUCTION_PASS_DIAGNOSTIC',
      'base_sha256':BASE_SHA,
      'output_sha256':out_sha,
      'output_size':len(b),
      'pe_checksum':f'0x{checksum:08X}',
      'mechanism':'Replace the binary A3 selector observer raw descriptor tuple loads with a shipping-safe-read8 trampoline. Success decodes the same A/B/beta tuple and resumes at 0x1BFD8B; read failure restores ThreadState* and branches to the observer fail-open path at 0x1BFE18.',
      'patches':[
        {'rva':hex(OBSERVER_TUPLE_RVA),'old':EXPECTED_RAW_TUPLE.hex(),'new':entry.hex(),'target_rva':hex(TRAMPOLINE_RVA)},
        {'rva':hex(TRAMPOLINE_RVA),'size':len(tr),'new':tr.hex(),'safe_read8_rva':hex(SAFE_READ8_RVA),'resume_rva':hex(OBSERVER_RESUME_RVA),'fail_open_rva':hex(OBSERVER_FAILOPEN_RVA)}
      ],
      'invariants':['HF3 selector-chain wrapper retained','shipping 1.45 legacy selector resolver unchanged','RAX Hotfix1 retained','four A3 producer hooks unchanged','draw callsites unchanged','U/L payloads unchanged','EnvDiffuse bridge OFF']
    }
    a.audit.write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps(audit,indent=2))

if __name__=='__main__': main()
