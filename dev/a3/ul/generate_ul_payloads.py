#!/usr/bin/env python3
"""Generate the A3 PTDE Upper/Lower shader payload set reproducibly.

Inputs are external on purpose:
  * vanilla DSR FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx
  * exact shipping DSRRL_Material_Response_1.45.addon64

The script validates semantic shader identities from UL48_CANONICAL_HOSTS.json and
requires the exact 1.45 SHA before extracting P_Metal alternatives 33/34/35.
No file in /mnt/data or previous handoff package is required.
"""
from __future__ import annotations
import argparse, hashlib, json, math, struct, zlib
from pathlib import Path

SHIPPING_145_SHA256='e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342'
PMETAL_ALT_INDICES=(33,34,35)
PMETAL_SHADER_INDICES=(894,913,932)
B13_DECL=[0x04000059,0x00208e46,0x0000000d,0x00000008]
MASK=0xffffffff
S=[7,12,17,22]*4+[5,9,14,20]*4+[4,11,16,23]*4+[6,10,15,21]*4
K=[int(abs(math.sin(i+1))*2**32)&MASK for i in range(64)]

def sha256(data:bytes)->str: return hashlib.sha256(data).hexdigest()
def rol(x,n): return ((x<<n)|(x>>(32-n)))&MASK
def md5_transform(st,block):
    M=list(struct.unpack('<16I',block)); a,b,c,d=st; A,B,C,D=a,b,c,d
    for i in range(64):
        if i<16: f=(b&c)|((~b)&d); g=i
        elif i<32: f=(d&b)|((~d)&c); g=(5*i+1)%16
        elif i<48: f=b^c^d; g=(3*i+5)%16
        else: f=c^(b|(~d)); g=(7*i)%16
        a,d,c,b=d,c,b,(b+rol((a+(f&MASK)+K[i]+M[g])&MASK,S[i]))&MASK
    return [(A+a)&MASK,(B+b)&MASK,(C+c)&MASK,(D+d)&MASK]
def checksum(data):
    p=data[0x14:]; n=len(p); bit=n*8; st=[0x67452301,0xefcdab89,0x98badcfe,0x10325476]
    full=n&~63
    for o in range(0,full,64): st=md5_transform(st,p[o:o+64])
    tail=p[full:]
    if len(tail)>=56:
        st=md5_transform(st,tail+b'\x80'+b'\0'*(63-len(tail)))
        q=[0]*16; q[0]=bit&MASK; q[15]=((bit>>2)|1)&MASK
        st=md5_transform(st,struct.pack('<16I',*q))
    else:
        bb=bytearray(struct.pack('<I',bit&MASK)+tail); bb+=b'\x80'+b'\0'*(64-len(bb)-1)
        struct.pack_into('<I',bb,60,((bit>>2)|1)&MASK); st=md5_transform(st,bytes(bb))
    return struct.pack('<4I',*st)
def fix_checksum(data):
    b=bytearray(data); b[4:20]=checksum(bytes(b)); return bytes(b)

def dxbc_chunks(data):
    if data[:4]!=b'DXBC': raise ValueError('not DXBC')
    n=struct.unpack_from('<I',data,28)[0]
    offs=struct.unpack_from('<'+'I'*n,data,32)
    out=[]
    for o in offs:
        sz=struct.unpack_from('<I',data,o+4)[0]
        if o+8+sz>len(data): raise ValueError('invalid DXBC chunk')
        out.append((o,data[o:o+4],data[o+8:o+8+sz]))
    return out

