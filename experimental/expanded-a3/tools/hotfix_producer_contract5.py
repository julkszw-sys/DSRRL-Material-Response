#!/usr/bin/env python3
from pathlib import Path
import argparse,struct,hashlib,json
BASE_SHA='a17031743c48dfa27c8dabd07673320ba4b22b61c3d55287ad0f436db406327b'
SINGLE_INSTALL_RVA=0x1C0EEE
SINGLE_SKIP_TARGET_RVA=0x1C0F19
EXPECTED_SINGLE_INSTALL=bytes.fromhex('488d050be5ffff')
PUBLISH_READ_RVA=0x1BF297
PUBLISH_RESUME_RVA=0x1BF29F
PUBLISH_FAILOPEN_RVA=0x1BF380
PUBLISH_TRAMP_RVA=0x1C1F40
SAFE_READ8_RVA=0x2A60
EXPECTED_PUBLISH_READ=bytes.fromhex('f30f1041088b490c')

def sha(b): return hashlib.sha256(b).hexdigest()
def rel32(src_next,target):
    d=target-src_next
    if not -(1<<31)<=d<(1<<31): raise ValueError('rel32')
    return struct.pack('<i',d)

def pe(b):
    lf=struct.unpack_from('<I',b,0x3c)[0]; n=struct.unpack_from('<H',b,lf+6)[0]
    osz=struct.unpack_from('<H',b,lf+20)[0]; opt=lf+24; sec=opt+osz
    rows=[]
    for i in range(n):
        o=sec+i*40
        name=bytes(b[o:o+8]).split(b'\0')[0].decode(errors='replace')
        vs,va,rs,rp=struct.unpack_from('<IIII',b,o+8); ch=struct.unpack_from('<I',b,o+36)[0]
        rows.append((name,va,vs,rs,rp,ch))
    return opt,rows

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--input',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--audit',type=Path,required=True)
    a=ap.parse_args()
    b=bytearray(a.input.read_bytes())
    if sha(b)!=BASE_SHA: raise SystemExit(f'base SHA mismatch: {sha(b)}')
    opt,secs=pe(b)
    def off(rva):
        for name,va,vs,rs,rp,ch in secs:
            if va<=rva<va+max(vs,rs): return rp+rva-va
        raise KeyError(hex(rva))
    def section(rva):
        for row in secs:
            if row[1]<=rva<row[1]+max(row[2],row[3]): return row
        raise KeyError(hex(rva))

    so=off(SINGLE_INSTALL_RVA)
    if bytes(b[so:so+7])!=EXPECTED_SINGLE_INSTALL: raise SystemExit('single-install preimage mismatch')
    single_patch=b'\xE9'+rel32(SINGLE_INSTALL_RVA+5,SINGLE_SKIP_TARGET_RVA)+b'\x90\x90'
    b[so:so+7]=single_patch

    po=off(PUBLISH_READ_RVA)
    if bytes(b[po:po+8])!=EXPECTED_PUBLISH_READ: raise SystemExit('publish read preimage mismatch')
    tr=bytearray()
    tr += bytes.fromhex('4883ec28')
    tr += bytes.fromhex('4883c108')
    tr += bytes.fromhex('488d542420')
    call_pos=PUBLISH_TRAMP_RVA+len(tr)
    tr += b'\xE8'+rel32(call_pos+5,SAFE_READ8_RVA)
    tr += bytes.fromhex('84c0')
    je_off=len(tr); tr += bytes.fromhex('0f8400000000')
    tr += bytes.fromhex('8b442420')
    tr += bytes.fromhex('660f6ec0')
    tr += bytes.fromhex('8b4c2424')
    tr += bytes.fromhex('4883c428')
    j_ok_off=len(tr); tr += b'\xE9\x00\x00\x00\x00'
    fail_local=len(tr)
    tr += bytes.fromhex('4883c428')
    j_fail_off=len(tr); tr += b'\xE9\x00\x00\x00\x00'
    struct.pack_into('<i',tr,je_off+2,(PUBLISH_TRAMP_RVA+fail_local)-(PUBLISH_TRAMP_RVA+je_off+6))
    struct.pack_into('<i',tr,j_ok_off+1,PUBLISH_RESUME_RVA-(PUBLISH_TRAMP_RVA+j_ok_off+5))
    struct.pack_into('<i',tr,j_fail_off+1,PUBLISH_FAILOPEN_RVA-(PUBLISH_TRAMP_RVA+j_fail_off+5))

    to=off(PUBLISH_TRAMP_RVA)
    pre=bytes(b[to:to+len(tr)])
    if any(x not in (0,0xcc) for x in pre): raise SystemExit('producer safe-read cave not pristine')
    if not (section(PUBLISH_TRAMP_RVA)[5] & 0x20000000): raise SystemExit('producer safe-read cave not executable')
    b[to:to+len(tr)]=tr
    pub_patch=b'\xE9'+rel32(PUBLISH_READ_RVA+5,PUBLISH_TRAMP_RVA)+b'\x90\x90\x90'
    b[po:po+8]=pub_patch

    checksum_off=opt+64; struct.pack_into('<I',b,checksum_off,0); total=0; i=0
    while i+1<len(b):
        word=0 if checksum_off<=i<checksum_off+4 else b[i]|(b[i+1]<<8)
        total=(total+word)&0xffffffff; total=(total&0xffff)+(total>>16); i+=2
    if i<len(b):
        total=(total+b[i])&0xffffffff; total=(total&0xffff)+(total>>16)
    total=(total&0xffff)+(total>>16); total=total+(total>>16)
    cs=(total&0xffff)+len(b); struct.pack_into('<I',b,checksum_off,cs)
    a.output.write_bytes(b); outsha=sha(b)
    audit={
      'schema':'dsrrl.a3.producer_contract_hotfix5.v1','status':'CONSTRUCTION_PASS_DIAGNOSTIC',
      'base_sha256':BASE_SHA,'output_sha256':outsha,'output_size':len(b),'pe_checksum':f'0x{cs:08X}',
      'mechanism':'Remove obsolete A3 hook of EXE 0x564510 (cache-builder SINGLE, not steady evaluator) and restore guarded post-wrapper assignment tuple read through shipping 1.45 safe-read8. True blend hook 0x5642F0 remains. Steady U/L intentionally fails open until canonical existing single-packer 0x563B80 integration is materialized.',
      'patches':[
        {'rva':hex(SINGLE_INSTALL_RVA),'old':EXPECTED_SINGLE_INSTALL.hex(),'new':single_patch.hex(),'purpose':'skip obsolete 0x564510 hook install'},
        {'rva':hex(PUBLISH_READ_RVA),'old':EXPECTED_PUBLISH_READ.hex(),'new':pub_patch.hex(),'target_rva':hex(PUBLISH_TRAMP_RVA),'purpose':'guard assignment A/B/beta tuple read'},
        {'rva':hex(PUBLISH_TRAMP_RVA),'size':len(tr),'new':tr.hex(),'safe_read8_rva':hex(SAFE_READ8_RVA),'resume_rva':hex(PUBLISH_RESUME_RVA),'fail_open_rva':hex(PUBLISH_FAILOPEN_RVA)}
      ],
      'canonical_basis':{'key':'project.branch.renderer_edition_ul_real_producer_capture_contract_v1','revision_seq':4401,'steady_single_packer':'0x140563B80','obsolete_cache_builder_helper':'0x140564510','blend_helper':'0x1405642F0'},
      'invariants':['HF4 selector safety retained','shipping 1.45 selector resolver retained','RAX Hotfix1 retained','wrapper5/wrapper6 capture retained','true blend capture retained','draw callsites unchanged','EnvDiffuse OFF','steady path fail-open rather than guessed'],
      'runtime_status':'NOT_TESTED','bridge_activation':'PARTIAL_BY_CONSTRUCTION','pixel_status':'OPEN'
    }
    a.audit.write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps(audit,indent=2))

if __name__=='__main__': main()
