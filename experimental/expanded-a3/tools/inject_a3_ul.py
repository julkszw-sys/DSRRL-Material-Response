#!/usr/bin/env python3
from __future__ import annotations
from pathlib import Path
import argparse, hashlib, json, struct

HOST_SHA='e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342'
HOST_SIZE=1803264
HOST_IMAGE_BASE=0x180000000
EXT_EXPECTED_IMAGE_BASE=0x1801BE000

PATCHES={
 'addon_init': (0x8C80, bytes.fromhex('e9cb011b00'), 'a3_init_wrapper', 'jmp5'),
 'addon_uninit': (0x9260, bytes.fromhex('e98bff1a00'), 'a3_uninit_wrapper', 'jmp5'),
 'gate': (0x632E, bytes.fromhex('e8ae5d1900'), 'a3_gate_stub', 'call5'),
 'pre': (0x65E7, bytes.fromhex('e87c5a1900'), 'a3_pre_stub', 'call5'),
 'draw_indexed': (0x6641, bytes.fromhex('ff15017f0000'), 'a3_draw_indexed_stub', 'call6'),
 'draw_instanced': (0x6664, bytes.fromhex('ff15de7e0000'), 'a3_draw_instanced_stub', 'call6'),
}


def sha256(b:bytes)->str:return hashlib.sha256(b).hexdigest()
def aup(x,a):return (x+a-1)&~(a-1)

def parse_pe(b:bytes):
    if b[:2]!=b'MZ': raise SystemExit('not MZ')
    lf=struct.unpack_from('<I',b,0x3c)[0]
    if b[lf:lf+4]!=b'PE\0\0': raise SystemExit('not PE')
    n=struct.unpack_from('<H',b,lf+6)[0]
    optsz=struct.unpack_from('<H',b,lf+20)[0]
    opt=lf+24
    if struct.unpack_from('<H',b,opt)[0]!=0x20b: raise SystemExit('not PE32+')
    image_base=struct.unpack_from('<Q',b,opt+24)[0]
    sec_align=struct.unpack_from('<I',b,opt+32)[0]
    file_align=struct.unpack_from('<I',b,opt+36)[0]
    sec0=opt+optsz
    secs=[]
    for i in range(n):
        o=sec0+i*40
        name=b[o:o+8].rstrip(b'\0').decode('ascii')
        vs,va,rs,rp=struct.unpack_from('<IIII',b,o+8)
        ch=struct.unpack_from('<I',b,o+36)[0]
        secs.append(dict(name=name,vs=vs,va=va,rs=rs,rp=rp,ch=ch,hdr=o))
    return dict(lf=lf,opt=opt,optsz=optsz,sections=secs,image_base=image_base,sec_align=sec_align,file_align=file_align,dd=opt+112)

def rva_off(pe,rva):
    for s in pe['sections']:
        if s['va']<=rva<s['va']+max(s['vs'],s['rs']):
            return s['rp']+(rva-s['va'])
    raise ValueError(f'RVA not file-backed/mapped: 0x{rva:X}')

def exports(b,pe):
    erva,esz=struct.unpack_from('<II',b,pe['dd'])
    o=rva_off(pe,erva)
    base=struct.unpack_from('<I',b,o+16)[0]
    nf=struct.unpack_from('<I',b,o+20)[0]
    nn=struct.unpack_from('<I',b,o+24)[0]
    funcs=struct.unpack_from('<I',b,o+28)[0]
    names=struct.unpack_from('<I',b,o+32)[0]
    ords=struct.unpack_from('<I',b,o+36)[0]
    fo=rva_off(pe,funcs); no=rva_off(pe,names); oo=rva_off(pe,ords)
    out={}
    for i in range(nn):
        nrva=struct.unpack_from('<I',b,no+4*i)[0]
        noff=rva_off(pe,nrva); end=b.index(0,noff); name=b[noff:end].decode('ascii')
        ordidx=struct.unpack_from('<H',b,oo+2*i)[0]
        if ordidx>=nf: raise SystemExit('bad export ordinal')
        frva=struct.unpack_from('<I',b,fo+4*ordidx)[0]
        out[name]=frva
    return out

