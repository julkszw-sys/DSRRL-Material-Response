import struct, hashlib, json, zipfile, textwrap, importlib.util
from pathlib import Path
ROOT=Path('/mnt/data')
BASE=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_28C_CE18_PMETAL_LINEAR_F0_NO_ANGULAR_NO_T9_DYNAMIC_LOD.addon64'
base=BASE.read_bytes()
PMETAL='ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b'
REC_OFF=0x81280; REC_STRIDE=0x48; HASH_OFF=0x87a00; HASH_STRIDE=0x50; COUNT=368
TARGETS={894:{'alt_slot':33,'mov_word':1675,'f0_reg':11},913:{'alt_slot':34,'mov_word':1584,'f0_reg':10},932:{'alt_slot':35,'mov_word':1244,'f0_reg':9}}
sp=importlib.util.spec_from_file_location('ck',ROOT/'test_dxbc_ck.py'); ck=importlib.util.module_from_spec(sp); sp.loader.exec_module(ck)
NOP=0x0100003a


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
    ao,asz=entries[s['alt_slot']]; old=bytes(b[ao:ao+asz]); k,p,sz,w=words(old)
    m=s['mov_word']; f=s['f0_reg']
    # V2.28C at this site is MOV r2.xyz, rF.xyz + 4 NOPs.
    expect=[0x05000036,0x00100072,0x00000002,0x00100246,f,NOP,NOP,NOP,NOP]
    assert w[m:m+9]==expect, (shader,[hex(x) for x in w[m:m+9]])
    # Replace only the direct-F0 MOV by the exact PTDE c101 donor multiply already carried in b12:
    # MUL r2.xyz, rF.xyz, cb12[0].  Operand encoding for cb12[0] is copied from the prior validated donor path.
    mul=[0x08000038,0x00100072,0x00000002,0x00100246,f,0x00208246,0x0000000c,0x00000000]
    w[m:m+9]=mul+[NOP]
    nd=write_words(old,w)
    _,_,_,nw=words(nd)
    assert nw[m:m+8]==mul and nw[m+8]==NOP
    b[ao:ao+asz]=nd
    patched.append({
        'shader_index':shader,'alt_slot':s['alt_slot'],'offset':hex(ao),'size':asz,'material_gain_word':m,
        'f0_source_reg':f,'operation':'r2.xyz = current linear pre-PBL P_Metal signal * cb12[0].rgb',
        'cb12_semantic':'exact PTDE c101 donor for base P_Metal; authored value 2.5, applied exactly once after V2.28A linearization and after V2.28C t9 removal',
        'before_sha256':hashlib.sha256(old).hexdigest(),'after_sha256':hashlib.sha256(nd).hexdigest()
    })

# Update runtime banner within existing padded reservation.
oldmsg=b'PTDE Material Response V2.28C active: CE18 + exact P_Metal linear F0 + no angular + no t9 split-sum; dynamic LOD retained.\0'
pos=bytes(b).find(oldmsg); assert pos>=0
reserve=len(oldmsg)+64
# accept the actual zero reservation until next nonzero byte; only overwrite conservative local span
assert all(x==0 for x in b[pos+len(oldmsg):pos+reserve])
msg=b'PTDE Material Response V2.28D active: CE18 + exact P_Metal PTDE c101 once on linear carrier; no angular/t9; dynamic LOD.\0'
assert len(msg)<=reserve
b[pos:pos+reserve]=msg+b'\0'*(reserve-len(msg))
out=bytes(b)

# Validate all embedded DXBC.
outs=scan_dxbc(out); assert len(outs)==48
for q,sz in outs:
    d=out[q:q+sz]; assert ck.dxbc_checksum(d)==d[4:20]

# Exact P_Metal alternate gate remains the only active spec alternate among effective hashes.
flags=[]
effective_hashes=None
mp=ROOT/'v291_recon/materials_parsed.json'
if mp.exists():
    mats=json.loads(mp.read_text(encoding='utf8'))['dsr']; first={}
    for mat in mats: first.setdefault(mat['name'],mat)
    effective_hashes={mat['sha'] for mat in first.values()}
for i in range(COUNT):
    h=out[HASH_OFF+i*HASH_STRIDE:HASH_OFF+i*HASH_STRIDE+64].decode('ascii',errors='ignore')
    a=struct.unpack_from('<I',out,REC_OFF+i*REC_STRIDE+0x44)[0]
    if a and (effective_hashes is None or h in effective_hashes): flags.append((i,h))
assert any(h==PMETAL for _,h in flags), flags
if effective_hashes is not None:
    assert len(flags)==1 and flags[0][1]==PMETAL, flags

# Section containment.
changed=[i for i,(x,y) in enumerate(zip(base,out)) if x!=y]
sec={n:sum(raw<=i<raw+rs for i in changed) for n,raw,rs in sections(base)}
assert sec.get('.rdata',0)==len(changed),sec
for n in ['.text','.data','.pdata','.reloc']: assert sec.get(n,0)==0,sec

