from pathlib import Path
import sys,json,hashlib,struct
sys.path.insert(0,'/mnt/data/ul_successor/scripts')
from generate_port_plans import parse_bnd3
from build_ul48_live import patch_ul
ROOT=Path('/mnt/data/dsrrl_mr_expanded_a3')
ROOT.mkdir(exist_ok=True); (ROOT/'src').mkdir(exist_ok=True)
BINDER=Path('/mnt/data/vanilla_FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx')
HOSTS=json.load(open('/mnt/data/ul_successor/data/UL48_CANONICAL_HOSTS.json'))['records']
import zlib
_dcxb=BINDER.read_bytes()
if not _dcxb.startswith(b'DCX\x00'):
    raise RuntimeError('expected DCX binder')
_raw=zlib.decompress(_dcxb[76:])
rows=parse_bnd3(_raw); byhash={hashlib.sha256(r['data']).hexdigest():r for r in rows}
recs=[]; blobs=[]
for idx,h in enumerate(HOSTS):
    row=byhash[h['original_sha256']]
    orig=row['data']
    ul,a=patch_ul(orig,h['name'])
    blobs.append(ul); recs.append({**h,'id':row['id'],'ul_sha256':hashlib.sha256(ul).hexdigest(),'ul_size':len(ul),**a})
# extract exact 1.45 alternates 33/34/35 and patch only U/L
release=Path('/mnt/data/dsrrl_mr_expanded_a1/package/DSRRL_Material_Response_1.45.addon64').read_bytes()
hits=[]; pos=0
while True:
    i=release.find(b'DXBC',pos)
    if i<0: break
    if i+32<=len(release):
        size=struct.unpack_from('<I',release,i+24)[0]; n=struct.unpack_from('<I',release,i+28)[0]
        if 32<=size<=100000 and i+size<=len(release) and n<64:
            ok=i+32+4*n<=i+size
            if ok:
                for j in range(n):
                    off=struct.unpack_from('<I',release,i+32+4*j)[0]
                    if off+8>size: ok=False; break
                    cs=struct.unpack_from('<I',release,i+off+4)[0]
                    if off+8+cs>size: ok=False; break
            if ok:hits.append(release[i:i+size])
    pos=i+4
pm=[]
for idx,shader_index in zip((33,34,35),(894,913,932)):
    base=hits[idx]; ul,a=patch_ul(base,f'release145_alt{idx}')
    pm.append({'embedded_index':idx,'shader_index':shader_index,'base_sha256':hashlib.sha256(base).hexdigest(),'ul_sha256':hashlib.sha256(ul).hexdigest(),'code':ul,'audit':a})
# emit header
lines=['#pragma once','typedef unsigned char u8; typedef unsigned int u32; typedef unsigned long long u64;','struct ULBlob { u32 shader_index; const u8 *code; u32 size; };']
for i,b in enumerate(blobs):
    lines.append(f'static const u8 g_ul_stock_blob_{i}[{len(b)}]={{')
    for j in range(0,len(b),32): lines.append(','.join(str(x) for x in b[j:j+32])+',')
    lines.append('};')
for i,r in enumerate(pm):
    b=r['code']; lines.append(f'static const u8 g_ul_pmetal_blob_{i}[{len(b)}]={{')
    for j in range(0,len(b),32): lines.append(','.join(str(x) for x in b[j:j+32])+',')
    lines.append('};')
lines.append('static const ULBlob g_ul_stock_blobs[48]={')
for i,r in enumerate(recs): lines.append(f'{{{r["id"]}u,g_ul_stock_blob_{i},{r["ul_size"]}u}},')
lines.append('};')
lines.append('static const ULBlob g_ul_pmetal_blobs[3]={')
for i,r in enumerate(pm): lines.append(f'{{{r["shader_index"]}u,g_ul_pmetal_blob_{i},{len(r["code"])}u}},')
lines.append('};')
(Path(ROOT/'src/generated_ul_payloads.hpp')).write_text('\n'.join(lines))
# compact audit
for r in pm: r.pop('code')
audit={'stock_ul48_count':48,'stock_base':'VANILLA_DSR_UL_ONLY','pmetal_count':3,'pmetal_base':'SHIPPING_1_45_ALTERNATES_33_34_35_UL_ONLY','pmetal':pm,'stock':[{'id':r['id'],'name':r['name'],'original_sha256':r['original_sha256'],'ul_sha256':r['ul_sha256'],'ul_size':r['ul_size']} for r in recs]}
(Path(ROOT/'UL_PAYLOAD_AUDIT.json')).write_text(json.dumps(audit,indent=2))
print('stock',len(recs),'pmetal',len(pm),'header', (ROOT/'src/generated_ul_payloads.hpp').stat().st_size)