def pe_checksum(buf:bytearray, checksum_off:int)->int:
    # PE checksum algorithm: 16-bit folded sum excluding checksum field, plus file length.
    total=0; n=len(buf); i=0
    while i+1<n:
        if checksum_off<=i<checksum_off+4:
            word=0
        else:
            word=buf[i] | (buf[i+1]<<8)
        total=(total+word)&0xffffffff
        total=(total&0xffff)+(total>>16)
        i+=2
    if i<n:
        total=(total+buf[i])&0xffffffff
        total=(total&0xffff)+(total>>16)
    total=(total&0xffff)+(total>>16)
    total=total+(total>>16)
    return (total&0xffff)+n

def patch_rel32(out,pe,rva,target_rva,kind,expected):
    o=rva_off(pe,rva)
    got=bytes(out[o:o+len(expected)])
    if got!=expected: raise SystemExit(f'preimage mismatch {kind} @0x{rva:X}: {got.hex()} != {expected.hex()}')
    disp=target_rva-(rva+5)
    if not -(1<<31)<=disp<(1<<31): raise SystemExit('rel32 out of range')
    op=0xE9 if kind=='jmp5' else 0xE8
    rep=bytes([op])+struct.pack('<i',disp)
    if kind=='call6': rep+=b'\x90'
    if len(rep)!=len(expected): raise SystemExit('patch width mismatch')
    out[o:o+len(rep)]=rep
    return dict(rva=f'0x{rva:X}',old=expected.hex(),new=rep.hex(),target_rva=f'0x{target_rva:X}')

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--host',type=Path,required=True)
    ap.add_argument('--extension',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--audit',type=Path,required=True)
    a=ap.parse_args()

    host=a.host.read_bytes(); ext=a.extension.read_bytes()
    if len(host)!=HOST_SIZE or sha256(host)!=HOST_SHA: raise SystemExit('shipping 1.45 identity mismatch')
    hp=parse_pe(host); ep=parse_pe(ext)
    if hp['image_base']!=HOST_IMAGE_BASE: raise SystemExit('host imagebase mismatch')
    if ep['image_base']!=EXT_EXPECTED_IMAGE_BASE: raise SystemExit('extension imagebase mismatch')
    # Extension must be importless and relocation-free after addon_base+RVA refactor.
    if struct.unpack_from('<II',ext,ep['dd']+8)[0]!=0: raise SystemExit('extension unexpectedly imports DLLs')
    reloc_rva,reloc_sz=struct.unpack_from('<II',ext,ep['dd']+5*8)
    if reloc_rva or reloc_sz: raise SystemExit('extension unexpectedly contains base relocations')

    sx=next(s for s in hp['sections'] if s['name']=='.srgbmt')
    current_raw_end_rva=sx['va']+sx['rs']
    delta=ep['image_base']-hp['image_base']
    etext=next(s for s in ep['sections'] if s['name']=='.text')
    if delta+etext['va']!=current_raw_end_rva:
        raise SystemExit(f'extension placement mismatch: text target 0x{delta+etext["va"]:X}, host free 0x{current_raw_end_rva:X}')

    out=bytearray(host)
    copied=[]
    max_end=current_raw_end_rva
    # Copy section raw data to same absolute VMA, expressed as host RVA.
    for s in ep['sections']:
        target_rva=delta+s['va']
        vend=target_rva+s['vs']
        max_end=max(max_end,vend)
        if s['rs']:
            dst=sx['rp']+(target_rva-sx['va'])
            need=dst+s['rs']
            if len(out)<need: out.extend(b'\0'*(need-len(out)))
            raw=ext[s['rp']:s['rp']+s['rs']]
            out[dst:dst+s['rs']]=raw
        copied.append(dict(name=s['name'],target_rva=f'0x{target_rva:X}',virtual_size=s['vs'],raw_size=s['rs']))

    # Build a combined x64 RUNTIME_FUNCTION table. Existing table remains preserved but directory points here.
    h_exc_rva,h_exc_sz=struct.unpack_from('<II',host,hp['dd']+3*8)
    heo=rva_off(hp,h_exc_rva)
    host_rows=[struct.unpack_from('<III',host,heo+i) for i in range(0,h_exc_sz,12)]
    e_exc_rva,e_exc_sz=struct.unpack_from('<II',ext,ep['dd']+3*8)
    eeo=rva_off(ep,e_exc_rva)
    ext_rows=[]
    for i in range(0,e_exc_sz,12):
        beg,end,uw=struct.unpack_from('<III',ext,eeo+i)
        ext_rows.append((beg+delta,end+delta,uw+delta))
    combined=sorted(host_rows+ext_rows,key=lambda x:x[0])
    for x,y in zip(combined,combined[1:]):
        if x[0]>=y[0]: raise SystemExit('exception table not strictly ordered')
    exc_blob=b''.join(struct.pack('<III',*r) for r in combined)
    new_exc_rva=aup(max_end,hp['sec_align'])
    exc_dst=sx['rp']+(new_exc_rva-sx['va'])
    need=exc_dst+len(exc_blob)
    if len(out)<need: out.extend(b'\0'*(need-len(out)))
    out[exc_dst:exc_dst+len(exc_blob)]=exc_blob
    max_end=new_exc_rva+len(exc_blob)
    struct.pack_into('<II',out,hp['dd']+3*8,new_exc_rva,len(exc_blob))

    # Extend the existing .srgbmt file-backed span. VirtualSize is already huge and must not change.
    new_rs=aup(max_end-sx['va'],hp['file_align'])
    new_file_end=sx['rp']+new_rs
    if len(out)<new_file_end: out.extend(b'\0'*(new_file_end-len(out)))
    elif len(out)>new_file_end: out=out[:new_file_end]
    struct.pack_into('<I',out,sx['hdr']+16,new_rs)

    # Patch only audited callsites/exports.
    ex=exports(ext,ep)
    required={v[2] for v in PATCHES.values()}
    miss=required-set(ex)
    if miss: raise SystemExit(f'missing extension exports: {sorted(miss)}')
    patch_audit=[]
    for label,(rva,pre,sym,kind) in PATCHES.items():
        target=delta+ex[sym]
        pa=patch_rel32(out,hp,rva,target,kind,pre); pa['label']=label;pa['symbol']=sym;patch_audit.append(pa)

    # PE checksum.
    checksum_off=hp['opt']+64
    struct.pack_into('<I',out,checksum_off,0)
    csum=pe_checksum(out,checksum_off)
    struct.pack_into('<I',out,checksum_off,csum)

    a.out.parent.mkdir(parents=True,exist_ok=True); a.out.write_bytes(out)
    audit={
      'schema':'dsrrl.a3.ul_monolith_injection.v1','status':'CONSTRUCTION_PASS_DIAGNOSTIC',
      'source_complete':False,'release_eligible':False,
      'host_sha256':HOST_SHA,'host_size':HOST_SIZE,
      'extension_sha256':sha256(ext),'extension_size':len(ext),
      'output_sha256':sha256(out),'output_size':len(out),'pe_checksum':f'0x{csum:08X}',
      'extension_delta_rva':f'0x{delta:X}','srgbmt_old_raw_size':sx['rs'],'srgbmt_new_raw_size':new_rs,
      'copied_sections':copied,
      'exception_table':{'old_rva':f'0x{h_exc_rva:X}','old_count':len(host_rows),'new_rva':f'0x{new_exc_rva:X}','new_count':len(combined),'new_size':len(exc_blob)},
      'patches':patch_audit,
      'invariants':[
        'exact shipping 1.45 preimage required','extension importless','extension relocation-free via addon_base+RVA addressing',
        'single native DrawIndexed/DrawIndexedInstanced retained; call target wrapped only','existing POST/PS/CB12 restore unmodified',
        'EnvDiffuse resource bridge OFF','EnvSpec replacement OFF beyond shipping policy','diagnostic build is not source-complete RC/release'
      ]
    }
    a.audit.parent.mkdir(parents=True,exist_ok=True);a.audit.write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps(audit,indent=2))

if __name__=='__main__':main()