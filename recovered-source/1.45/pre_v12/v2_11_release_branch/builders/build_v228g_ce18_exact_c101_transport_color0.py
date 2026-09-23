import struct, hashlib, json, zipfile, textwrap, importlib.util
from pathlib import Path
ROOT=Path('/mnt/data')
BASE=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_28F_CE18_ALL_EXACT_PTDE_C101_COLOR0_LINEAR_CARRIER_NO_ANGULAR_NO_T9_DYNAMIC_LOD.addon64'
MAT=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_28F_CE18_ALL_EXACT_PTDE_C101_COLOR0_LINEAR_CARRIER_NO_ANGULAR_NO_T9_DYNAMIC_LOD_MATERIALS.json'
base=BASE.read_bytes(); mats=json.loads(MAT.read_text(encoding='utf8'))
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

# Runtime RE fact for this binary family (function around VA 0x1800077A0):
# the 64-byte b12 buffer is built as:
#   cb12[0].xyz <- donor record +0x2C/+0x30/+0x34 ; cb12[0].w <- alt flag
#   cb12[1].xyz <- donor record +0x10/+0x14/+0x18 ; cb12[1].w <- 1
#   cb12[2] = 0 ; cb12[3] = 0
# V2.28F shader multiplies by cb12[0].rgb. V2.28E/F had enabled all CE18 alternate flags
# but most records still carried legacy rooted c101 at +0x2C, not the exact PTDE c101 stored at +0x20.
# V2.28G fixes only that donor transport: for the 18 exact CE18 records, copy exact c101 +0x20..+0x28
# into the actually uploaded cb12[0].xyz source +0x2C..+0x34.

b=bytearray(base)
# Hash table index lookup
embedded={}
for i in range(COUNT):
    h=base[HASH_OFF+i*HASH_STRIDE:HASH_OFF+i*HASH_STRIDE+64].decode('ascii',errors='ignore')
    if len(h)==64: embedded[h]=i

rows=[]; changed_materials=[]
for r in mats['records']:
    h=r['sha256']; assert h in embedded
    i=embedded[h]; o=REC_OFF+i*REC_STRIDE
    flag=struct.unpack_from('<I',base,o+0x44)[0]; exact=struct.unpack_from('<I',base,o+0x38)[0]
    assert flag==1 and exact==1, (r['name'],flag,exact)
    exact_c101=struct.unpack_from('<3f',base,o+0x20)
    uploaded_before=struct.unpack_from('<3f',base,o+0x2c)
    # Cross-check metadata table's PTDE donor.
    expected=tuple(float(x) for x in r['ptde_c101'])
    assert all(abs(a-bb)<1e-6 for a,bb in zip(exact_c101,expected)), (r['name'],exact_c101,expected)
    struct.pack_into('<3f',b,o+0x2c,*exact_c101)
    uploaded_after=struct.unpack_from('<3f',b,o+0x2c)
    changed = any(abs(a-bb)>1e-7 for a,bb in zip(uploaded_before,uploaded_after))
    if changed: changed_materials.append(r['name'])
    rows.append({
        'name':r['name'],'sha256':h,'record_index':i,'ptde_c101_exact':list(exact_c101),
        'cb12_0_uploaded_before':list(uploaded_before),'cb12_0_uploaded_after':list(uploaded_after),
        'changed':changed,'ptde_envspc_slot':r['ptde_envspc_slot']
    })

# Banner update within padded reservation.
old=b'PTDE Material Response V2.28F active: CE18 exact PTDE c101 * native COLOR0 on linear carrier; no angular/t9; dynamic LOD.\0'
pos=bytes(b).find(old); assert pos>=0
reserve=len(old)+64
assert all(x==0 for x in b[pos+len(old):pos+reserve])
msg=b'PTDE Material Response V2.28G active: CE18 exact c101 transport + native COLOR0 on linear carrier; no angular/t9; dynamic LOD.\0'
assert len(msg)<=reserve
b[pos:pos+reserve]=msg+b'\0'*(reserve-len(msg))
out=bytes(b)