def code_info(data):
    for o,t,p in dxbc_chunks(data):
        if t in (b'SHEX',b'SHDR'):
            if len(p)%4: raise ValueError('unaligned shader code')
            return o+8,list(struct.unpack('<'+'I'*(len(p)//4),p))
    raise ValueError('no code chunk')

def instrs(w):
    out=[]; i=2
    while i<len(w):
        ln=(w[i]>>24)&0x7f
        if not ln or i+ln>len(w): raise ValueError('invalid instruction stream')
        out.append((i,w[i]&0x7ff,w[i:i+ln])); i+=ln
    if i!=len(w): raise ValueError('instruction stream tail')
    return out

def rebuild(data,new_code_payload):
    chunks=[]
    for _,t,p in dxbc_chunks(data):
        if t==b'RDEF': continue
        chunks.append((t,new_code_payload if t in (b'SHEX',b'SHDR') else p))
    n=len(chunks); header_size=32+4*n; offs=[]; body=bytearray(); cur=header_size
    for t,p in chunks:
        offs.append(cur); ch=t+struct.pack('<I',len(p))+p; body+=ch; cur+=len(ch)
    out=bytearray(data[:20]); out+=struct.pack('<I',1); out+=struct.pack('<I',cur); out+=struct.pack('<I',n); out+=struct.pack('<'+'I'*n,*offs); out+=body
    out[4:20]=b'\0'*16
    return fix_checksum(bytes(out))

def cbrefs(ins):
    out=[]; i=1
    while i<len(ins):
        tok=ins[i]; base=tok & 0x7fffffff
        if (base & 0x00fff00f)==0x00208006:
            ext=bool(tok&0x80000000); j=i+1+(1 if ext else 0)
            if j+1<len(ins): out.append((i,j,j+1,ins[j],ins[j+1],ext))
            i=j+2; continue
        i+=1
    return out

def patch_ul(base:bytes,label:str):
    _,w=code_info(base); aa=instrs(w); candidates=[]
    for i in range(1,len(aa)-1):
        prev,mid,nxt=aa[i-1],aa[i],aa[i+1]
        if prev[1]!=0x32 or mid[1]!=0x0 or nxt[1]!=0x32: continue
        mr=cbrefs(mid[2]); nr=cbrefs(nxt[2])
        sm=[(r[3],r[4]) for r in mr]; sn=[(r[3],r[4]) for r in nr]
        if (0,7) in sm and (0,8) in sm and (0,8) in sn: candidates.append((i,mr,nr))
    if len(candidates)!=1: raise RuntimeError(f'{label}: expected exactly one U/L island, got {len(candidates)}')
    i,mr,nr=candidates[0]; changed=[]
    for which,refs,ins_off in [('ADD',mr,aa[i][0]),('MAD',nr,aa[i+1][0])]:
        for r in refs:
            slot_idx,vec_idx=r[1],r[2]; slot,vec=r[3],r[4]
            if slot!=0 or vec not in (7,8): continue
            newvec=6 if vec==7 else 7
            w[ins_off+slot_idx]=13; w[ins_off+vec_idx]=newvec
            changed.append({'instruction':which,'old_cb':0,'old_index':vec,'new_cb':13,'new_index':newvec})
    if len(changed)!=3: raise RuntimeError(f'{label}: expected three changed U/L references')
    dcls=[x for x in instrs(w) if x[1]==0x59]
    if not dcls: raise RuntimeError(f'{label}: no constant-buffer declaration')
    insert_at=dcls[-1][0]+len(dcls[-1][2])
    nw=w[:insert_at]+B13_DECL+w[insert_at:]; nw[1]=len(nw)
    payload=struct.pack('<'+'I'*len(nw),*nw); out=rebuild(base,payload)
    tags=[t for _,t,_ in dxbc_chunks(out)]
    if b'RDEF' in tags: raise RuntimeError(f'{label}: RDEF retained')
    _,ww=code_info(out); aaa=instrs(ww)
    b13d=[x for x in aaa if x[1]==0x59 and len(x[2])==4 and x[2][1]==0x00208e46 and x[2][2]==13 and x[2][3]==8]
    if len(b13d)!=1: raise RuntimeError(f'{label}: b13 declaration count {len(b13d)}')
    refs=[]
    for _,op,ins in aaa:
        if op==0x59: continue
        for r in cbrefs(ins):
            if r[3]==13: refs.append(r[4])
    if sorted(refs)!=[6,7,7]: raise RuntimeError(f'{label}: unexpected b13 refs {refs}')
    return out,{'island_instruction_index':i,'changed_refs':changed,'b13_refs':refs,'rdef_stripped':True}

def parse_bnd3(data):
    if data[:4]!=b'BND3': raise ValueError('not BND3')
    count=struct.unpack_from('<I',data,0x10)[0]
    if 0x20+count*0x18>len(data): raise ValueError('BND3 table out of range')
    rows=[]
    for i in range(count):
        off=0x20+i*0x18
        flags,size,data_off,fid,name_off,size2=struct.unpack_from('<IIIIII',data,off)
        if size!=size2 or data_off+size>len(data): raise ValueError('invalid BND3 record')
        end=data.find(b'\0',name_off)
        if end<0: raise ValueError('missing BND3 name terminator')
        rows.append({'id':fid,'name':data[name_off:end].decode('ascii'),'data':data[data_off:data_off+size]})
    return rows

def read_shader_binder(path:Path):
    data=path.read_bytes()
    if data.startswith(b'DCX\0'):
        # DSR binder used by this project stores the zlib stream at 0x4c.
        data=zlib.decompress(data[76:])
    return parse_bnd3(data)

def extract_embedded_dxbc(data:bytes):
    hits=[]; pos=0
    while True:
        i=data.find(b'DXBC',pos)
        if i<0: break
        if i+32<=len(data):
            size=struct.unpack_from('<I',data,i+24)[0]; n=struct.unpack_from('<I',data,i+28)[0]
            if 32<=size<=100000 and i+size<=len(data) and n<64:
                ok=i+32+4*n<=i+size
                if ok:
                    for j in range(n):
                        off=struct.unpack_from('<I',data,i+32+4*j)[0]
                        if off+8>size: ok=False; break
                        cs=struct.unpack_from('<I',data,i+off+4)[0]
                        if off+8+cs>size: ok=False; break
                if ok: hits.append(data[i:i+size])
        pos=i+4
    return hits

def emit_header(stock,pmetal,out:Path):
    lines=['#pragma once','typedef unsigned char u8; typedef unsigned int u32; typedef unsigned long long u64;','struct ULBlob { u32 shader_index; const u8 *code; u32 size; };']
    for i,r in enumerate(stock):
        b=r['code']; lines.append(f'static const u8 g_ul_stock_blob_{i}[{len(b)}]={{')
        for j in range(0,len(b),32): lines.append(','.join(str(x) for x in b[j:j+32])+',')
        lines.append('};')
    for i,r in enumerate(pmetal):
        b=r['code']; lines.append(f'static const u8 g_ul_pmetal_blob_{i}[{len(b)}]={{')
        for j in range(0,len(b),32): lines.append(','.join(str(x) for x in b[j:j+32])+',')
        lines.append('};')
    lines.append(f'static const ULBlob g_ul_stock_blobs[{len(stock)}]={{')
    for i,r in enumerate(stock): lines.append(f'{{{r["shader_index"]}u,g_ul_stock_blob_{i},{len(r["code"])}u}},')
    lines.append('};')
    lines.append(f'static const ULBlob g_ul_pmetal_blobs[{len(pmetal)}]={{')
    for i,r in enumerate(pmetal): lines.append(f'{{{r["shader_index"]}u,g_ul_pmetal_blob_{i},{len(r["code"])}u}},')
    lines.append('};')
    out.write_text('\n'.join(lines)+'\n')

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--binder',type=Path,required=True)
    ap.add_argument('--shipping-145',type=Path,required=True)
    ap.add_argument('--hosts',type=Path,default=Path(__file__).with_name('UL48_CANONICAL_HOSTS.json'))
    ap.add_argument('--out-header',type=Path,required=True)
    ap.add_argument('--out-audit',type=Path,required=True)
    a=ap.parse_args()
    release=a.shipping_145.read_bytes()
    if sha256(release)!=SHIPPING_145_SHA256: raise SystemExit('shipping 1.45 SHA mismatch')
    rows=read_shader_binder(a.binder)
    byhash={}
    for r in rows: byhash.setdefault(sha256(r['data']),[]).append(r)
    hosts=json.loads(a.hosts.read_text())['records']
    stock=[]
    for h in hosts:
        candidates=byhash.get(h['original_sha256'],[])
        row=next((r for r in candidates if r['name']==h['name']),None)
        if row is None: raise SystemExit(f'missing certified host name/hash pair {h["name"]}')
        ul,audit=patch_ul(row['data'],row['name'])
        stock.append({'shader_index':row['id'],'name':row['name'],'original_sha256':h['original_sha256'],'ul_sha256':sha256(ul),'code':ul,'audit':audit})
    hits=extract_embedded_dxbc(release)
    pmetal=[]
    for embedded_index,shader_index in zip(PMETAL_ALT_INDICES,PMETAL_SHADER_INDICES):
        if embedded_index>=len(hits): raise SystemExit('not enough embedded DXBC in shipping 1.45')
        base=hits[embedded_index]; ul,audit=patch_ul(base,f'shipping145_alt{embedded_index}')
        pmetal.append({'embedded_index':embedded_index,'shader_index':shader_index,'base_sha256':sha256(base),'ul_sha256':sha256(ul),'code':ul,'audit':audit})
    a.out_header.parent.mkdir(parents=True,exist_ok=True); a.out_audit.parent.mkdir(parents=True,exist_ok=True)
    emit_header(stock,pmetal,a.out_header)
    audit={'status':'PASS','shipping_145_sha256':SHIPPING_145_SHA256,'stock_ul48_count':len(stock),'pmetal_count':len(pmetal),'stock':[ {k:v for k,v in r.items() if k!='code'} for r in stock ],'pmetal':[ {k:v for k,v in r.items() if k!='code'} for r in pmetal ]}
    a.out_audit.write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({'status':'PASS','stock':len(stock),'pmetal':len(pmetal),'header_sha256':sha256(a.out_header.read_bytes()),'audit_sha256':sha256(a.out_audit.read_bytes())},indent=2))
if __name__=='__main__': main()
