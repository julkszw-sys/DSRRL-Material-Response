import struct, hashlib, json, zipfile, textwrap, importlib.util
from pathlib import Path
ROOT=Path('/mnt/data')
BASE=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_28D_CE18_PMETAL_PTDE_C101_ON_LINEAR_CARRIER_NO_ANGULAR_NO_T9_DYNAMIC_LOD.addon64'
REG=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_27B_CE18_STOCK_DSR_ENVSPEC_DYNAMIC_LOD_CE18_REGISTRY.json'
base=BASE.read_bytes(); reg=json.loads(REG.read_text(encoding='utf8'))['records']
assert len(reg)==18
REC_OFF=0x81280; REC_STRIDE=0x48; HASH_OFF=0x87a00; HASH_STRIDE=0x50; COUNT=368
sp=importlib.util.spec_from_file_location('ck',ROOT/'test_dxbc_ck.py'); ck=importlib.util.module_from_spec(sp); sp.loader.exec_module(ck)

def scan_dxbc(blob):
    es=[]; q=0
    while True:
        q=blob.find(b'DXBC',q)
        if q<0: break
        if q+28<=len(blob):
            sz=struct.unpack_from('<I',blob,q+24)[0]
            if 1000<sz<100000 and q+sz<=len(blob):
                d=blob[q:q+sz]
                if struct.unpack_from('<I',d,24)[0]==len(d): es.append((q,sz,hashlib.sha256(d).hexdigest()))
        q+=4
    return es

def sections(blob):
    e=struct.unpack_from('<I',blob,0x3c)[0]; n=struct.unpack_from('<H',blob,e+6)[0]; osz=struct.unpack_from('<H',blob,e+20)[0]; st=e+24+osz; out=[]
    for i in range(n):
        q=st+i*40; name=blob[q:q+8].rstrip(b'\0').decode(errors='replace'); vs,va,rs,raw=struct.unpack_from('<IIII',blob,q+8); out.append((name,raw,rs))
    return out

# Build exact CE18 hash->record map from embedded table and validate material-specific PTDE c101 donors already present.
embedded={}
for i in range(COUNT):
    h=base[HASH_OFF+i*HASH_STRIDE:HASH_OFF+i*HASH_STRIDE+64].decode('ascii',errors='ignore')
    if len(h)==64: embedded[h]=i

b=bytearray(base); material_rows=[]
for r in reg:
    h=r['sha256']; assert h in embedded and embedded[h]==r['index'], (h,embedded.get(h),r['index'])
    o=REC_OFF+r['index']*REC_STRIDE
    tier=struct.unpack_from('<I',base,o+0x1c)[0]; assert tier==0
    c101=struct.unpack_from('<3f',base,o+0x20)
    exact=struct.unpack_from('<I',base,o+0x38)[0]
    c102=struct.unpack_from('<f',base,o+0x3c)[0]
    slot=struct.unpack_from('<I',base,o+0x40)[0]
    old_alt=struct.unpack_from('<I',base,o+0x44)[0]
    assert exact==1, (r['names'],exact)
    # Enable the V2.28D alternate receiver for this exact CE18 donor.
    struct.pack_into('<I',b,o+0x44,1)
    material_rows.append({'name':r['names'][0],'sha256':h,'c100':list(struct.unpack_from('<3f',base,o+0x10)),'ptde_c101':list(c101),'ptde_c102':c102,'ptde_envspc_slot':slot,'alt_before':old_alt,'alt_after':1})

# Assert only CE18 EFFECTIVE DSR material identities can resolve to the alternate at runtime.
# The donor table contains historical dormant alt flags for hashes that are not effective first-entry materials;
# these do not contribute to C101_EXACT_REG and are intentionally left untouched.
mats=json.loads((ROOT/'v291_recon/materials_parsed.json').read_text(encoding='utf8'))['dsr']
first={}
for m in mats: first.setdefault(m['name'],m)
effective_hashes={m['sha'] for m in first.values()}
active=[]
for i in range(COUNT):
    a=struct.unpack_from('<I',b,REC_OFF+i*REC_STRIDE+0x44)[0]
    h=b[HASH_OFF+i*HASH_STRIDE:HASH_OFF+i*HASH_STRIDE+64].decode('ascii',errors='ignore')
    if a and h in effective_hashes: active.append((i,h))
assert len(active)==18 and {h for _,h in active}=={r['sha256'] for r in reg}, active

# Banner update only within existing padded reservation.
old=b'PTDE Material Response V2.28D active: CE18 + exact P_Metal PTDE c101 once on linear carrier; no angular/t9; dynamic LOD.\0'
pos=bytes(b).find(old); assert pos>=0
reserve=len(old)+64
assert all(x==0 for x in b[pos+len(old):pos+reserve])
msg=b'PTDE Material Response V2.28E active: CE18 exact materials use PTDE c101 on linear carrier; no angular/t9; dynamic LOD.\0'
assert len(msg)<=reserve
b[pos:pos+reserve]=msg+b'\0'*(reserve-len(msg))
out=bytes(b)

# Shader payloads must be byte-identical to V2.28D.
e0=scan_dxbc(base); e1=scan_dxbc(out); assert len(e0)==len(e1)==48
assert [x[2] for x in e0]==[x[2] for x in e1]
for q,sz,_ in e1:
    d=out[q:q+sz]; assert ck.dxbc_checksum(d)==d[4:20]