# Shader payloads are byte-identical to V2.28F; this is donor transport only.
e0=scan_dxbc(base); e1=scan_dxbc(out); assert len(e0)==len(e1)==48
assert [x[2] for x in e0]==[x[2] for x in e1]
for q,sz,_ in e1:
    d=out[q:q+sz]; assert ck.dxbc_checksum(d)==d[4:20]

# Effective routing remains exactly the same 18 CE18 identities.
parsed=json.loads((ROOT/'v291_recon/materials_parsed.json').read_text(encoding='utf8'))['dsr']; first={}
for x in parsed: first.setdefault(x['name'],x)
effective_hashes={x['sha'] for x in first.values()}
active=[]
for i in range(COUNT):
    h=out[HASH_OFF+i*HASH_STRIDE:HASH_OFF+i*HASH_STRIDE+64].decode('ascii',errors='ignore')
    a=struct.unpack_from('<I',out,REC_OFF+i*REC_STRIDE+0x44)[0]
    if a and h in effective_hashes: active.append((i,h))
assert len(active)==18 and {h for _,h in active}=={r['sha256'] for r in mats['records']}, active

# Prove the actual uploaded cb12[0].xyz now equals exact c101 for all 18 active CE18 records.
for r in rows:
    i=r['record_index']; o=REC_OFF+i*REC_STRIDE
    exact_c101=struct.unpack_from('<3f',out,o+0x20)
    uploaded=struct.unpack_from('<3f',out,o+0x2c)
    assert exact_c101==uploaded, (r['name'],exact_c101,uploaded)

# PE diffs stay in .rdata.
changed=[i for i,(x,y) in enumerate(zip(base,out)) if x!=y]
sec={n:sum(raw<=i<raw+rs for i in changed) for n,raw,rs in sections(base)}
assert sec.get('.rdata',0)==len(changed),sec
for n in ['.text','.data','.pdata','.reloc']: assert sec.get(n,0)==0,sec

