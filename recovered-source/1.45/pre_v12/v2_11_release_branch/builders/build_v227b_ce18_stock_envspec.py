import struct, hashlib, json, zipfile, textwrap, re, collections
from pathlib import Path
from importlib.util import spec_from_file_location, module_from_spec
ROOT=Path('/mnt/data')
SRCZIP=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_26A_PMETAL_V291_PBL_FIXED_LOD3_ONLY_RUNTIME_TEST.zip'
with zipfile.ZipFile(SRCZIP) as z:
    an=[n for n in z.namelist() if n.endswith('.addon64')][0]
    base=z.read(an)
SRCNAME=an
REC_OFF=0x81280; REC_STRIDE=0x48; HASH_OFF=0x87a00; HASH_STRIDE=0x50; COUNT=368
PMETAL='ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b'
MATS=json.loads((ROOT/'v291_recon/materials_parsed.json').read_text(encoding='utf8'))['dsr']
first={}
for m in MATS: first.setdefault(m['name'],m)
bysha=collections.defaultdict(list)
for m in first.values(): bysha[m['sha']].append(m['name'])
# Recover exact CE18 as the raw hashes from the V2.26A hash table that intersect the 580 effective DSR MTD hashes.
active=[]
for i in range(COUNT):
    h=base[HASH_OFF+i*HASH_STRIDE:HASH_OFF+i*HASH_STRIDE+64].decode('ascii',errors='ignore')
    if h in bysha:
        tier=struct.unpack_from('<I',base,REC_OFF+i*REC_STRIDE+0x1c)[0]
        c100=struct.unpack_from('<3f',base,REC_OFF+i*REC_STRIDE+0x10)
        alt=struct.unpack_from('<I',base,REC_OFF+i*REC_STRIDE+0x44)[0]
        active.append({'index':i,'sha256':h,'names':bysha[h],'tier':tier,'c100':list(c100),'alt_before':alt})
assert len(active)==18, len(active)
assert all(x['tier']==0 for x in active)
pm=next(x for x in active if x['sha256']==PMETAL)
assert pm['alt_before']==1
assert sum(x['alt_before'] for x in active)==1
# Only semantic change vs V2.26A: disable exact P_Metal alternate, so CE18 all uses generic V2.9.1 + stock DSR PBL/EnvSpec/dynamic LOD.
b=bytearray(base)
struct.pack_into('<I',b,REC_OFF+pm['index']*REC_STRIDE+0x44,0)
# banner replacement within old zero-padded reservation
old=b'PTDE Material Response V2.26A active: V2.9.1 material/PBL + P_Metal fixed LOD3 only; diagnostic.\0'
pos=bytes(b).find(old); assert pos>=0
reserve=len(old)+96
assert all(x==0 for x in b[pos+len(old):pos+reserve])
msg=b'PTDE Material Response V2.27B active: CE18 V2.9.1 c100 only; stock DSR PBL/EnvSpec/dynamic LOD; map/world fail-open.\0'
assert len(msg)<=reserve
b[pos:pos+reserve]=msg+b'\0'*(reserve-len(msg))
out=bytes(b)
# PE section containment
def sections(blob):
    e=struct.unpack_from('<I',blob,0x3c)[0]; n=struct.unpack_from('<H',blob,e+6)[0]; osz=struct.unpack_from('<H',blob,e+20)[0]; st=e+24+osz; r=[]
    for j in range(n):
        q=st+j*40; name=blob[q:q+8].rstrip(b'\0').decode(errors='replace'); vs,va,rs,raw=struct.unpack_from('<IIII',blob,q+8); r.append((name,raw,rs))
    return r
changed=[i for i,(x,y) in enumerate(zip(base,out)) if x!=y]
sec={n:sum(raw<=i<raw+rs for i in changed) for n,raw,rs in sections(base)}
assert sec.get('.rdata',0)==len(changed),sec
for n in ['.text','.data','.pdata','.reloc']: assert sec.get(n,0)==0,sec
# DXBC byte-identical scan hashes
def dxbc_hashes(blob):
    hs=[]; q=0
    while True:
        q=blob.find(b'DXBC',q)
        if q<0: break
        if q+28<=len(blob):
            sz=struct.unpack_from('<I',blob,q+24)[0]
            if 1000<sz<100000 and q+sz<=len(blob) and struct.unpack_from('<I',blob,q+24)[0]==sz:
                d=blob[q:q+sz]
                if d[:4]==b'DXBC': hs.append(hashlib.sha256(d).hexdigest())
        q+=4
    return hs
h0=dxbc_hashes(base); h1=dxbc_hashes(out); assert len(h0)==48 and h0==h1
# Verify active DSR-name hits remain exactly 18 and only P_Metal alt changed to zero.
active_after=[]
for i in range(COUNT):
    h=out[HASH_OFF+i*HASH_STRIDE:HASH_OFF+i*HASH_STRIDE+64].decode('ascii',errors='ignore')
    if h in bysha:
        active_after.append((i,h,struct.unpack_from('<I',out,REC_OFF+i*REC_STRIDE+0x44)[0]))