# PE diffs restricted to .rdata.
changed=[i for i,(x,y) in enumerate(zip(base,out)) if x!=y]
sec={n:sum(raw<=i<raw+rs for i in changed) for n,raw,rs in sections(base)}
assert sec.get('.rdata',0)==len(changed),sec
for n in ['.text','.data','.pdata','.reloc']: assert sec.get(n,0)==0,sec

stem='DSRRL_PTDE_MATERIAL_RESPONSE_V2_28E_CE18_ALL_EXACT_PTDE_C101_LINEAR_CARRIER_NO_ANGULAR_NO_T9_DYNAMIC_LOD'
addon=ROOT/(stem+'.addon64'); audit=ROOT/(stem+'_AUDIT.json'); readme=ROOT/(stem+'_README.txt'); table=ROOT/(stem+'_MATERIALS.json'); script=Path(__file__); zp=ROOT/(stem+'_RUNTIME_TEST.zip')
addon.write_bytes(out)
table.write_text(json.dumps({'schema':'DSRRL_V2_28E_CE18_C101_TABLE_V1','count':18,'records':material_rows},indent=2),encoding='utf8')
aud={
 'schema':'DSRRL_V2_28E_CE18_ALL_C101_LINEAR_CARRIER_AUDIT_V1','version':'V2.28E',
 'basis':BASE.name,'basis_sha256':hashlib.sha256(base).hexdigest(),'output':addon.name,'output_sha256':hashlib.sha256(out).hexdigest(),
 'purpose':'Owner requested extending the V2.28D PTDE-authored c101-on-linear-carrier experiment beyond exact P_Metal to the other materials already inside the safe CE18 character/equipment gate.',
 'operator':'Same V2.28D alternate receiver, selected per exact CE18 raw-MTD. Each draw consumes its own existing PTDE donor cb12[0].rgb from that material record; there is no blanket 2.5 gain.',
 'routing':{'active_alt_records':18,'tier0_exact_only':18,'map_world_shared_nonhomologous':'FAIL_OPEN_STOCK_DSR','hashes':[h for _,h in active]},
 'material_donors':material_rows,
 'preserved':['CE18-only c100/diffuse-linear gate','V2.28A SPEC pow2.2->identity on alternate stable opaque HemEnv hosts','V2.28B angular/horizon bypass','V2.28C t9 split-sum bypass','stock roughness-driven dynamic EnvSpec LOD','native DSR EnvSpec cube','native SpecTex RGB/alpha','HemEnvLerp deferred/stock','PointLight unchanged'],
 'absent':['fixed LOD3','blanket c101=2.5','raw SpecTex*c101 resample bridge','world/map activation','cube replacement','chroma manipulation'],
 'shader_payloads':'48/48 byte-identical to V2.28D; all DXBC checksums valid','section_diffs':sec,'changed_bytes':len(changed),
 'expected_runtime':{'C100_REG':18,'TIER0':18,'TIER1':0,'TIER2':0,'UNMAPPED':562,'C101_EXACT_REG':18,'C101_SIBLING_REG':0},
 'coverage_note':'Alternate shader bodies exist only for the stable opaque HemEnv host set carried by V2.28D (894/913/932). CE18 materials routed through other shader bodies remain on their existing fail-open/stock path; enabling their record does not synthesize unsupported shader coverage.',
 'final_mod':'PARAM_ONLY; diagnostic addon only'
}
audit.write_text(json.dumps(aud,indent=2),encoding='utf8')
readme.write_text(textwrap.dedent('''\
DSRRL V2.28E — CE18 ALL EXACT MATERIALS: PTDE c101 ON LINEAR CARRIER

Yes: V2.28D applied the new PTDE c101 material gain only to exact base P_Metal.

V2.28E extends the SAME alternate receiver to all 18 exact Tier0 materials in the already-safe CE18 character/equipment gate. It does NOT multiply every material by 2.5. Each material consumes its own retained exact PTDE c101 donor from cb12[0].rgb:
- Metal family: 2.5 where authored by PTDE.
- Leather family: 1.5 where authored by PTDE.
- Wet family: 2.0 where authored by PTDE.
- DullLeather/body/face materials: typically 1.0 where authored by PTDE.

The rest of the receiver remains V2.28D:
- linear pre-PBL SPEC carrier,
- angular/horizon bypass,
- t9 split-sum bypass,
- stock roughness-driven dynamic EnvSpec LOD (NO fixed LOD3),
- native DSR EnvSpec cubemap.

Routing safety is unchanged: only CE18 exact character/equipment raw-MTD identities can select the alternate; map/world/shared/nonhomologous materials still fail open to stock DSR. HemEnvLerp remains deferred and PointLight unchanged.

Important: the alternate DXBC payloads cover the existing stable opaque HemEnv host set (894/913/932). CE18 materials that route through other bodies are not force-patched.

Diagnostic only. Final production mod remains PARAM-only.
'''),encoding='utf8')
with zipfile.ZipFile(zp,'w',zipfile.ZIP_DEFLATED) as z:
    for p in [addon,audit,readme,table,script]: z.write(p,p.name)
with zipfile.ZipFile(zp) as z: assert z.testzip() is None
print(json.dumps({'zip':str(zp),'zip_sha256':hashlib.sha256(zp.read_bytes()).hexdigest(),'addon_sha256':hashlib.sha256(out).hexdigest(),'active_alt_records':len(active),'changed_bytes':len(changed),'section_diffs':sec,'dxbc':'48/48 byte-identical V2.28D','materials':[(r['name'],r['ptde_c101']) for r in material_rows]},indent=2))
