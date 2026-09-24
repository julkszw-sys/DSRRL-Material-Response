#!/usr/bin/env python3
"""Byte-exact reconstructed generators for the historical V2.9/V2.10/V2.11
Material Response generated DXBC headers.

Inputs are source/provenance artifacts, never historical generated headers:
  * vanilla DSR FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx
    SHA256 ad180732ac79d5d98783aa504c789c2bab15e515c3b0f66b237d8b8c69113394
  * V29_SHADER_MANIFEST.json (original and expected replacement hashes)
  * V210_C101_MATERIAL_RESPONSE_AUDIT.json (semantic word sites / expected hashes)
  * V211_C101_UNBOUNDED_AUDIT.json (MUL_SAT word sites / expected hashes)

The V2.9 sites below are a recovered, SHA-guarded semantic patch manifest. They
encode the canonical two gFC_DifMapMulCol cb0[9] operand coordinates and the
canonical diffuse pow(2.2) literal triple in each of the 24 stable HemEnv hosts.
No target generated header is used as an input.
"""
from pathlib import Path
import argparse, hashlib, json, re, struct, zlib

MASK=0xffffffff
EXPECTED_BINDER_DCX='ad180732ac79d5d98783aa504c789c2bab15e515c3b0f66b237d8b8c69113394'
EXPECTED_V29_HEADER='1458180458dddd6dada0e1af7aa83543cd8c77c4fbf51bf146b0adcdeb221a20'
EXPECTED_V210_C101_HEADER='97e0788c4fb90d305b063851b4bef87136c57303aea765bd3e4787a2619b8c76'
EXPECTED_V211_C101_HEADER='5f5bb82fffce98b37d87021e3a10829eb2b189e66b4818c83fe8d82947cfcfd3'

# stable_index: ((cb0[9] pair starts), diffuse pow triple start) in STOCK SHEX words.
# Each cb site patches only the two index dwords immediately following its unchanged operand token.
V29_SITES={
0:((1527,1617),1631),1:((1436,1526),1540),2:((1092,1182),1196),3:((1465,1555),1569),
4:((1374,1464),1478),5:((1034,1124),1138),6:((1254,1344),1358),7:((1163,1253),1267),
8:((819,909),923),9:((1195,1285),1299),10:((1104,1194),1208),11:((764,854),868),
12:((1083,1173),1187),13:((992,1082),1096),14:((648,738),752),15:((1021,1111),1125),
16:((930,1020),1034),17:((590,680),694),18:((997,1087),1101),19:((906,996),1010),
20:((562,652),666),21:((938,1028),1042),22:((847,937),951),23:((507,597),611),
}
CB12_DECL=[0x04000059,0x00208e46,0x0000000c,0x00000004]

