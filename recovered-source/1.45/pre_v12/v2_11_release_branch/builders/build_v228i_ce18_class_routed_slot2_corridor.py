import struct, hashlib, json, zipfile, textwrap, importlib.util
from pathlib import Path

ROOT=Path('/mnt/data')
BASE=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_28G_CE18_EXACT_C101_TRANSPORT_COLOR0_LINEAR_CARRIER_NO_ANGULAR_NO_T9_DYNAMIC_LOD.addon64'
MAT=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_28G_CE18_EXACT_C101_TRANSPORT_COLOR0_LINEAR_CARRIER_NO_ANGULAR_NO_T9_DYNAMIC_LOD_MATERIALS.json'
base=BASE.read_bytes(); mats=json.loads(MAT.read_text(encoding='utf8'))
sp=importlib.util.spec_from_file_location('ck',ROOT/'test_dxbc_ck.py'); ck=importlib.util.module_from_spec(sp); sp.loader.exec_module(ck)
NOP=0x0100003a
REC_OFF=0x81280; REC_STRIDE=0x48; HASH_OFF=0x87a00; HASH_STRIDE=0x50; COUNT=368
TARGETS={894:33,913:34,932:35}


def chunks(d):
    assert d[:4]==b'DXBC'; total=struct.unpack_from('<I',d,24)[0]; assert total==len(d)
    n=struct.unpack_from('<I',d,28)[0]; offs=struct.unpack_from('<'+'I'*n,d,32); out={}
    for o in offs:
        tag=d[o:o+4].decode(); sz=struct.unpack_from('<I',d,o+4)[0]
        out[tag]=(o,o+8,sz,d[o+8:o+8+sz])
    return out