stem='DSRRL_PTDE_MATERIAL_RESPONSE_V2_28D_CE18_PMETAL_PTDE_C101_ON_LINEAR_CARRIER_NO_ANGULAR_NO_T9_DYNAMIC_LOD'
addon=ROOT/(stem+'.addon64'); audit=ROOT/(stem+'_AUDIT.json'); readme=ROOT/(stem+'_README.txt'); script=Path(__file__); zp=ROOT/(stem+'_RUNTIME_TEST.zip')
addon.write_bytes(out)
aud={
 'schema':'DSRRL_V2_28D_PMETAL_C101_ON_LINEAR_CARRIER_AUDIT_V1','version':'V2.28D',
 'basis':BASE.name,'basis_sha256':hashlib.sha256(base).hexdigest(),'output':addon.name,'output_sha256':hashlib.sha256(out).hexdigest(),
 'motivation':'Owner reports V2.28C remains substantially under-reflective versus PTDE after SPEC-domain linearization plus angular and t9 removal. The next remaining verified PTDE material-response term is the authored P_Metal direct c101 gain 2.5. V2.28D applies that known donor exactly once to the already-linear pre-PBL carrier while leaving dynamic LOD and the DSR environment resource intact.',
 'operator':{
   'V2.28C':'E = Source_D(dynamic LOD cube) * Lspec_linear',
   'V2.28D':'E = Source_D(dynamic LOD cube) * [Lspec_linear * c101_PTDE], c101_PTDE=2.5 for exact base P_Metal',
   'changed':'direct PTDE-authored material gain only; sourced from existing exact P_Metal cb12[0] donor, not arbitrary visual tuning'
 },
 'preserved':['CE18-only c100/diffuse-linear routing','map/world/shared/nonhomologous stock DSR fail-open','exact base P_Metal selector','V2.28A SPEC pow2.2->identity','V2.28B angular/horizon bypass','V2.28C t9 split-sum bypass','native SpecTex RGB/alpha','stock roughness-driven dynamic EnvSpec LOD','native DSR EnvSpec cube','HemEnvLerp deferred/stock','PointLight unchanged'],
 'absent':['fixed LOD3','raw t1 re-sample legacy bridge','inverse-F0 rooting','additional hardcoded gain beyond cb12 donor','cube replacement','chroma manipulation'],
 'patched':patched,'dxbc_valid':'48/48','section_diffs':sec,'changed_bytes':len(changed),
 'expected_runtime':{'C100_REG':18,'TIER0':18,'TIER1':0,'TIER2':0,'UNMAPPED':562,'C101_EXACT_REG':1,'C101_SIBLING_REG':0},
 'final_mod':'PARAM_ONLY; diagnostic addon only'
}
audit.write_text(json.dumps(aud,indent=2),encoding='utf8')
readme.write_text(textwrap.dedent('''\
DSRRL V2.28D — CE18 + EXACT P_METAL PTDE c101 ON LINEAR CARRIER; NO ANGULAR/t9; DYNAMIC LOD RETAINED

V2.28C is still visibly too weak versus the PTDE armor reference. Angular/horizon and t9 are therefore not sufficient explanations of the missing reflection energy.

V2.28D restores the next known PTDE material-response coordinate without arbitrary tuning:

    V2.28C: E = Source_D(dynamic LOD cube) * Lspec_linear
    V2.28D: E = Source_D(dynamic LOD cube) * [Lspec_linear * c101_PTDE]

For exact base P_Metal, c101_PTDE is the authored PTDE material gain 2.5 already carried by the add-on donor sidecar cb12[0]. It is applied exactly once to the current linear pre-PBL signal. This is NOT the rejected V2.25 raw-SpecTex re-sample bridge and does not add a second hardcoded x2.5.

Still retained:
- CE18-only diffuse bridge; map/world materials fail open to stock DSR.
- Exact base P_Metal material gate.
- SPEC-domain pow(2.2)->identity from V2.28A.
- Angular/horizon bypass from V2.28B.
- t9 split-sum bypass from V2.28C.
- Native SpecTex RGB/alpha.
- Stock roughness-driven dynamic EnvSpec LOD. No fixed LOD3.
- Native DSR EnvSpec cubemap.
- HemEnvLerp deferred; PointLight unchanged.

Diagnostic goal: determine whether the missing PTDE-authored direct material gain is the dominant remaining strength residual before touching the dynamic-LOD/resource-representation axis.

Final production mod remains PARAM-only.
'''),encoding='utf8')
with zipfile.ZipFile(zp,'w',zipfile.ZIP_DEFLATED) as z:
    for p in [addon,audit,readme,script]: z.write(p,p.name)
with zipfile.ZipFile(zp) as z: assert z.testzip() is None
print(json.dumps({'zip':str(zp),'zip_sha256':hashlib.sha256(zp.read_bytes()).hexdigest(),'addon':str(addon),'addon_sha256':hashlib.sha256(out).hexdigest(),'changed_bytes':len(changed),'section_diffs':sec,'dxbc':'48/48','patched':patched},indent=2))