# Microsoft DXBC checksum (MD5-derived variant used by D3D bytecode containers).
def rol(x,n): return ((x<<n)|(x>>(32-n))) & MASK
def F(x,y,z): return ((x&y)|((~x)&z)) & MASK
def G(x,y,z): return ((x&z)|(y&(~z))) & MASK
def H(x,y,z): return (x^y^z) & MASK
def I(x,y,z): return (y^(x|(~z))) & MASK
OPS=[
(F,0,7,3614090360),(F,1,12,3905402710),(F,2,17,606105819),(F,3,22,3250441966),(F,4,7,4118548399),(F,5,12,1200080426),(F,6,17,2821735955),(F,7,22,4249261313),(F,8,7,1770035416),(F,9,12,2336552879),(F,10,17,4294925233),(F,11,22,2304563134),(F,12,7,1804603682),(F,13,12,4254626195),(F,14,17,2792965006),(F,15,22,1236535329),
(G,1,5,4129170786),(G,6,9,3225465664),(G,11,14,643717713),(G,0,20,3921069994),(G,5,5,3593408605),(G,10,9,38016083),(G,15,14,3634488961),(G,4,20,3889429448),(G,9,5,568446438),(G,14,9,3275163606),(G,3,14,4107603335),(G,8,20,1163531501),(G,13,5,2850285829),(G,2,9,4243563512),(G,7,14,1735328473),(G,12,20,2368359562),
(H,5,4,4294588738),(H,8,11,2272392833),(H,11,16,1839030562),(H,14,23,4259657740),(H,1,4,2763975236),(H,4,11,1272893353),(H,7,16,4139469664),(H,10,23,3200236656),(H,13,4,681279174),(H,0,11,3936430074),(H,3,16,3572445317),(H,6,23,76029189),(H,9,4,3654602809),(H,12,11,3873151461),(H,15,16,530742520),(H,2,23,3299628645),
(I,0,6,4096336452),(I,7,10,1126891415),(I,14,15,2878612391),(I,5,21,4237533241),(I,12,6,1700485571),(I,3,10,2399980690),(I,10,15,4293915773),(I,1,21,2240044497),(I,8,6,1873313359),(I,15,10,4264355552),(I,6,15,2734768916),(I,13,21,1309151649),(I,4,6,4149444226),(I,11,10,3174756917),(I,2,15,718787259),(I,9,21,3951481745)]
def transform(buf,X):
    a,b,c,d=buf
    def step(fn,a,b,c,d,x,s,ac):
        a=(a+fn(b,c,d)+x+ac)&MASK; a=rol(a,s); return (a+b)&MASK
    for j,(fn,k,s,ac) in enumerate(OPS):
        m=j&3
        if m==0:a=step(fn,a,b,c,d,X[k],s,ac)
        elif m==1:d=step(fn,d,a,b,c,X[k],s,ac)
        elif m==2:c=step(fn,c,d,a,b,X[k],s,ac)
        else:b=step(fn,b,c,d,a,X[k],s,ac)
    return [(buf[0]+a)&MASK,(buf[1]+b)&MASK,(buf[2]+c)&MASK,(buf[3]+d)&MASK]
def dxbc_checksum(data):
    p=data[0x14:]; nbits=(len(p)*8)&MASK; buf=[0x67452301,0xefcdab89,0x98badcfe,0x10325476]; full=len(p)&~63
    for o in range(0,full,64): buf=transform(buf,list(struct.unpack('<16I',p[o:o+64])))
    last=p[full:]
    if len(last)>=56:
        block=last+b'\x80'+b'\x00'*(63-len(last)); buf=transform(buf,list(struct.unpack('<16I',block))); X=[0]*16; X[0]=nbits; X[15]=((nbits>>2)|1)&MASK; buf=transform(buf,X)
    else:
        block=bytearray(struct.pack('<I',nbits)+last); block+=b'\x80'+b'\x00'*(64-len(block)-1); struct.pack_into('<I',block,60,((nbits>>2)|1)&MASK); buf=transform(buf,list(struct.unpack('<16I',block)))
    return struct.pack('<4I',*buf)

def sha(b): return hashlib.sha256(b).hexdigest()
def parse_dxbc(d):
    assert d[:4]==b'DXBC'
    total=struct.unpack_from('<I',d,24)[0]; assert total==len(d)
    n=struct.unpack_from('<I',d,28)[0]; offs=list(struct.unpack_from('<'+'I'*n,d,32))
    chunks=[]
    for o in offs:
        tag=d[o:o+4]; sz=struct.unpack_from('<I',d,o+4)[0]; chunks.append((tag,d[o+8:o+8+sz]))
    return chunks

def rebuild_dxbc(d, new_shex_payload):
    chunks=parse_dxbc(d)
    out=bytearray(d[:32+4*len(chunks)])
    offs=[]
    for tag,payload in chunks:
        if tag==b'SHEX': payload=new_shex_payload
        offs.append(len(out)); out += tag+struct.pack('<I',len(payload))+payload
    struct.pack_into('<I',out,24,len(out)); struct.pack_into('<I',out,28,len(chunks))
    struct.pack_into('<'+'I'*len(offs),out,32,*offs)
    out[4:20]=b'\0'*16; out[4:20]=dxbc_checksum(bytes(out))
    assert out[4:20]==dxbc_checksum(bytes(out))
    return bytes(out)

