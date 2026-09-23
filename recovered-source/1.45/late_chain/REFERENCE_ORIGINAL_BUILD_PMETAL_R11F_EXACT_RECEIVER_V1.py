#!/usr/bin/env python3
from pathlib import Path
import struct, hashlib, json, math, subprocess, tempfile, shutil, zipfile, os, re

BASE=Path('/mnt/data/DSRRL_Material_Response_1.45_OFFLINE_RADIANCE.addon64')
PACK=Path('/mnt/data/PTDE_GI_ENVSPEC_PACK_R11F.bin')
OUTDIR=Path('/mnt/data/DSRRL_Material_Response_1.45_PMETAL_R11F_EXACT_RECEIVER_V1_2026-09-21')
OUT=OUTDIR/'DSRRL_Material_Response_1.45.addon64'
ZIP=Path('/mnt/data/DSRRL_Material_Response_1.45_PMETAL_R11F_EXACT_RECEIVER_V1_RUNTIME_2026-09-21.zip')
BASE_SHA='a5dc6f817e75e157bd92ee945830d0dd3028ba887f544ebf6cff9976e149c900'
PACK_SHA='6c660c9256e2b1279e53bfc0c3c8663ed39cab130ebba238fb7fb22c4f223e4b'
PACK_SIZE=33619968
IMAGE_BASE=0x180000000
NOP=0x0100003a
SAT_BIT=0x2000
READY=0x180112277
DEVICE=0x180107880
GUARD=0x18000e548
ORIG_RDI=0x180107a10
STATE=[0x180108800,0x180108808,0x180108810]
DXBC_VA=[0x18018b000,0x180190250,0x180195370]
DXBC_LEN=[0x4da0,0x4c74,0x469c]
DEDICATED_LEN=[0x4dcc,0x4ca0,0x46c8]
ROUTE=345
# Current build119 embedded indices / V5-compatible word coordinates.
T={
 33:{'direct':7,'t12':1605,'t14':1630,'sample_old':1316,'nop_start':1652,'mul':1667,'terminal':2822,'vis':(1045,1064)},
 34:{'direct':6,'t12':1514,'t14':1539,'sample_old':1225,'nop_start':1561,'mul':1576,'terminal':2741,'vis':(954,973)},
 35:{'direct':5,'t12':1174,'t14':1199,'sample_old':885,'nop_start':1221,'mul':1236,'terminal':2394,'vis':None},
}
SLOT={33:72,34:73,35:74}

# ---- DXBC checksum: AMD/GPUOpen algorithm ----
MASK=0xffffffff
ROT=[7,12,17,22]*4+[5,9,14,20]*4+[4,11,16,23]*4+[6,10,15,21]*4
K=[int(abs(math.sin(i+1))*(1<<32))&MASK for i in range(64)]
def rol(x,n): return ((x<<n)|(x>>(32-n)))&MASK
def md5_transform(st, block):
    M=list(struct.unpack('<16I',block)); a,b,c,d=st; aa,bb,cc,dd=a,b,c,d
    for i in range(64):
        if i<16: f=(b&c)|((~b)&d); g=i
        elif i<32: f=(d&b)|((~d)&c); g=(5*i+1)%16
        elif i<48: f=b^c^d; g=(3*i+5)%16
        else: f=c^(b|(~d)); g=(7*i)%16
        f &= MASK
        oldd=d; d=c; c=b
        b=(b+rol((a+f+K[i]+M[g])&MASK,ROT[i]))&MASK
        a=oldd
    return [(aa+a)&MASK,(bb+b)&MASK,(cc+c)&MASK,(dd+d)&MASK]