stem='DSRRL_PTDE_MATERIAL_RESPONSE_V2_28G_CE18_EXACT_C101_TRANSPORT_COLOR0_LINEAR_CARRIER_NO_ANGULAR_NO_T9_DYNAMIC_LOD'
addon=ROOT/(stem+'.addon64'); audit=ROOT/(stem+'_AUDIT.json'); readme=ROOT/(stem+'_README.txt'); table=ROOT/(stem+'_MATERIALS.json'); script=Path(__file__); zp=ROOT/(stem+'_RUNTIME_TEST.zip')
addon.write_bytes(out)
table.write_text(json.dumps({'schema':'DSRRL_V2_28G_EXACT_C101_TRANSPORT_TABLE_V1','count':18,'records':rows},indent=2),encoding='utf8')
aud={
 'schema':'DSRRL_V2_28G_EXACT_C101_TRANSPORT_AUDIT_V1','version':'V2.28G',
 'basis':BASE.name,'basis_sha256':hashlib.sha256(base).hexdigest(),'output':addon.name,'output_sha256':hashlib.sha256(out).hexdigest(),
 'new_RE_finding':{
   'runtime_b12_layout':'cb12[0].xyz is sourced from donor record +0x2C/+0x30/+0x34; cb12[1].xyz from +0x10/+0x14/+0x18; cb12[2] and cb12[3] are zero-initialized in the current runtime path.',
   'v228e_f_transport_bug':'V2.28E/F enabled all 18 exact CE18 alternate selectors but did not rewrite most +0x2C donor values. Therefore non-P_Metal c101 values reaching cb12[0] were legacy rooted values (e.g. Leather 1.5^(1/2.2), Wet 2.0^(1/2.2), generic Metal 2.5^(1/2.2)) rather than exact PTDE c101. P_Metal base was already 2.5 and remains unchanged.'
 },
 'operator':{
   'V2.28F_effective_non_pmetal':'M = Lspec_linear * c101_legacy_sidecar * COLOR0, where c101_legacy_sidecar was often root-gamma form.',
   'V2.28G':'M = Lspec_linear * c101_PTDE_exact(material) * COLOR0_DSR.rgb',
   'change':'donor transport correction only; shader math and routing unchanged.'
 },
 'changed_material_count':len(changed_materials),'changed_materials':changed_materials,'materials':rows,
 'preserved':['CE18 exact 18-material gate','map/world/shared/nonhomologous fail-open stock DSR','V2.28F COLOR0 multiplier','SPEC-domain pow2.2->identity','angular/horizon bypass','t9 split-sum bypass','stock roughness-driven dynamic EnvSpec LOD','native DSR EnvSpec cube','HemEnvLerp deferred','PointLight unchanged'],
 'absent':['fixed LOD3','world/map activation','raw SpecTex legacy resample','arbitrary gain','dynamic LOD modification','EnvSpec source amplitude compensation'],
 'shader_payloads':'48/48 byte-identical to V2.28F; checksums valid','section_diffs':sec,'changed_bytes':len(changed),
 'expected_runtime':{'C100_REG':18,'TIER0':18,'TIER1':0,'TIER2':0,'UNMAPPED':562,'C101_EXACT_REG':18,'C101_SIBLING_REG':0},
 'final_mod':'PARAM_ONLY; diagnostic addon only'
}
audit.write_text(json.dumps(aud,indent=2),encoding='utf8')
readme.write_text(textwrap.dedent(f'''\
DSRRL V2.28G — CE18 exact c101 transport correction + COLOR0

V2.28F had no obvious visual falsifier in the owner test, so the COLOR0/VertexSpec candidate remains in the active diagnostic path.

Before moving to the EnvSpec resource/LOD residual, an offline runtime-dataflow audit found an important transport mismatch in V2.28E/F:

- the shader reads c101 from cb12[0].rgb;
- the current runtime fills cb12[0].rgb from donor-record offsets +0x2C/+0x30/+0x34;
- the exact PTDE c101 values are stored separately at +0x20/+0x24/+0x28;
- for most non-P_Metal CE18 records, +0x2C held the legacy rooted donor rather than exact PTDE c101.

V2.28G changes ONLY those donor values for the already-selected 18 exact CE18 records so that:

    cb12[0].rgb = c101_PTDE_exact(material)
    M = Lspec_linear * c101_PTDE_exact(material) * COLOR0_DSR.rgb

P_Metal base already received 2.5 and is unchanged. The corrected materials are the CE18 Metal/Leather/Wet families whose exact PTDE c101 differs from the prior rooted sidecar representation. {len(changed_materials)} of 18 active records change numerically.

Everything else is byte-identical at shader level to V2.28F:
- no fixed LOD3;
- stock DSR dynamic roughness/LOD and native EnvSpec cube;
- angular and t9 remain bypassed on the covered alternate;
- map/world materials remain stock-DSR fail-open;
- HemEnvLerp remains deferred; PointLight unchanged.

This is a transport correction, not an arbitrary gain experiment. Final production mod remains PARAM-only.
'''),encoding='utf8')
with zipfile.ZipFile(zp,'w',zipfile.ZIP_DEFLATED) as z:
    for p in [addon,audit,readme,table,script]: z.write(p,p.name)
with zipfile.ZipFile(zp) as z: assert z.testzip() is None
print(json.dumps({'zip':str(zp),'zip_sha256':hashlib.sha256(zp.read_bytes()).hexdigest(),'addon':str(addon),'addon_sha256':hashlib.sha256(out).hexdigest(),'changed_material_count':len(changed_materials),'changed_materials':changed_materials,'changed_bytes':len(changed),'section_diffs':sec,'dxbc':'48/48 byte-identical V2.28F','active_alt_records':len(active)},indent=2))