def get_shex_words(d):
    for tag,payload in parse_dxbc(d):
        if tag==b'SHEX': return list(struct.unpack('<'+'I'*(len(payload)//4),payload))
    raise AssertionError('SHEX missing')

def patch_shex_same_size(d, patches):
    words=get_shex_words(d)
    for wi,old,new in patches:
        assert words[wi]==old,(wi,hex(words[wi]),hex(old)); words[wi]=new
    payload=struct.pack('<'+'I'*len(words),*words)
    return rebuild_dxbc(d,payload)

def patch_v29(stock,idx):
    words=get_shex_words(stock)
    cb_sites,pow_site=V29_SITES[idx]
    assert words[1]==len(words), (idx,words[1],len(words))
    # Verify semantic sites before mutation.
    for s in cb_sites: assert words[s:s+2]==[0,9],(idx,s,words[s:s+2])
    assert words[pow_site:pow_site+3]==[0x400ccccd]*3,(idx,pow_site)
    # Sites are stock coordinates; patch first, then insert declaration.
    for s in cb_sites: words[s:s+2]=[12,1]
    words[pow_site:pow_site+3]=[0x3f800000]*3
    words[11:11]=CB12_DECL
    words[1]+=4
    assert words[1]==len(words)
    return rebuild_dxbc(stock,struct.pack('<'+'I'*len(words),*words))

def extract_originals_from_binder(dcx_path, records):
    raw=Path(dcx_path).read_bytes(); assert sha(raw)==EXPECTED_BINDER_DCX,(sha(raw),EXPECTED_BINDER_DCX)
    # DSR DCX DFLT has a zlib stream at 0x4c; scan defensively if layout differs.
    dec=None
    for off in range(0, min(0x200,len(raw)-2)):
        if raw[off] == 0x78:
            try:
                cand=zlib.decompress(raw[off:])
                if b'DXBC' in cand and len(cand)>1_000_000: dec=cand; break
            except Exception: pass
    assert dec is not None,'could not decompress DSR binder DCX'
    wanted={r['original_sha256'] for r in records}
    found={}; pos=0
    while True:
        p=dec.find(b'DXBC',pos)
        if p<0: break
        if p+32<=len(dec):
            total=struct.unpack_from('<I',dec,p+24)[0]
            if 1000 < total < 100000 and p+total<=len(dec):
                blob=dec[p:p+total]; h=sha(blob)
                if h in wanted: found[h]=blob
        pos=p+4
    assert set(found)==wanted,(len(found),len(wanted),sorted(wanted-set(found))[:3])
    return found

def byte_lines(blob):
    vals=list(blob); return ''.join('  '+','.join(map(str,vals[i:i+24]))+',\n' for i in range(0,len(vals),24))
def render_diffuse(blobs,records):
    s='#pragma once\n#include <array>\n#include <cstddef>\n#include <cstdint>\n#include <string_view>\nnamespace dsrrl::diffusemr {\n'
    s+='struct plan { std::string_view label; std::string_view original_sha256; bool lerp; int stable_index; const std::uint8_t *code; std::size_t code_size; };\n'
    for i,b in enumerate(blobs):
        s+=f'inline constexpr std::array<std::uint8_t,{len(b)}> blob_{i} = {{\n'+byte_lines(b)+'};\n'
    s+=f'inline constexpr std::array<plan,{len(records)}> k_plans = {{{{\n'
    for r in records:
        if r['lerp']: code='nullptr,0'
        else: code=f"blob_{r['stable_index']}.data(),blob_{r['stable_index']}.size()"
        s+=f'  {{"{r["label"]}","{r["original_sha256"]}",{str(r["lerp"]).lower()},{r["stable_index"]},{code}}},\n'
    s+='}};\ninline int find_original(std::string_view h) noexcept { for(std::size_t i=0;i<k_plans.size();++i) if(k_plans[i].original_sha256==h) return static_cast<int>(i); return -1; }\n}\n'
    return s.encode()
def render_c101(blobs):
    s='#pragma once\n#include <array>\n#include <cstddef>\n#include <cstdint>\nnamespace dsrrl::c101mr {\n'
    for i,b in enumerate(blobs): s+=f'inline constexpr std::array<std::uint8_t,{len(b)}> blob_{i} = {{\n'+byte_lines(b)+'};\n'
    s+='struct code_ref { const std::uint8_t *code; std::size_t size; };\ninline constexpr std::array<code_ref,24> k_code = {{\n'
    for i in range(24): s+=f'  {{blob_{i}.data(),blob_{i}.size()}},\n'
    s+='}};\n}\n'; return s.encode()

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--binder-dcx',required=True); ap.add_argument('--v29-manifest',required=True); ap.add_argument('--v210-audit',required=True); ap.add_argument('--v211-audit',required=True); ap.add_argument('--out-dir',required=True); a=ap.parse_args()
    records=json.loads(Path(a.v29_manifest).read_text())['records']; originals=extract_originals_from_binder(a.binder_dcx,records)
    stable=sorted((r for r in records if not r['lerp']),key=lambda r:r['stable_index'])
    v29=[]
    for r in stable:
        b=patch_v29(originals[r['original_sha256']],r['stable_index']); assert sha(b)==r['replacement_sha256'],(r['stable_index'],sha(b),r['replacement_sha256']); v29.append(b)
    v29h=render_diffuse(v29,records); assert sha(v29h)==EXPECTED_V29_HEADER,(sha(v29h),EXPECTED_V29_HEADER)

    a210=json.loads(Path(a.v210_audit).read_text())['shader']['hosts']; assert len(a210)==24
    v210=[]
    for h in a210:
        i=h['index']; assert sha(v29[i])==h['source_sha256']
        patches=[(x['word'],int(x['old'],16),int(x['new'],16)) for x in h['shex_patch_dwords']]
        b=patch_shex_same_size(v29[i],patches); assert sha(b)==h['c101_sha256'],(i,sha(b),h['c101_sha256']); v210.append(b)
    v210h=render_c101(v210); assert sha(v210h)==EXPECTED_V210_C101_HEADER,(sha(v210h),EXPECTED_V210_C101_HEADER)

    a211=json.loads(Path(a.v211_audit).read_text())['hosts']; assert len(a211)==24
    v211=[]
    for h in a211:
        i=h['index']; assert sha(v210[i])==h['source_v210_sha256']
        b=patch_shex_same_size(v210[i],[(h['chain_mul_word'],int(h['old_instruction_token'],16),int(h['new_instruction_token'],16))])
        assert sha(b)==h['v211_sha256'],(i,sha(b),h['v211_sha256']); v211.append(b)
    v211h=render_c101(v211); assert sha(v211h)==EXPECTED_V211_C101_HEADER,(sha(v211h),EXPECTED_V211_C101_HEADER)
    od=Path(a.out_dir); od.mkdir(parents=True,exist_ok=True)
    (od/'generated_ptde_diffuse_material_response.hpp').write_bytes(v29h)
    (od/'generated_ptde_c101_material_response_v210.hpp').write_bytes(v210h)
    (od/'generated_ptde_c101_material_response.hpp').write_bytes(v211h)
    print(json.dumps({'status':'PASS','binder_dcx_sha256':EXPECTED_BINDER_DCX,'v29_header_sha256':sha(v29h),'v210_c101_header_sha256':sha(v210h),'v211_c101_header_sha256':sha(v211h),'stable_hosts':24},indent=2))
if __name__=='__main__': main()
