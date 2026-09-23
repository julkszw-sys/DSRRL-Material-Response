import struct, hashlib, json, zipfile, textwrap, importlib.util
from pathlib import Path

ROOT=Path('/mnt/data')
BASE=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_28E_CE18_ALL_EXACT_PTDE_C101_LINEAR_CARRIER_NO_ANGULAR_NO_T9_DYNAMIC_LOD.addon64'
MAT=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_28E_CE18_ALL_EXACT_PTDE_C101_LINEAR_CARRIER_NO_ANGULAR_NO_T9_DYNAMIC_LOD_MATERIALS.json'
base=BASE.read_bytes(); mats=json.loads(MAT.read_text(encoding='utf8'))
sp=importlib.util.spec_from_file_location('ck',ROOT/'test_dxbc_ck.py'); ck=importlib.util.module_from_spec(sp); sp.loader.exec_module(ck)
NOP=0x0100003a
# Alt slots are the same stable opaque HemEnv hosts used since V2.28A.
# COLOR0 input registers are semantic facts independently recovered from ISGN / prior exact host diagnostics.
TARGETS={
    894:{'alt_slot':33,'c101_word':1675,'linear_reg':11,'color0_reg':6},
    913:{'alt_slot':34,'c101_word':1584,'linear_reg':10,'color0_reg':7},
    932:{'alt_slot':35,'c101_word':1244,'linear_reg':9, 'color0_reg':6},
}
REC_OFF=0x81280; REC_STRIDE=0x48; HASH_OFF=0x87a00; HASH_STRIDE=0x50; COUNT=368

def chunks(d):
    assert d[:4]==b'DXBC'; total=struct.unpack_from('<I',d,24)[0]; assert total==len(d)
    n=struct.unpack_from('<I',d,28)[0]; offs=struct.unpack_from('<'+'I'*n,d,32); out={}
    for o in offs:
        tag=d[o:o+4].decode(); sz=struct.unpack_from('<I',d,o+4)[0]; out[tag]=(o,o+8,sz,d[o+8:o+8+sz])
    return out