def dxbc_checksum(blob:bytes)->bytes:
    p=blob[20:]; bits=(len(p)*8)&MASK
    st=[0x67452301,0xefcdab89,0x98badcfe,0x10325476]
    full=(len(p)//64)*64
    for off in range(0,full,64): st=md5_transform(st,p[off:off+64])
    rem=p[full:]; r=len(rem)
    if r>=56:
        b1=rem+b'\x80'+b'\0'*(63-r); st=md5_transform(st,b1)
        b2=bytearray(64); struct.pack_into('<I',b2,0,bits); struct.pack_into('<I',b2,60,((bits>>2)|1)&MASK); st=md5_transform(st,b2)
    else:
        b=bytearray(64); struct.pack_into('<I',b,0,bits); b[4:4+r]=rem; b[4+r]=0x80; struct.pack_into('<I',b,60,((bits>>2)|1)&MASK); st=md5_transform(st,b)
    return struct.pack('<4I',*st)
def valid_dxbc(blob): return blob[:4]==b'DXBC' and blob[4:20]==dxbc_checksum(blob)

def sha(b): return hashlib.sha256(b).hexdigest()

def scan_dxbc(data):
    out=[];pos=0
    while True:
        i=data.find(b'DXBC',pos)
        if i<0: break
        if i+32<=len(data):
            sz=struct.unpack_from('<I',data,i+24)[0]; n=struct.unpack_from('<I',data,i+28)[0]
            if 32<=sz<=200000 and i+sz<=len(data) and 1<=n<=32: out.append((i,sz,data[i:i+sz]))
        pos=i+4
    return out

def parse_pe(data):
    pe=struct.unpack_from('<I',data,0x3c)[0]; n=struct.unpack_from('<H',data,pe+6)[0]; osz=struct.unpack_from('<H',data,pe+20)[0]; so=pe+24+osz
    secs=[]
    for i in range(n):
        o=so+i*40; name=data[o:o+8].rstrip(b'\0').decode(errors='ignore'); vs,va,rs,rp=struct.unpack_from('<IIII',data,o+8); ch=struct.unpack_from('<I',data,o+36)[0]
        secs.append({'name':name,'vs':vs,'va':va,'rs':rs,'rp':rp,'hdr':o,'ch':ch})
    return pe,secs

def rvaoff(data,rva):
    _,secs=parse_pe(data)
    for s in secs:
        if s['va']<=rva<s['va']+max(s['vs'],s['rs']): return s['rp']+(rva-s['va'])
    raise ValueError(hex(rva))

def shex(blob):
    n=struct.unpack_from('<I',blob,28)[0]
    for k in range(n):
        o=struct.unpack_from('<I',blob,32+4*k)[0]; tag=blob[o:o+4]; sz=struct.unpack_from('<I',blob,o+4)[0]
        if tag in (b'SHEX',b'SHDR'):
            return o+8,list(struct.unpack_from('<'+'I'*(sz//4),blob,o+8))
    raise RuntimeError('no SHEX')

def rebuild_shex(blob, words):
    n=struct.unpack_from('<I',blob,28)[0]
    offs=[struct.unpack_from('<I',blob,32+4*k)[0] for k in range(n)]
    si=None
    for k,o in enumerate(offs):
        if blob[o:o+4] in (b'SHEX',b'SHDR'):
            si=k; shex_off=o; old_sz=struct.unpack_from('<I',blob,o+4)[0]; tag=blob[o:o+4]; break
    assert si is not None
    old_end=shex_off+8+old_sz
    next_off=min([o for o in offs if o> shex_off], default=len(blob))
    assert old_end==next_off
    new_data=struct.pack('<'+'I'*len(words),*words)
    delta=len(new_data)-old_sz
    assert delta==44
    out=bytearray(blob[:shex_off])
    out += tag + struct.pack('<I',len(new_data)) + new_data
    out += blob[old_end:]
    # DXBC total size and shifted chunk table offsets.
    struct.pack_into('<I',out,24,len(out))
    for k,o in enumerate(offs):
        no=o+delta if o> shex_off else o
        struct.pack_into('<I',out,32+4*k,no)
    out[4:20]=dxbc_checksum(bytes(out))
    return bytes(out)

def patch_shader(blob, idx):
    assert valid_dxbc(blob)
    off,w=shex(blob); t=T[idx]
    old_non_shex=[]
    # EnvSpec t12/t14: direct reflection vector and physical PTDE one-mip LOD0.
    for pos,residx in [(t['t12'],12),(t['t14'],14)]:
        ins=w[pos:pos+13]
        assert len(ins)==13 and (ins[0]&0x7ff)==0x48, (idx,pos,hex(ins[0])) # SAMPLE_L
        assert ins[8]==residx, (idx,pos,'resource',ins[8])
        ins[6]=t['direct']
        ins[11]=0x00004001
        ins[12]=0x00000000
        w[pos:pos+13]=ins
    # Remove DSR-only visibility response exponent, retaining the pre-exponent visibility core.
    if t['vis']:
        a,z=t['vis']; assert z-a==19
        assert (w[a]&0x7ff)==0x2f
        assert (w[a+6]&0x7ff)==0x38
        assert (w[a+14]&0x7ff)==0x19
        w[a:z]=[NOP]*19
    # Fresh exact PTDE SpecRGB re-acquire. Build119 has no 15-DWORD NOP island,
    # so the dedicated copy grows structurally by 11 DWORDs. Slots 72/73/74 have
    # certified capacity for this growth; shared receivers remain byte-identical.
    old=w[t['sample_old']:t['sample_old']+11]
    assert len(old)==11 and (old[0]&0x7ff)==0x45
    assert old[7]==0x00107936 and old[8]==10
    assert old[9]==0x00106000 and old[10]==1
    fresh=list(old); fresh[3]=0x00100072; fresh[4]=2 # r2.xyz
    insert=t['mul']
    w[insert:insert]=fresh
    w[1]+=11 # tokenized shader program DWORD count
    mulpos=t['mul']+11
    mul=w[mulpos:mulpos+8]
    assert mul[0]==0x08000038 and mul[1]==0x00100072 and mul[2]==2
    oldmat=mul[4]; assert oldmat in (9,10,11)
    w[mulpos+4]=2
    term=t['terminal']+11
    assert (w[term]&0x7ff)==0x36
    w[term] |= SAT_BIT
    out=rebuild_shex(blob,w)
    assert valid_dxbc(out) and len(out)==len(blob)+44
    return out, {'src_index':idx,'dst_index':SLOT[idx],'direct_R_reg':f'r{t["direct"]}','t12_word':t['t12'],'t14_word':t['t14'],'fresh_specrgb_insert_word':insert,'material_mul_word_after_insert':mulpos,'old_stale_material_reg':f'r{oldmat}','new_material_reg':'r2','visibility_exponent_removed':bool(t['vis']),'terminal_sat_word_after_insert':term,'size_before':len(blob),'size_after':len(out),'sha256':sha(out)}

def assemble(name, va, source, syms):
    td=Path(tempfile.mkdtemp(prefix='dsrrl_asm_'))
    try:
        s=td/(name+'.s'); o=td/(name+'.o'); elf=td/(name+'.elf'); raw=td/(name+'.bin')
        s.write_text('.intel_syntax noprefix\n.text\n.global _start\n_start:\n'+source+'\n',encoding='utf-8')
        subprocess.run(['as','--64','-o',str(o),str(s)],check=True,capture_output=True)
        cmd=['ld','-nostdlib','-Ttext='+hex(va),'-e','_start']
        for k,v in syms.items(): cmd += ['--defsym',f'{k}={hex(v)}']
        cmd += ['-o',str(elf),str(o)]
        subprocess.run(cmd,check=True,capture_output=True)
        subprocess.run(['objcopy','-O','binary','-j','.text',str(elf),str(raw)],check=True,capture_output=True)
        blob=raw.read_bytes()
        # no dynamic relocations in final ELF for the text we extract
        rel=subprocess.run(['readelf','-r',str(elf)],check=True,capture_output=True,text=True).stdout
        if 'There are no relocations in this file.' not in rel: raise RuntimeError(name+' unresolved relocations\n'+rel)
        return blob, source
    finally:
        shutil.rmtree(td)

def rel_jmp(src,dst): return b'\xE9'+struct.pack('<i',dst-(src+5))

def patch_va(buf,va,expected,new,label):
    off=rvaoff(buf,va-IMAGE_BASE)
    got=bytes(buf[off:off+len(expected)])
    if got!=expected: raise RuntimeError(f'{label}: got {got.hex()} expected {expected.hex()}')
    if len(new)>len(expected): raise RuntimeError(label+' too long')
    buf[off:off+len(expected)]=new+b'\x90'*(len(expected)-len(new))

base=BASE.read_bytes(); assert sha(base)==BASE_SHA and len(base)==1803264
pack=PACK.read_bytes(); assert len(pack)==PACK_SIZE and sha(pack)==PACK_SHA
pe,secs=parse_pe(base); sec={s['name']:s for s in secs}
assert sec['.v13x']['ch']&0x20000000 and sec['.data']['ch']&0x80000000
# Cave certified untouched/unused by byte content. External textual xref audit is emitted separately.
cave_ranges=[(0x10d8f4,0x10da00),(0x10da00,0x10dc00),(0x10dc00,0x10dd00),(0x10dd00,0x10e000)]
for a,z in cave_ranges:
    x=base[rvaoff(base,a):rvaoff(base,a)+(z-a)]
    assert set(x)<={0,0xcc}, (hex(a),hex(z),set(x))
# .data BSS extension must remain before .pdata and not require raw bytes.
assert sec['.data']['va']==0x107000 and sec['.data']['vs']==0x1100 and sec['.pdata']['va']==0x109000
assert 0x108818 < sec['.pdata']['va']

# Dedicated DXBC materialization into already reserved/unreferenced slots 72/73/74.
emb=scan_dxbc(base); assert len(emb)==78
assert [emb[i][0] for i in [33,34,35,72,73,74]]==[0xbbb40,0xc08e0,0xc5560,0x184400,0x189650,0x18e770]
assert [emb[i][1] for i in [33,34,35]]==DXBC_LEN
assert all(valid_dxbc(e[2]) for e in emb)
addon=bytearray(base); shader_audit=[]
for src in [33,34,35]:
    new,info=patch_shader(emb[src][2],src); dst=SLOT[src]; doff,cap,_=emb[dst]
    assert len(new)<=cap
    addon[doff:doff+cap]=new+b'\0'*(cap-len(new))
    shader_audit.append(info)
# Extend .data VirtualSize only, certifying private zero-filled qwords at RVA 0x108800..817.
struct.pack_into('<I',addon,sec['.data']['hdr']+8,0x1900)

# Build code stubs in the existing .v13x cave.
common={'READY':READY,'STATE0':STATE[0],'STATE1':STATE[1],'STATE2':STATE[2]}
gate_src=r'''
    mov ecx, DWORD PTR [rcx+0x14]
    cmp BYTE PTR [rip+READY], 1
    jne reject
    cmp edi, 345
    jne reject
    cmp ecx, 9
    je check0
    cmp ecx, 10
    je check1
    cmp ecx, 11
    jne reject
    cmp QWORD PTR [rip+STATE2], 0
    je reject
    jmp accept
check1:
    cmp QWORD PTR [rip+STATE1], 0
    je reject
    jmp accept
check0:
    cmp QWORD PTR [rip+STATE0], 0
    je reject
accept:
    test ecx, ecx
    jmp PREP_CONT
reject:
    jmp PREP_REJECT
'''
gate, _=assemble('gate',IMAGE_BASE+0x10d8f4,gate_src,{**common,'PREP_CONT':0x18019abd8,'PREP_REJECT':0x18019ab9d})
assert len(gate)<=0x10da00-0x10d8f4

init_src=r'''
    xor eax, eax
    mov QWORD PTR [rip+STATE0], rax
    mov QWORD PTR [rip+STATE1], rax
    mov QWORD PTR [rip+STATE2], rax
    mov rcx, QWORD PTR [rip+DEVICE]
    test rcx, rcx
    je done
    mov rax, QWORD PTR [rcx]
    lea rdx, [rip+DX0]
    mov r8d, 0x4dcc
    xor r9d, r9d
    lea r10, [rip+STATE0]
    mov QWORD PTR [rsp+0x20], r10
    mov rax, QWORD PTR [rax+0x78]
    call QWORD PTR [rip+GUARD]
    mov rcx, QWORD PTR [rip+DEVICE]
    test rcx, rcx
    je done
    mov rax, QWORD PTR [rcx]
    lea rdx, [rip+DX1]
    mov r8d, 0x4ca0
    xor r9d, r9d
    lea r10, [rip+STATE1]
    mov QWORD PTR [rsp+0x20], r10
    mov rax, QWORD PTR [rax+0x78]
    call QWORD PTR [rip+GUARD]
    mov rcx, QWORD PTR [rip+DEVICE]
    test rcx, rcx
    je done
    mov rax, QWORD PTR [rcx]
    lea rdx, [rip+DX2]
    mov r8d, 0x46c8
    xor r9d, r9d
    lea r10, [rip+STATE2]
    mov QWORD PTR [rsp+0x20], r10
    mov rax, QWORD PTR [rax+0x78]
    call QWORD PTR [rip+GUARD]
done:
    lea rax, [r13+0x100f58]
    jmp INIT_CONT
'''
init, _=assemble('init',IMAGE_BASE+0x10da00,init_src,{**common,'DEVICE':DEVICE,'GUARD':GUARD,'DX0':DXBC_VA[0],'DX1':DXBC_VA[1],'DX2':DXBC_VA[2],'INIT_CONT':0x180006982})
assert len(init)<=0x200

selector_src=r'''
    cmp BYTE PTR [rip+READY], 1
    jne fallback
    cmp edi, 345
    jne fallback
    movsxd rax, DWORD PTR [rsi]
    cmp eax, 9
    jb fallback
    cmp eax, 11
    ja fallback
    sub eax, 9
    lea rcx, [rip+STATE0]
    mov rcx, QWORD PTR [rcx+rax*8]
    test rcx, rcx
    je fallback
    mov QWORD PTR [rbp-0x79], rcx
    jmp SELECT_DONE
fallback:
    cmp r12b, 2
    jne ORIGINAL_NE
    jmp ORIGINAL_EQ
'''
selector, _=assemble('selector',IMAGE_BASE+0x10dc00,selector_src,{**common,'SELECT_DONE':0x18000641c,'ORIGINAL_NE':0x180189336,'ORIGINAL_EQ':0x18018930a})
assert len(selector)<=0x100

cleanup_src=r'''
    mov rcx, QWORD PTR [rip+STATE0]
    test rcx, rcx
    je next1
    mov rax, QWORD PTR [rcx]
    mov rax, QWORD PTR [rax+0x10]
    call QWORD PTR [rip+GUARD]
    xor eax, eax
    mov QWORD PTR [rip+STATE0], rax
next1:
    mov rcx, QWORD PTR [rip+STATE1]
    test rcx, rcx
    je next2
    mov rax, QWORD PTR [rcx]
    mov rax, QWORD PTR [rax+0x10]
    call QWORD PTR [rip+GUARD]
    xor eax, eax
    mov QWORD PTR [rip+STATE1], rax
next2:
    mov rcx, QWORD PTR [rip+STATE2]
    test rcx, rcx
    je done
    mov rax, QWORD PTR [rcx]
    mov rax, QWORD PTR [rax+0x10]
    call QWORD PTR [rip+GUARD]
    xor eax, eax
    mov QWORD PTR [rip+STATE2], rax
done:
    mov rdi, QWORD PTR [rip+ORIG_RDI]
    jmp CLEAN_CONT
'''
cleanup, _=assemble('cleanup',IMAGE_BASE+0x10dd00,cleanup_src,{**common,'GUARD':GUARD,'ORIG_RDI':ORIG_RDI,'CLEAN_CONT':0x180007e78})
assert len(cleanup)<=0x300

# Write stubs into the certified code cave.
for rva,blob in [(0x10d8f4,gate),(0x10da00,init),(0x10dc00,selector),(0x10dd00,cleanup)]:
    o=rvaoff(addon,rva); addon[o:o+len(blob)]=blob
# Exact guarded entry-point detours.
patch_va(addon,0x18019abd3,bytes.fromhex('8b491485c9'),rel_jmp(0x18019abd3,0x18010d8f4),'PREPARE gate')
patch_va(addon,0x180189182,bytes.fromhex('498d85580f1000e9f4d7e7ff'),rel_jmp(0x180189182,0x18010da00),'dedicated PS init')
patch_va(addon,0x180189300,bytes.fromhex('4180fc020f852c000000'),rel_jmp(0x180189300,0x18010dc00),'exact route selector')
patch_va(addon,0x1801895e4,bytes.fromhex('488b3d25e4f7ffe988e8e7ff'),rel_jmp(0x1801895e4,0x18010dd00),'dedicated PS cleanup')

# Output and independent structural validations.
OUTDIR.mkdir(parents=True,exist_ok=True); OUT.write_bytes(addon)
out=bytes(addon); emb2=scan_dxbc(out); assert len(emb2)==78
assert all(valid_dxbc(e[2]) for e in emb2), [i for i,e in enumerate(emb2) if not valid_dxbc(e[2])]
# shared receivers untouched; dedicated copies match intended modified source bytecode exactly
for i in [33,34,35]: assert emb2[i][2]==emb[i][2]
for src,dst in SLOT.items():
    expected,_=patch_shader(emb[src][2],src); assert emb2[dst][2]==expected
# route table exact identity: known route table around route records from canonical build remains globally unchanged outside declared regions.
# Stronger check: no byte in canonical source route-table region changes. Determine region by prior known 368*record area if present via exact full-file diff policy below.
# Header raw sizes and pack path/SHA/format remain byte-identical except .data VirtualSize.
_,s2=parse_pe(out); sd={s['name']:s for s in s2}
assert sd['.data']['vs']==0x1900 and sd['.data']['rs']==sec['.data']['rs'] and sd['.pdata']['va']==0x109000
# external pack destination begins RVA 0x1bf000; all injected executable stubs are <0x10e000 and cannot be overwritten by pack load.
assert max(0x10d8f4+len(gate),0x10da00+len(init),0x10dc00+len(selector),0x10dd00+len(cleanup)) < 0x10e000 < 0x1bf000
# data storage is disjoint from .pdata and from V15.7 live .v13d state.
assert STATE[0]-IMAGE_BASE>=0x108800 and STATE[-1]-IMAGE_BASE+8<=0x108818<0x109000
# imports / exception / reloc data-directory entries unchanged.
opt=pe+24
for idx in [1,3,5,9,10,12]: # import, exception, base reloc, TLS, load config, IAT
    a=base[opt+112+idx*8:opt+112+idx*8+8]; b=out[opt+112+idx*8:opt+112+idx*8+8]; assert a==b,(idx,a.hex(),b.hex())
# exact external R11F pack filename + expected SHA bytes survived
assert b'P\x00T\x00D\x00E\x00_\x00G\x00I\x00_\x00E\x00N\x00V\x00S\x00P\x00E\x00C\x00_\x00P\x00A\x00C\x00K\x00_\x00R\x001\x001\x00F\x00.\x00b\x00i\x00n\x00' in out
# Loader/path/SHA/format region is byte-identical to certified R11F RC1.
assert out[0x1B5800:0x1B7604] == base[0x1B5800:0x1B7604]
# format constants at known R11F locations retained (26)
assert struct.unpack_from('<I',out,0x1B7590)[0]==26 and struct.unpack_from('<I',out,0x1B75A0)[0]==26

# Analyze raw diff containment.
diffs=[i for i,(a,b) in enumerate(zip(base,out)) if a!=b]
# Build allowed file ranges.
allowed=[]
def add_range(off,n,label): allowed.append((off,off+n,label))
for dst in [72,73,74]: add_range(emb[dst][0],emb[dst][1],f'DXBC{dst}')
for rva,blob,label in [(0x10d8f4,gate,'gate_cave'),(0x10da00,init,'init_cave'),(0x10dc00,selector,'selector_cave'),(0x10dd00,cleanup,'cleanup_cave')]: add_range(rvaoff(base,rva),len(blob),label)
for va,n,label in [(0x18019abd3,5,'prepare_detour'),(0x180189182,12,'init_detour'),(0x180189300,10,'selector_detour'),(0x1801895e4,12,'cleanup_detour')]: add_range(rvaoff(base,va-IMAGE_BASE),n,label)
add_range(sec['.data']['hdr']+8,4,'data_virtual_size')
outside=[i for i in diffs if not any(a<=i<b for a,b,_ in allowed)]
assert not outside, outside[:20]
# record per-range changed byte counts
range_counts={lab:sum(1 for i in diffs if a<=i<b) for a,b,lab in allowed}

# Optional static xref scan against the original objdump text: no external textual refs into the chosen v13x cave.
disasm=Path('/mnt/data/current_full_disasm.txt')
external_cave_refs=[]
if disasm.exists():
    lo=IMAGE_BASE+0x10d8f4; hi=IMAGE_BASE+0x10e000
    for line in disasm.read_text(errors='ignore').splitlines():
        m=re.match(r'\s*([0-9a-f]+):',line)
        if not m: continue
        src=int(m.group(1),16)
        for ss in re.findall(r'\b(180[0-9a-f]{7,})\b',line):
            dst=int(ss,16)
            if lo<=dst<hi and not(lo<=src<hi): external_cave_refs.append({'src':hex(src),'dst':hex(dst),'line':line.strip()})
assert not external_cave_refs, external_cave_refs[:3]

audit={
 'build':'Material Response 1.45 P_Metal R11F Exact Receiver V1',
 'basis_build':'build119 / 1.45-OFFLINE-ENVSPEC-R11F-RC1',
 'basis_sha256':BASE_SHA,
 'output_sha256':sha(out),
 'size_bytes':len(out),
 'construction_status':'PASS',
 'compatibility_status':'PASS_STATIC',
 'runtime_liveness':'NOT_TESTED',
 'bridge_activation':'NOT_TESTED',
 'pixel_behavior':'OPEN',
 'exact_material_route':345,
 'receiver_gate':{'route_index':345,'source_indices':[9,10,11],'sidecar_ready_required':True,'dedicated_ps_nonnull_required':True},
 'fail_open':'Any missing/wrong R11F sidecar, non-route345 material, unsupported source index, or failed dedicated PS creation falls back to the pre-existing selector and denies R11F PREPARE substitution.',
 'resource_carrier':{'format':'DXGI_FORMAT_R11G11B10_FLOAT','path':'DSRRL/EnvSpec/PackedGI/PTDE_GI_ENVSPEC_PACK_R11F.bin','sha256':PACK_SHA,'size':PACK_SIZE,'decode_location':'offline','runtime_rgb_over_alpha':False},
 'operator':{'reflection':'direct R','envspec_lod':0,'ptde_ab_donors_beta':'preserved from build119','fresh_specrgb':'t10 re-acquired at final material cut','c101':'cb12[0].rgb preserved','color0':'preserved','dsr_visibility_exponent':'removed on Csd/Sdw only','pre_exponent_visibility':'preserved','envdiffuse_merge':'preserved','terminal_rgb_sat':True,'pointlight_changed':False},
 'dedicated_dxbc':shader_audit,
 'shared_dxbc_33_35_byte_identical':True,
 'dedicated_slots':[72,73,74],
 'all_78_dxbc_checksums_valid':True,
 'code_cave':{'section':'.v13x','rva_range':['0x10d8f4','0x10e000'],'external_textual_xrefs_found':0,'external_pack_destination_rva':'0x1bf000','overlap_with_external_pack':False},
 'state_storage':{'section':'.data','rvas':['0x108800','0x108808','0x108810'],'virtual_size_before':'0x1100','virtual_size_after':'0x1900','next_section_rva':'0x109000','build118_collision_addresses_reused':False,'v15_7_0x112260_state_reused':False},
 'stubs':{'gate_len':len(gate),'init_len':len(init),'selector_len':len(selector),'cleanup_len':len(cleanup)},
 'diff':{'changed_bytes':len(diffs),'outside_declared_regions':0,'per_region_changed_bytes':range_counts},
 'pe_data_directories_unchanged':['IMPORT','EXCEPTION','BASERELOC','TLS','LOAD_CONFIG','IAT'],
 'known_residuals':['Exact PTDE s12/s14 sampler transaction is not yet merged into this R11F V1 receiver; one-mip LOD0 removes LOD filtering dependence but sampler address/seam behavior remains a declared equivalence residual.','R11G11B10_FLOAT offline carrier has the already-canonical small quantization residual versus exact RGB_byte/Alpha_byte.'],
}
(OUTDIR/'STATIC_AUDIT.json').write_text(json.dumps(audit,indent=2),encoding='utf-8')
# Copy offline decoder for reproducibility.
shutil.copy2('/mnt/data/offline_decode_envspec_rgba8_to_r11g11b10.py',OUTDIR/'offline_decode_envspec_rgba8_to_r11g11b10.py')
readme=f'''DSRRL Material Response 1.45 — P_Metal R11F Exact Receiver V1\n\nConstruction: PASS\nRuntime: NOT TESTED\nPixel equivalence: OPEN\n\nThis build is based on the certified offline-R11F RC1. It does NOT decode RGB/A at runtime.\nRequired sidecar:\n  DSRRL\\EnvSpec\\PackedGI\\PTDE_GI_ENVSPEC_PACK_R11F.bin\n  size: {PACK_SIZE}\n  SHA256: {PACK_SHA}\n\nExact gate:\n  actual-material route345 (P_Metal[DSB].mtd)\n  AND source receiver index 9/10/11\n  AND exact R11F sidecar READY\n  AND dedicated pixel shader creation succeeded.\nAnything else fails open to the previous DSR/Material Response path.\n\nP_Metal EnvSpec island in the dedicated receivers:\n  direct reflection R -> R11F t12/t14 LOD0 -> existing PTDE A/B donors+beta -> fresh t10 SpecRGB -> cb12[0] c101 -> COLOR0.\n  The DSR-only visibility response exponent is removed only on Csd/Sdw; the homologous pre-exponent visibility is preserved. EnvDiffuse merge and PointLight/local microfacet remain untouched. Terminal RGB SAT is enabled.\n\nImportant: this V1 intentionally does not yet merge the exact PTDE s12/s14 sampler transaction. That remains an explicit residual rather than an arbitrary substitute.\n'''
(OUTDIR/'README_RUNTIME_TEST.txt').write_text(readme,encoding='utf-8')
shutil.copy2(__file__,OUTDIR/'BUILD_PMETAL_R11F_EXACT_RECEIVER_V1.py')
# sidecar verifier cmd (PowerShell-independent certutil fallback via Python not assumed); simple sha instructions manifest.
manifest=[]
for p in sorted(OUTDIR.iterdir()):
    if p.is_file() and p.name!='MANIFEST_SHA256.txt': manifest.append((sha(p.read_bytes()),p.name))
(OUTDIR/'MANIFEST_SHA256.txt').write_text(''.join(f'{h}  {n}\n' for h,n in manifest),encoding='utf-8')
if ZIP.exists(): ZIP.unlink()
with zipfile.ZipFile(ZIP,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in sorted(OUTDIR.iterdir()):
        if p.is_file(): z.write(p,p.name)
with zipfile.ZipFile(ZIP) as z: assert z.testzip() is None
print(json.dumps({'addon':str(OUT),'addon_sha256':sha(out),'addon_size':len(out),'zip':str(ZIP),'zip_sha256':sha(ZIP.read_bytes()),'zip_size':ZIP.stat().st_size,'changed_bytes':len(diffs),'stubs':audit['stubs'],'shader_audit':shader_audit},indent=2))