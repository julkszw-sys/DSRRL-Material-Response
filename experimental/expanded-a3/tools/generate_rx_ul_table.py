#!/usr/bin/env python3
import struct,hashlib,json,sys
from pathlib import Path
sys.path.insert(0,'/mnt/data/dsrrl_mr_expanded_a3/repro')
import generate_ul_payloads as g
BIN=Path('/mnt/data/dsrrl_mr_expanded_a3/package/DSRRL_Material_Response_1.45.addon64')
BINDER=Path('/mnt/data/vanilla_FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx')
HOSTS=Path('/mnt/data/ul_successor/data/UL48_CANONICAL_HOSTS.json')
OUT=Path('/mnt/data/dsrrl_mr_expanded_a3/monolith/src/a3_ul_rx.hpp')
AUD=Path('/mnt/data/dsrrl_mr_expanded_a3/monolith/out/A3_UL_RX_AUDIT.json')

d=BIN.read_bytes()
lf=struct.unpack_from('<I',d,0x3c)[0]; ns=struct.unpack_from('<H',d,lf+6)[0]; op=struct.unpack_from('<H',d,lf+20)[0]; sec=lf+24+op
sects=[]
for i in range(ns):
 o=sec+40*i; name=d[o:o+8].split(b'\0')[0].decode(); vs,va,rs,rp=struct.unpack_from('<IIII',d,o+8); sects.append((name,va,vs,rs,rp))
def rvaoff(rva):
 for name,va,vs,rs,rp in sects:
  if va<=rva<va+max(vs,rs): return rp+(rva-va)
 raise ValueError(hex(rva))
def va_bytes(va,n): return d[rvaoff(va-0x180000000):rvaoff(va-0x180000000)+n]
rows=g.read_shader_binder(BINDER); byhash={hashlib.sha256(r['data']).hexdigest():r for r in rows}
hosts=json.load(open(HOSTS))['records']
stock=[]
for i,h in enumerate(hosts[:47]):
 r=byhash[h['original_sha256']]; u,a=g.patch_ul(r['data'],r['name'])
 stock.append({'rx':i,'name':r['name'],'base_sha':h['original_sha256'],'ul':u,'ul_sha':hashlib.sha256(u).hexdigest()})
meta_rva=0x189e14
recs=[]
for rx in range(47):
 off=rvaoff(meta_rva+rx*0x38)
 code_va=struct.unpack_from('<Q',d,off+4)[0]; code_sz=struct.unpack_from('<I',d,off+12)[0]
 alt=None
 if code_va and code_sz:
  b=va_bytes(code_va,code_sz); u,a=g.patch_ul(b,f'rx{rx}_shipping145_alt')
  alt={'base_sha':hashlib.sha256(b).hexdigest(),'base_size':len(b),'ul':u,'ul_sha':hashlib.sha256(u).hexdigest()}
 recs.append({'rx':rx,'stock':stock[rx],'alt':alt})
blob=bytearray()
def addblob(b):
 while len(blob)%16: blob.append(0)
 off=len(blob); blob.extend(b); return off,len(b)
for r in recs:
 r['stock_off'],r['stock_size']=addblob(r['stock']['ul'])
 if r['alt']:
  r['alt_off'],r['alt_size']=addblob(r['alt']['ul'])
 else:r['alt_off']=r['alt_size']=0
lines=['#pragma once','#include <stdint.h>','struct A3ULRxRec { uint32_t stock_off,stock_size,alt_off,alt_size; };',f'static const uint32_t a3_ul_rx_count={len(recs)}u;',f'static const uint32_t a3_ul_rx_blob_size={len(blob)}u;',f'static const uint8_t a3_ul_rx_blob[{len(blob)}]={{']
for j in range(0,len(blob),48):lines.append(','.join(str(x) for x in blob[j:j+48])+',')
lines+=['};',f'static const A3ULRxRec a3_ul_rx_recs[{len(recs)}]={{']
for r in recs:lines.append(f'{{{r["stock_off"]}u,{r["stock_size"]}u,{r["alt_off"]}u,{r["alt_size"]}u}},')
lines.append('};')
OUT.write_text('\n'.join(lines)+'\n')
audit={'release_sha256':hashlib.sha256(d).hexdigest(),'metadata_rva':hex(meta_rva),'receiver_count':47,'alt_count':sum(bool(r['alt']) for r in recs),'blob_size':len(blob),'records':[]}
for r in recs:
 audit['records'].append({'rx':r['rx'],'name':r['stock']['name'],'stock_base_sha256':r['stock']['base_sha'],'stock_ul_sha256':r['stock']['ul_sha'],'alt_base_sha256':r['alt']['base_sha'] if r['alt'] else None,'alt_ul_sha256':r['alt']['ul_sha'] if r['alt'] else None})
AUD.write_text(json.dumps(audit,indent=2)+'\n')
print(json.dumps({'count':47,'alts':audit['alt_count'],'blob_size':len(blob),'header_sha256':hashlib.sha256(OUT.read_bytes()).hexdigest(),'audit_sha256':hashlib.sha256(AUD.read_bytes()).hexdigest()},indent=2))