def words(d):
    c=chunks(d); k='SHEX' if 'SHEX' in c else 'SHDR'; _,p,sz,blob=c[k]
    return k,p,sz,list(struct.unpack('<'+'I'*(sz//4),blob))

def write_words(d,w):
    c=chunks(d); k='SHEX' if 'SHEX' in c else 'SHDR'; _,p,sz,_=c[k]
    q=bytearray(d); q[p:p+sz]=struct.pack('<'+'I'*len(w),*w)
    q[4:20]=b'\0'*16; q[4:20]=ck.dxbc_checksum(bytes(q)); assert q[4:20]==ck.dxbc_checksum(bytes(q))
    return bytes(q)

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

def sections(blob):
    e=struct.unpack_from('<I',blob,0x3c)[0]; n=struct.unpack_from('<H',blob,e+6)[0]; osz=struct.unpack_from('<H',blob,e+20)[0]; st=e+24+osz; out=[]
    for i in range(n):
        q=st+i*40; name=blob[q:q+8].rstrip(b'\0').decode(errors='replace'); vs,va,rs,raw=struct.unpack_from('<IIII',blob,q+8); out.append((name,raw,rs))
    return out

entries=scan_dxbc(base); assert len(entries)==48
b=bytearray(base); patched=[]
for shader,s in TARGETS.items():
    ao,asz=entries[s['alt_slot']]; old=bytes(b[ao:ao+asz]); _,_,_,w=words(old)
    m=s['c101_word']; f=s['linear_reg']; v=s['color0_reg']
    # V2.28E inherited V2.28D material gain at m:
    c101_mul=[0x08000038,0x00100072,0x00000002,0x00100246,f,0x00208246,0x0000000c,0x00000000]
    assert w[m:m+8]==c101_mul and w[m+8]==NOP, (shader,[hex(x) for x in w[m:m+9]])
    # Last 8 words before m are NOP padding created by the removed t9 receiver island.
    pre=m-8
    assert w[pre:m]==[NOP]*8, (shader,pre,[hex(x) for x in w[pre:m]])
    # Stage 1: preserve exact material-specific PTDE c101 multiplication, moved into the padding.
    w[pre:m]=c101_mul
    # Stage 2: multiply the resulting linear material carrier by semantic native DSR COLOR0.rgb,
    # the shader-coordinate homolog of PTDE VertexSpec. Exact asset-content equality is NOT claimed.
    color_mul=[0x07000038,0x00100072,0x00000002,0x00100246,0x00000002,0x00101246,v]
    assert len(color_mul)==7
    w[m:m+9]=color_mul+[NOP,NOP]
    nd=write_words(old,w)
    _,_,_,nw=words(nd)
    assert nw[pre:m]==c101_mul
    assert nw[m:m+7]==color_mul and nw[m+7:m+9]==[NOP,NOP]
    b[ao:ao+asz]=nd
    patched.append({
        'shader_index':shader,'alt_slot':s['alt_slot'],'offset':hex(ao),'size':asz,
        'c101_stage_word':pre,'color0_stage_word':m,'linear_source_reg':f,'semantic_color0_input_reg':v,
        'equation':'r2.xyz = linear_spec_carrier * cb12[0].rgb; r2.xyz = r2.xyz * COLOR0.rgb',
        'before_sha256':hashlib.sha256(old).hexdigest(),'after_sha256':hashlib.sha256(nd).hexdigest()
    })

# Banner update inside existing zero-padded reservation.
oldmsg=b'PTDE Material Response V2.28E active: CE18 exact materials use PTDE c101 on linear carrier; no angular/t9; dynamic LOD.\0'
pos=bytes(b).find(oldmsg); assert pos>=0
reserve=len(oldmsg)+64
assert all(x==0 for x in b[pos+len(oldmsg):pos+reserve])
msg=b'PTDE Material Response V2.28F active: CE18 exact PTDE c101 * native COLOR0 on linear carrier; no angular/t9; dynamic LOD.\0'
assert len(msg)<=reserve
b[pos:pos+reserve]=msg+b'\0'*(reserve-len(msg))
out=bytes(b)

# Validate all embedded DXBC checksums.
outs=scan_dxbc(out); assert len(outs)==48
for q,sz in outs:
    d=out[q:q+sz]; assert ck.dxbc_checksum(d)==d[4:20]

# Routing invariants: same 18 effective exact CE18 records remain selected; no map/world expansion.
mp=ROOT/'v291_recon/materials_parsed.json'; dsr=json.loads(mp.read_text(encoding='utf8'))['dsr']; first={}
for x in dsr: first.setdefault(x['name'],x)
effective_hashes={x['sha'] for x in first.values()}
active=[]
for i in range(COUNT):
    h=out[HASH_OFF+i*HASH_STRIDE:HASH_OFF+i*HASH_STRIDE+64].decode('ascii',errors='ignore')
    a=struct.unpack_from('<I',out,REC_OFF+i*REC_STRIDE+0x44)[0]
    if a and h in effective_hashes: active.append((i,h))
assert len(active)==18, active
assert {h for _,h in active}=={r['sha256'] for r in mats['records']}

# PE section containment.
changed=[i for i,(x,y) in enumerate(zip(base,out)) if x!=y]
sec={n:sum(raw<=i<raw+rs for i in changed) for n,raw,rs in sections(base)}
assert sec.get('.rdata',0)==len(changed),sec
for n in ['.text','.data','.pdata','.reloc']: assert sec.get(n,0)==0,sec

stem='DSRRL_PTDE_MATERIAL_RESPONSE_V2_28F_CE18_ALL_EXACT_PTDE_C101_COLOR0_LINEAR_CARRIER_NO_ANGULAR_NO_T9_DYNAMIC_LOD'
addon=ROOT/(stem+'.addon64'); audit=ROOT/(stem+'_AUDIT.json'); readme=ROOT/(stem+'_README.txt'); table=ROOT/(stem+'_MATERIALS.json'); script=Path(__file__); zp=ROOT/(stem+'_RUNTIME_TEST.zip')
addon.write_bytes(out)
# Preserve V2.28E per-material donor table and annotate the newly added semantic factor.
tab=dict(mats); tab['schema']='DSRRL_V2_28F_CE18_C101_COLOR0_TABLE_V1'; tab['receiver_material_factor']='linear_spec_carrier * c101_PTDE(material) * native_DSR_COLOR0.rgb'; tab['color0_asset_homology']='SHADER_COORDINATE_CONFIRMED; ASSET_CONTENT_EQUALITY_NOT_CLAIMED'
table.write_text(json.dumps(tab,indent=2),encoding='utf8')
aud={
 'schema':'DSRRL_V2_28F_CE18_C101_COLOR0_LINEAR_CARRIER_AUDIT_V1','version':'V2.28F',
 'basis':BASE.name,'basis_sha256':hashlib.sha256(base).hexdigest(),'output':addon.name,'output_sha256':hashlib.sha256(out).hexdigest(),
 'purpose':'Next isolated PTDE broad-EnvSpec material operator after V2.28E: add semantic native DSR COLOR0.rgb as the shader-coordinate homolog of PTDE VertexSpec, while preserving the established CE18 safety gate and all resource/LOD state.',
 'operator':{
   'V2.28E':'M = Lspec_linear * c101_PTDE(material)',
   'V2.28F':'M = Lspec_linear * c101_PTDE(material) * COLOR0_DSR.rgb',
   'status':'COLOR0 coordinate/topology homology is confirmed across the paired Phn Spc family; PTDE-vs-DSR COLOR0 asset-content equality remains unproven, so this is an operator-isolation diagnostic rather than an equivalence claim.'
 },
 'patched':patched,
 'routing':{'active_alt_records':18,'tier0_exact_only':18,'map_world_shared_nonhomologous':'FAIL_OPEN_STOCK_DSR'},
 'preserved':['CE18-only c100/diffuse-linear gate','per-material exact PTDE c101 donor from V2.28E','V2.28A SPEC pow2.2->identity on alternate stable opaque HemEnv hosts','V2.28B angular/horizon bypass','V2.28C t9 split-sum bypass','stock roughness-driven dynamic EnvSpec LOD','native DSR EnvSpec cube','native SpecTex RGB/alpha','HemEnvLerp deferred/stock','PointLight unchanged'],
 'absent':['fixed LOD3','blanket c101=2.5','raw SpecTex resample legacy bridge','world/map activation','cube replacement','COLOR0 achromatization','arbitrary gain'],
 'shader_payloads':'3 alternate payloads changed for COLOR0 factor; all 48 DXBC checksums valid','section_diffs':sec,'changed_bytes':len(changed),
 'expected_runtime':{'C100_REG':18,'TIER0':18,'TIER1':0,'TIER2':0,'UNMAPPED':562,'C101_EXACT_REG':18,'C101_SIBLING_REG':0},
 'coverage_note':'COLOR0 stage exists only on existing stable opaque HemEnv alternate hosts 894/913/932. No unsupported host/body coverage is synthesized.',
 'final_mod':'PARAM_ONLY; diagnostic addon only'
}
audit.write_text(json.dumps(aud,indent=2),encoding='utf8')
readme.write_text(textwrap.dedent('''\
DSRRL V2.28F — CE18 exact c101 + native COLOR0 on linear material carrier

Purpose
-------
V2.28E restored the exact PTDE per-material c101 material gain for the safe CE18 gate. The remaining verified PTDE legacy broad-EnvSpec material product also contains VertexSpec. Across the paired PTDE/DSR Phn Spc family, COLOR0 is the confirmed shader-coordinate homolog, although byte/content equality of the underlying vertex-color assets is not established.

V2.28F therefore changes one material-response coordinate only:

    V2.28E: M = Lspec_linear * c101_PTDE(material)
    V2.28F: M = Lspec_linear * c101_PTDE(material) * COLOR0_DSR.rgb

Everything else remains fixed:
- CE18 exact character/equipment gate only; map/world/shared/nonhomologous materials fail open to stock DSR.
- Per-material PTDE c101 donors; no blanket x2.5.
- SPEC-domain pow(2.2)->identity.
- Angular/horizon bypass.
- t9 split-sum bypass.
- Stock roughness-driven dynamic EnvSpec LOD; NO fixed LOD3.
- Native DSR EnvSpec cubemap and native SpecTex alpha/roughness.
- HemEnvLerp deferred; PointLight unchanged.

Interpretation
--------------
This is an operator-isolation diagnostic. A visible change establishes that the native DSR COLOR0 lane materially participates in the missing PTDE-style broad material response. It does not prove PTDE/DSR vertex-color asset equality.

Final production mod remains PARAM-only.
'''),encoding='utf8')
with zipfile.ZipFile(zp,'w',zipfile.ZIP_DEFLATED) as z:
    for p in [addon,audit,readme,table,script]: z.write(p,p.name)
with zipfile.ZipFile(zp) as z: assert z.testzip() is None
print(json.dumps({'zip':str(zp),'zip_sha256':hashlib.sha256(zp.read_bytes()).hexdigest(),'addon':str(addon),'addon_sha256':hashlib.sha256(out).hexdigest(),'changed_bytes':len(changed),'section_diffs':sec,'dxbc':'48/48','active_alt_records':len(active),'patched':patched},indent=2))