assert len(active_after)==18 and all(a==0 for _,_,a in active_after)

stem='DSRRL_PTDE_MATERIAL_RESPONSE_V2_27B_CE18_STOCK_DSR_ENVSPEC_DYNAMIC_LOD'
addon=ROOT/(stem+'.addon64'); audit=ROOT/(stem+'_AUDIT.json'); readme=ROOT/(stem+'_README.txt'); registry=ROOT/(stem+'_CE18_REGISTRY.json'); script=Path(__file__); zp=ROOT/(stem+'_RUNTIME_TEST.zip')
addon.write_bytes(out)
registry.write_text(json.dumps({'schema':'DSRRL_CE18_RELEASE_GATE_V1','count':18,'records':active},indent=2),encoding='utf8')
aud={
 'schema':'DSRRL_V2_27B_CE18_STOCK_DSR_ENVSPEC_DYNAMIC_LOD_AUDIT_V1','version':'V2.27B',
 'basis_zip':SRCZIP.name,'basis_addon':SRCNAME,'basis_addon_sha256':hashlib.sha256(base).hexdigest(),
 'output_addon':addon.name,'output_addon_sha256':hashlib.sha256(out).hexdigest(),
 'reason':'V2.27A full 368-hash V2.9.1 c100 rollout is visually falsified by a class of over-bright map metallic objects (elevator, Sens Fortress grate, additional map metals). Return to CE18 character/equipment-only exact-SPX HOMOLOGOUS_SPC gate and remove fixed LOD3.',
 'active_registry':{'count':18,'tier0':18,'tier1':0,'tier2':0,'expected_C100_REG':18,'expected_UNMAPPED':562},
 'ce18_policy':'CHARACTER_EQUIPMENT_ONLY + HOMOLOGOUS_SPC + EXACT_SPX + effective first-entry raw MTD identity; all map/world/shared/nonhomologous materials fail-open to stock DSR',
 'spec_alt_policy':{'active_alt_before':1,'active_alt_after':0,'pmetal_sha256':PMETAL,'fixed_lod3':'DISABLED'},
 'active_behavior':['CE18 V2.9.1 PTDE c100 donor injection','CE18 diffuse material-domain x^2.2 -> identity','stock DSR SpecTex->F0/PBL','stock DSR angular/horizon','stock DSR t9 BRDF','native DSR EnvSpec cube','stock DSR roughness/dynamic EnvSpec LOD','map/world materials stock DSR','HemEnvLerp unchanged/deferred','PointLight unchanged'],
 'shader_payloads':{'dxbc_count':48,'byte_identical_to_V2_26A':True},
 'changed_bytes':len(changed),'section_diffs':sec,'zip_crc':'PASS after package creation','final_mod':'PARAM_ONLY; addon remains renderer diagnostic/release-module prototype'
}
audit.write_text(json.dumps(aud,indent=2),encoding='utf8')
readme.write_text(textwrap.dedent('''\
DSRRL V2.27B — CE18 SAFE GATE + STOCK DSR ENVSPEC / DYNAMIC LOD

Full V2.9.1 world/map rollout is removed after class-wide visual falsification: elevator, Sens Fortress grate and other map metallic objects became too bright.

Active bridge is restricted to CE18 only:
- 18 Tier0 character/equipment-only materials;
- HOMOLOGOUS_SPC;
- exact PTDE<->DSR SPX pairing;
- effective first-entry raw MTD identity.

Everything outside CE18 — especially map/world/shared/nonhomologous materials — fails open to stock DSR.

P_Metal uses the generic CE18 path with fully stock DSR SpecTex->F0/PBL/EnvSpec and stock dynamic roughness LOD. Fixed LOD3 and all c101/spec alternate branches are OFF.
HemEnvLerp remains deferred. PointLight unchanged.

Expected counters:
C100_REG=18 TIER0=18 TIER1=0 TIER2=0 UNMAPPED=562
C101_EXACT_REG=0 C101_SIBLING_REG=0

Diagnostic/runtime module only; final DSR Restored Lighting remains PARAM-only.
'''),encoding='utf8')
with zipfile.ZipFile(zp,'w',zipfile.ZIP_DEFLATED) as z:
    for p in [addon,audit,readme,registry,script]: z.write(p,p.name)
with zipfile.ZipFile(zp) as z: assert z.testzip() is None
print(json.dumps({'zip':str(zp),'zip_sha256':hashlib.sha256(zp.read_bytes()).hexdigest(),'addon_sha256':hashlib.sha256(out).hexdigest(),'active_ce18':18,'pmetal_alt_after':0,'dxbc':'48/48 byte-identical','changed_bytes':len(changed),'section_diffs':sec},indent=2))