def words(d):
    c=chunks(d); k='SHEX' if 'SHEX' in c else 'SHDR'; _,p,sz,blob=c[k]
    return k,p,sz,list(struct.unpack('<'+'I'*(sz//4),blob))

def write_words(d,w):
    c=chunks(d); k='SHEX' if 'SHEX' in c else 'SHDR'; _,p,sz,_=c[k]
    q=bytearray(d); q[p:p+sz]=struct.pack('<'+'I'*len(w),*w)
    q[4:20]=b'\0'*16; q[4:20]=ck.dxbc_checksum(bytes(q)); assert q[4:20]==ck.dxbc_checksum(bytes(q))
    return bytes(q)

def instrs(w):
    i=2; out=[]
    while i<len(w):
        tok=w[i]; ln=(tok>>24)&0x7f; op=tok&0x7ff
        assert ln>0,(i,hex(tok)); out.append((i,op,ln,w[i:i+ln])); i+=ln
    return out

def scan_dxbc(blob):
    es=[]; q=0
    while True:
        q=blob.find(b'DXBC',q)
        if q<0: break
        if q+28<=len(blob):
            sz=struct.unpack_from('<I',blob,q+24)[0]
            if 1000<sz<100000 and q+sz<=len(blob):
                d=blob[q:q+sz]
                try: chunks(d); es.append((q,sz))
                except: pass
        q+=4
    return es

def pe_sections(blob):
    e=struct.unpack_from('<I',blob,0x3c)[0]; n=struct.unpack_from('<H',blob,e+6)[0]; osz=struct.unpack_from('<H',blob,e+20)[0]; st=e+24+osz; out=[]
    for i in range(n):
        q=st+i*40; name=blob[q:q+8].rstrip(b'\0').decode(errors='replace'); vs,va,rs,raw=struct.unpack_from('<IIII',blob,q+8); out.append((name,va,raw,rs,vs))
    return out

def va_to_file(blob, rva):
    for name,va,raw,rs,vs in pe_sections(blob):
        if va <= rva < va+max(rs,vs): return raw+(rva-va)
    raise ValueError(hex(rva))

def patch_class_bounds_uploader(buf):
    b=bytearray(buf)
    # Existing runtime uploader builds b12 at 0x1800078da..; replace only cb12[0].w and cb12[1].w producers.
    # cb12[0].w: instead of bool(alt flag), load donor +0x3C = LOD low bound.
    off=va_to_file(buf,0x7906)
    old=b[off:off+16]
    assert old.hex()=='385cc144c745f30000803f7503895df3',old.hex()
    new=bytes.fromhex('f30f1044c13c' 'f30f1145f3') + b'\x90'*5
    assert len(new)==16
    b[off:off+16]=new
    # cb12[1].w: load donor +0x40 = LOD high bound. cb12[2]/[3] are unused by this branch, so the old zeroing block can be retired.
    off2=va_to_file(buf,0x7937)
    old2=b[off2:off2+21]
    assert old2.hex()=='c745030000803f0f57c90f294d070f57c00f294517',old2.hex()
    new2=bytes.fromhex('f30f1044c140' 'f30f114503') + b'\x90'*10
    assert len(new2)==21
    b[off2:off2+21]=new2
    return bytes(b),{
        'uploader_cb12_0_w':'donor record +0x3C -> cb12[0].w (LOD low)',
        'uploader_cb12_1_w':'donor record +0x40 -> cb12[1].w (LOD high)',
        'note':'cb12[2]/cb12[3] remain unused by all V2.28I alternate shaders; their former explicit zeroing is removed to keep a length-preserving .text patch.'
    }

def patch_dynamic_lod_bounds(d):
    k,p,sz,w=words(d); ins=instrs(w)
    candidates=[]
    for idx,(i,op,ln,ww) in enumerate(ins):
        if op==72 and ln==13 and ww[7:11]==[0x00107936,0x0000000c,0x00106000,0x0000000c] and ww[11:13]==[0x0010003a,0x00000002]:
            candidates.append((idx,i,ww))
    assert len(candidates)==1,candidates
    idx,samp_i,samp=candidates[0]
    a0=ins[idx-5]; lg=ins[idx-4]; md=ins[idx-3]; a1=ins[idx-2]; amp=ins[idx-1]
    assert (a0[1],a0[2],lg[1],lg[2],md[1],md[2],a1[1],a1[2],amp[1],amp[2])==(0,8,47,5,50,9,0,7,56,9)
    assert a0[0]+8==lg[0] and lg[0]+5==md[0] and md[0]+9==a1[0] and a1[0]+7==amp[0] and amp[0]+9==samp_i
    add0=a0[3]; logw=lg[3]; madw=md[3]; add1=a1[3]; ampw=amp[3]
    # Compact exact stock LOD reconstruction:
    #   LOD_D = 1.2*log2(rho) + cb79.w - 3
    direct_mad=[0x0A000032] + madw[1:7] + add0[5:8]
    add_minus3=[0x07000000] + add1[1:5] + [0x00004001,0xC0400000]
    # Per-material legal corridor supplied by runtime sidecar:
    #   low = cb12[0].w ; high = cb12[1].w
    #   LOD_I = min(max(LOD_D,low),high)
    maxlow=[0x08000034] + add1[1:5] + [0x0020803a,0x0000000c,0x00000000]
    minhigh=[0x08000033] + add1[1:5] + [0x0020803a,0x0000000c,0x00000001]
    repl=logw+direct_mad+add_minus3+maxlow+minhigh
    start=a0[0]; end=samp_i
    assert len(repl)==end-start==38,(len(repl),end-start)
    w[start:end]=repl
    # Relocate source-amplitude multiplies into the already-dead angular block exactly as V2.28H.
    sm=ins[idx+1]; dp3=ins[idx+2]; angmad=ins[idx+3]; mov1=ins[idx+4]; n1=ins[idx+5]; n2=ins[idx+6]; angmul=ins[idx+7]
    assert (sm[1],sm[2],dp3[1],dp3[2],angmad[1],angmad[2],mov1[1],mov1[2],n1[1],n1[2],n2[1],n2[2],angmul[1],angmul[2])==(56,7,16,7,50,9,54,5,58,1,58,1,56,7)
    w[sm[0]:sm[0]+7]=[NOP]*7
    w[dp3[0]:angmad[0]+9]=ampw+sm[3]
    w[mov1[0]:angmul[0]+7]=[NOP]*(angmul[0]+7-mov1[0])
    nd=write_words(d,w)
    _,_,_,nw=words(nd); ni=instrs(nw)
    t12=[x for x in ni if x[1]==72 and x[2]==13 and x[3][7:11]==[0x00107936,0x0000000c,0x00106000,0x0000000c]]
    assert len(t12)==1 and t12[0][3][11:13]==[0x0010003a,0x00000002]
    # exact MAX/MIN sidecar operands
    assert any(i==start+22 and op==52 and ww[-3:]==[0x0020803a,0x0000000c,0] for i,op,ln,ww in ni)
    assert any(i==start+30 and op==51 and ww[-3:]==[0x0020803a,0x0000000c,1] for i,op,ln,ww in ni)
    return nd,{
        'lod_block_start_word':start,'t12_sample_word':samp_i,
        'equation':'LOD_I=min(max(cb79.w-3+1.2*log2(rho), cb12[0].w), cb12[1].w)',
        'bounds_source':'per exact CE18 donor record, uploaded through b12 .w lanes',
        'amplitude_relocation':'cb4*cb79.x and cube*amplitude MUL relocated after sample into dead angular block; math preserved; angular remains bypassed.'
    }

# 1) CPU sidecar transport for per-material LOD corridor bounds.
transported,transport_meta=patch_class_bounds_uploader(base)
b=bytearray(transported)

# 2) Patch the three stable opaque alternate shaders to consume per-material bounds.
entries=scan_dxbc(transported); assert len(entries)==48
patched=[]
for shader,slot in TARGETS.items():
    q,sz=entries[slot]; old=bytes(b[q:q+sz]); nd,meta=patch_dynamic_lod_bounds(old); b[q:q+sz]=nd
    patched.append({'shader_index':shader,'alt_slot':slot,'offset':hex(q),'size':sz,**meta,'before_sha256':hashlib.sha256(old).hexdigest(),'after_sha256':hashlib.sha256(nd).hexdigest()})

# 3) Exact identity gate stays CE18-only for all 18. Bounds are class-resolved by exact PTDE donor slot.
# V2.28I deliberately changes only slot2 resource shape. Other classes use [0,7], which is the native 8-mip resource's effective legal sample range,
# so their stock fractional LOD remains untouched while they gain the exact per-material legacy material tail already validated structurally in V2.28G.
embedded={}
for i in range(COUNT):
    h=base[HASH_OFF+i*HASH_STRIDE:HASH_OFF+i*HASH_STRIDE+64].decode('ascii',errors='ignore')
    if len(h)==64: embedded[h]=i
rows=[]
for r in mats['records']:
    i=embedded[r['sha256']]; o=REC_OFF+i*REC_STRIDE
    slot=r['ptde_envspc_slot']
    low,high=(3.0,4.0) if slot==2 else (0.0,7.0)
    # Repurpose donor c102/slot fields only inside this diagnostic sidecar transport. They are not uploaded/consumed by this no-PointLight broad EnvSpec branch otherwise.
    old_c102=struct.unpack_from('<f',base,o+0x3c)[0]; old_slot=struct.unpack_from('<I',base,o+0x40)[0]
    assert old_slot==slot
    struct.pack_into('<f',b,o+0x3c,low)
    struct.pack_into('<f',b,o+0x40,high)
    struct.pack_into('<I',b,o+0x44,1)
    rows.append({'name':r['name'],'sha256':r['sha256'],'record_index':i,'ptde_envspc_slot':slot,'ptde_c101_exact':r['ptde_c101_exact'],
                 'ptde_c102_preserved_in_audit_only':old_c102,'lod_low':low,'lod_high':high,
                 'response':'exact CE18 c100 + linear SPEC carrier + exact PTDE c101 + native DSR COLOR0; angular/t9 bypass',
                 'lod_policy':'slot2 evidence corridor [3,4]' if slot==2 else 'stock effective DSR 8-mip range [0,7] (no class LOD projection yet)'})

# Banner update.
oldmsg=b'PTDE Material Response V2.28G active: CE18 exact c101 transport + native COLOR0 on linear carrier; no angular/t9; dynamic LOD.\0'
pos=bytes(b).find(oldmsg); assert pos>=0
reserve=len(oldmsg)+64; assert all(x==0 for x in b[pos+len(oldmsg):pos+reserve])
msg=b'PTDE Material Response V2.28I active: exact CE18 class-routed response; slot2 metal LOD corridor 3..4; other CE18 stock dynamic LOD.\0'
assert len(msg)<=reserve
b[pos:pos+reserve]=msg+b'\0'*(reserve-len(msg))
out=bytes(b)

# Validate all DXBC and exact active routing.
outs=scan_dxbc(out); assert len(outs)==48
for q,sz in outs:
    d=out[q:q+sz]; assert ck.dxbc_checksum(d)==d[4:20]
parsed=json.loads((ROOT/'v291_recon/materials_parsed.json').read_text(encoding='utf8'))['dsr']; first={}
for x in parsed: first.setdefault(x['name'],x)
effective_hashes={x['sha'] for x in first.values()}
active=[]
for i in range(COUNT):
    h=out[HASH_OFF+i*HASH_STRIDE:HASH_OFF+i*HASH_STRIDE+64].decode('ascii',errors='ignore')
    a=struct.unpack_from('<I',out,REC_OFF+i*REC_STRIDE+0x44)[0]
    if a and h in effective_hashes: active.append((i,h))
assert len(active)==18 and {h for _,h in active}=={r['sha256'] for r in mats['records']},active
# exact c101 transport survives
for r in rows:
    o=REC_OFF+r['record_index']*REC_STRIDE
    assert struct.unpack_from('<3f',out,o+0x20)==struct.unpack_from('<3f',out,o+0x2c)
# bounds encoded correctly
for r in rows:
    o=REC_OFF+r['record_index']*REC_STRIDE
    assert abs(struct.unpack_from('<f',out,o+0x3c)[0]-r['lod_low'])<1e-6
    assert abs(struct.unpack_from('<f',out,o+0x40)[0]-r['lod_high'])<1e-6

# PE section diff accounting: V2.28I necessarily changes .text (sidecar uploader) and .rdata (DXBC/records/banner), nothing else.
changed=[i for i,(x,y) in enumerate(zip(base,out)) if x!=y]
sec={n:sum(raw<=i<raw+rs for i in changed) for n,va,raw,rs,vs in pe_sections(base)}
assert sec.get('.text',0)>0 and sec.get('.rdata',0)>0,sec
for n in ['.data','.pdata','.reloc']: assert sec.get(n,0)==0,sec

stem='DSRRL_PTDE_MATERIAL_RESPONSE_V2_28I_CE18_CLASS_ROUTED_EXACT_C101_COLOR0_SLOT2_DYNAMIC_LOD_CORRIDOR_OTHER_CLASSES_STOCK_LOD'
addon=ROOT/(stem+'.addon64'); audit=ROOT/(stem+'_AUDIT.json'); readme=ROOT/(stem+'_README.txt'); table=ROOT/(stem+'_MATERIALS.json'); script=Path(__file__); zp=ROOT/(stem+'_RUNTIME_TEST.zip')
addon.write_bytes(out)
table.write_text(json.dumps({'schema':'DSRRL_V2_28I_CE18_CLASS_ROUTED_TABLE_V1','count':18,'records':rows},indent=2),encoding='utf8')
counts={str(s):sum(1 for r in rows if r['ptde_envspc_slot']==s) for s in range(4)}
aud={
 'schema':'DSRRL_V2_28I_CE18_CLASS_ROUTED_AUDIT_V1','version':'V2.28I',
 'basis':BASE.name,'basis_sha256':hashlib.sha256(base).hexdigest(),'output':addon.name,'output_sha256':hashlib.sha256(out).hexdigest(),
 'goal':'Combine the owner-safe exact CE18 identity gate and V2.28G per-material legacy response with the V2.28H metal-only resource-shape correction, without blanket world/material activation.',
 'class_counts':counts,
 'material_response':{
   'all_exact_CE18':'c100 diffuse linearization + SPEC-domain pow2.2 identity + exact PTDE c101(material) + native semantic DSR COLOR0; DSR angular/horizon and t9 EnvSpec tail bypassed',
   'slot0':'8 exact body/dull/face identities; stock effective DSR dynamic LOD range [0,7]',
   'slot1':'2 exact Leather identities; stock effective DSR dynamic LOD range [0,7]',
   'slot2':'4 exact Metal identities; evidence-backed dynamic LOD corridor [3,4]',
   'slot3':'4 exact Wet/body identities; stock effective DSR dynamic LOD range [0,7]',
   'map_world_shared_nonhomologous':'FAIL_OPEN_STOCK_DSR'
 },
 'lod_operator':'LOD_I=min(max(LOD_D,low(material)),high(material)); LOD_D=cb79.w-3+1.2*log2(rho). V2.28I sets [3,4] only for exact PTDE slot2 CE18 and [0,7] for the other exact CE18 classes.',
 'slot2_basis':'342 matched probes; mip3 or mip4 is per-probe best integer DSR mip for 271/342=79.24%. This does not claim universal exact resource homology.',
 'transport':transport_meta,'patched_shaders':patched,
 'preserved':['exact CE18-only identity gate (18)','exact PTDE c101 transport','native DSR COLOR0 candidate','native DSR EnvSpec cubemap','fractional/spec-alpha dynamic LOD','no fixed LOD3','HemEnvLerp deferred','PointLight unchanged','map/world fail-open'],
 'not_claimed':['PTDE pixel equivalence','slot0/1/3 LOD equivalence','generic cloth/world-material coverage','PTDE/DSR vertex-color asset equality','EnvSpec source amplitude equivalence'],
 'dxbc_valid':'48/48','active_alt_records':len(active),'section_diffs':sec,'changed_bytes':len(changed),
 'expected_runtime':{'C100_REG':18,'TIER0':18,'TIER1':0,'TIER2':0,'UNMAPPED':562,'C101_EXACT_REG':18,'C101_SIBLING_REG':0},
 'final_mod':'PARAM_ONLY; diagnostic addon only'
}
audit.write_text(json.dumps(aud,indent=2),encoding='utf8')
readme.write_text(textwrap.dedent('''\
DSRRL V2.28I — CE18 CLASS-ROUTED MATERIAL RESPONSE + METAL SLOT2 LOD CORRIDOR

This combines the two useful branches instead of choosing between them:

1) V2.28G exact equipment-material response is active again for ALL 18 exact CE18 identities.
   That includes the exact slot0 body/dull/face group, Leather, Metal and Wet equipment identities.
   It does NOT enable a blanket material response for map/world/shared materials.

2) The V2.28H resource-shape correction remains limited to the exact PTDE slot2 metal class.
   Metal keeps a dynamic fractional DSR LOD, but its effective range is projected to [3,4].
   This is not fixed LOD3.

The other CE18 classes use [0,7], i.e. the native effective 8-mip range, so their stock DSR dynamic LOD is not yet forced into an unproven slot0/slot1/slot3 corridor. They still receive the more PTDE-like material tail:

    linear SPEC carrier * exact PTDE c101(material) * native DSR COLOR0

with DSR angular/horizon and t9 split-sum bypassed on the exact CE18 alternate.

Current exact CE18 split:
- slot0 body/dull/face: 8
- slot1 Leather: 2
- slot2 Metal: 4
- slot3 Wet/body: 4

No world/map/shared activation. No arbitrary gain. No fixed LOD. HemEnvLerp remains deferred. PointLight unchanged.

This version also introduces a per-material LOD-bound sidecar transport, so later slot0/1/3 resource-shape work can be changed per exact material/class without another blanket shader response.

Final production mod remains PARAM-only.
'''),encoding='utf8')
with zipfile.ZipFile(zp,'w',zipfile.ZIP_DEFLATED) as z:
    for p in [addon,audit,readme,table,script]: z.write(p,p.name)
with zipfile.ZipFile(zp) as z: assert z.testzip() is None
print(json.dumps({'zip':str(zp),'zip_sha256':hashlib.sha256(zp.read_bytes()).hexdigest(),'addon':str(addon),'addon_sha256':hashlib.sha256(out).hexdigest(),'size':zp.stat().st_size,'members':5,'dxbc':'48/48','active_alt_records':len(active),'slot_counts':counts,'section_diffs':sec,'changed_bytes':len(changed)},indent=2))
