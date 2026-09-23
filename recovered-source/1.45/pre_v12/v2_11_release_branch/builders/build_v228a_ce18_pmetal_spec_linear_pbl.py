import struct, hashlib, json, zipfile, textwrap, collections, importlib.util
from pathlib import Path
ROOT=Path('/mnt/data')
BASE=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_27B_CE18_STOCK_DSR_ENVSPEC_DYNAMIC_LOD.addon64'
base=BASE.read_bytes()
PMETAL='ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b'
REC_OFF=0x81280; REC_STRIDE=0x48; HASH_OFF=0x87a00; HASH_STRIDE=0x50; COUNT=368
# Exact P_Metal live stable HemEnv generic payload slots and SPEC pow sequence starts.
# Sequence in generic shader is:
#   MUL Lspec = SpecTex.rgb * cb10.rgb
#   LOG r1.xyz, r1.xyz
#   MUL r1.xyz, r1.xyz, 2.2
#   EXP F0dst.xyz, r1.xyz
# We preserve MUL and replace LOG+MUL+EXP by NOP padding + MOV F0dst.xyz,r1.xyz.
TARGETS={
  894:{'generic_slot':9,'alt_slot':33,'log_word':1320,'mul_word':1325,'exp_word':1335},
  913:{'generic_slot':10,'alt_slot':34,'log_word':1229,'mul_word':1234,'exp_word':1244},
  932:{'generic_slot':11,'alt_slot':35,'log_word':889,'mul_word':894,'exp_word':904},
}
# checksum implementation
sp=importlib.util.spec_from_file_location('ck',ROOT/'test_dxbc_ck.py'); ck=importlib.util.module_from_spec(sp); sp.loader.exec_module(ck)
NOP=0x0100003a
F22=struct.unpack('<I',struct.pack('<f',2.2))[0]

def chunks(d):
    assert d[:4]==b'DXBC'; total=struct.unpack_from('<I',d,24)[0]; assert total==len(d)
    n=struct.unpack_from('<I',d,28)[0]; offs=struct.unpack_from('<'+'I'*n,d,32); out={}
    for o in offs:
        tag=d[o:o+4].decode(); sz=struct.unpack_from('<I',d,o+4)[0]; out[tag]=(o,o+8,sz,d[o+8:o+8+sz])
    return out

def write_words(d,words):
    c=chunks(d); k='SHEX' if 'SHEX' in c else 'SHDR'; _,p,sz,_=c[k]
    q=bytearray(d); q[p:p+sz]=struct.pack('<'+'I'*len(words),*words); q[4:20]=b'\0'*16; q[4:20]=ck.dxbc_checksum(bytes(q)); assert q[4:20]==ck.dxbc_checksum(bytes(q)); return bytes(q)

def patch_spec_pow_identity(d,s):
    c=chunks(d); k='SHEX' if 'SHEX' in c else 'SHDR'; _,_,sz,blob=c[k]
    w=list(struct.unpack('<'+'I'*(sz//4),blob))
    lw,mw,ew=s['log_word'],s['mul_word'],s['exp_word']
    # exact structural assertions: LOG len5, MUL len10 with immediate 2.2 RGB, EXP len5.
    assert (w[lw]&0x7ff)==47 and ((w[lw]>>24)&0x7f)==5, (lw,hex(w[lw]))
    assert (w[mw]&0x7ff)==56 and ((w[mw]>>24)&0x7f)==10, (mw,hex(w[mw]))
    assert w[mw+6:mw+9]==[F22,F22,F22], [hex(x) for x in w[mw:mw+10]]
    assert (w[ew]&0x7ff)==25 and ((w[ew]>>24)&0x7f)==5, (ew,hex(w[ew]))
    # EXP and source operands must be usable unchanged as MOV destination/source.
    exp=w[ew:ew+5]
    # NOP the LOG and gamma MUL (15 dwords total), then turn EXP opcode into MOV preserving operands.
    w[lw:ew]=[NOP]*(ew-lw)
    exp[0]=(exp[0]&~0x7ff)|54
    w[ew:ew+5]=exp
    nd=write_words(d,w)
    # Verify stream semantics at patch site.
    cc=chunks(nd); kk='SHEX' if 'SHEX' in cc else 'SHDR'; ww=list(struct.unpack('<'+'I'*(cc[kk][2]//4),cc[kk][3]))
    assert all(x==NOP for x in ww[lw:ew])
    assert (ww[ew]&0x7ff)==54 and ((ww[ew]>>24)&0x7f)==5
    return nd

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
assert all(entries[i][1]==entries[i+24][1] for i in range(24))
# Clean all dormant alternates back to their exact paired generic payload first, so no old V2.20-V2.26 experiments survive latent.
b=bytearray(base)
for i in range(24):
    go,gs=entries[i]; ao,asz=entries[i+24]; assert gs==asz
    b[ao:ao+asz]=base[go:go+gs]
# Patch only exact-P_Metal live family alternates.
patched=[]
for shader,s in TARGETS.items():
    go,gs=entries[s['generic_slot']]; ao,asz=entries[s['alt_slot']]; assert gs==asz
    generic=bytes(b[go:go+gs])
    nd=patch_spec_pow_identity(generic,s)
    b[ao:ao+asz]=nd
    patched.append({'shader_index':shader,'generic_slot':s['generic_slot'],'alt_slot':s['alt_slot'],'generic_offset':hex(go),'alt_offset':hex(ao),'size':gs,'spec_log_word':s['log_word'],'spec_mul22_word':s['mul_word'],'spec_exp_to_mov_word':s['exp_word'],'generic_sha256':hashlib.sha256(generic).hexdigest(),'alt_sha256':hashlib.sha256(nd).hexdigest()})
# Enable alternate only for exact base P_Metal record among active CE18.
# V2.27B has all active alternate flags off.
pm_index=None
# Only the 18 CE18 hashes are live in V2.27B; dormant table rows may retain historical alt bits but cannot register.
MATS=json.loads((ROOT/'v291_recon/materials_parsed.json').read_text(encoding='utf8'))['dsr']
first={}
for m in MATS: first.setdefault(m['name'],m)
effective_hashes={m['sha'] for m in first.values()}
active_alt_before=[]
for i in range(COUNT):
    h=base[HASH_OFF+i*HASH_STRIDE:HASH_OFF+i*HASH_STRIDE+64].decode('ascii',errors='ignore')
    alt=struct.unpack_from('<I',base,REC_OFF+i*REC_STRIDE+0x44)[0]
    if h in effective_hashes and alt: active_alt_before.append((i,h))
    if h==PMETAL: pm_index=i
assert pm_index is not None and active_alt_before==[], active_alt_before
struct.pack_into('<I',b,REC_OFF+pm_index*REC_STRIDE+0x44,1)
# banner replace within same padded reservation
old=b'PTDE Material Response V2.27B active: CE18 V2.9.1 c100 only; stock DSR PBL/EnvSpec/dynamic LOD; map/world fail-open.\0'
pos=bytes(b).find(old); assert pos>=0
reserve=len(old)+96
assert all(x==0 for x in b[pos+len(old):pos+reserve])
msg=b'PTDE Material Response V2.28A active: CE18 + exact P_Metal linear pre-PBL F0 input; stock BRDF/EnvSpec/dynamic LOD.\0'
assert len(msg)<=reserve
b[pos:pos+reserve]=msg+b'\0'*(reserve-len(msg))
out=bytes(b)
# static validity: 48 DXBC all valid
out_entries=scan_dxbc(out); assert len(out_entries)==48
for q,sz in out_entries:
    d=out[q:q+sz]; assert ck.dxbc_checksum(d)==d[4:20]
# Exact live alt gating: among effective registered DSR hashes, only P_Metal is enabled. Dormant rows are irrelevant.
flags=[]
for i in range(COUNT):
    h=out[HASH_OFF+i*HASH_STRIDE:HASH_OFF+i*HASH_STRIDE+64].decode('ascii',errors='ignore')
    a=struct.unpack_from('<I',out,REC_OFF+i*REC_STRIDE+0x44)[0]
    if h in effective_hashes and a: flags.append((i,h))
assert flags==[(pm_index,PMETAL)], flags
# Verify target alternates retain generic dynamic LOD instruction (no LOD3) by checking old baseline operand at known prior LOD words.
LOD_WORD={894:1616,913:1525,932:1185}
for shader,s in TARGETS.items():
    go,gs=out_entries[s['generic_slot']]; ao,asz=out_entries[s['alt_slot']]
    gd=out[go:go+gs]; ad=out[ao:ao+asz]
    cg=chunks(gd); ca=chunks(ad); k='SHEX'
    wg=list(struct.unpack('<'+'I'*(cg[k][2]//4),cg[k][3])); wa=list(struct.unpack('<'+'I'*(ca[k][2]//4),ca[k][3]))
    lw=LOD_WORD[shader]
    assert wg[lw:lw+2]==[0x0010003a,0x00000002]
    assert wa[lw:lw+2]==wg[lw:lw+2]
    # semantic differences in SHEX are confined exactly to LOG/MUL/EXP span.
    dif=[i for i,(x,y) in enumerate(zip(wg,wa)) if x!=y]
    allowed=set(range(s['log_word'],s['exp_word']+5))
    assert set(dif)<=allowed and dif, (shader,dif)
# containment in PE sections
changed=[i for i,(x,y) in enumerate(zip(base,out)) if x!=y]
sec={n:sum(raw<=i<raw+rs for i in changed) for n,raw,rs in sections(base)}
assert sec.get('.rdata',0)==len(changed),sec
for n in ['.text','.data','.pdata','.reloc']: assert sec.get(n,0)==0,sec
stem='DSRRL_PTDE_MATERIAL_RESPONSE_V2_28A_CE18_PMETAL_LINEAR_F0_STOCK_PBL_DYNAMIC_LOD'
addon=ROOT/(stem+'.addon64'); audit=ROOT/(stem+'_AUDIT.json'); readme=ROOT/(stem+'_README.txt'); script=Path(__file__); zp=ROOT/(stem+'_RUNTIME_TEST.zip')
addon.write_bytes(out)
aud={
 'schema':'DSRRL_V2_28A_CE18_PMETAL_LINEAR_F0_STOCK_PBL_AUDIT_V1','version':'V2.28A',
 'basis':BASE.name,'basis_sha256':hashlib.sha256(base).hexdigest(),'output':addon.name,'output_sha256':hashlib.sha256(out).hexdigest(),
 'motivation':'V2.27B is visually safe for world/map and character/equipment, but owner reports armor does not yet reach the full PTDE metal effect. This candidate changes one P_Metal material-domain operator only, without fixed LOD, c101 gain, raw SpecTex legacy multiplication, cube change or BRDF bypass.',
 'release_gate':'CE18 unchanged; map/world/shared/nonhomologous fail open to stock DSR',
 'pmetal_gate':{'sha256':PMETAL,'record_index':pm_index,'alternate_enabled_only_for_pmetal':True},
 'operator':{'stock':'Lspec=SpecTex.rgb*cb10.rgb; F0=pow(Lspec,2.2); stock DSR BRDF/EnvSpec','candidate':'Lspec=SpecTex.rgb*cb10.rgb; F0_input:=Lspec (pow 2.2 -> identity); stock DSR BRDF/EnvSpec retained','purpose':'move exact P_Metal material signal toward PTDE linear SpecMap domain while retaining Remaster PBL receiver'},
 'explicitly_preserved':['V2.27B CE18 c100/diffuse-linear bridge','P_Metal cb10 authored DSR value (no x2.5)','native SpecTex RGB/alpha','stock roughness-driven dynamic EnvSpec LOD','stock angular/horizon','stock t9 BRDF split-sum','native DSR EnvSpec cube','map/world stock DSR fail-open','HemEnvLerp deferred/stock','PointLight unchanged'],
 'explicitly_absent':['fixed LOD3','PTDE c101=2.5 direct gain','inverse-F0 rooting','raw SpecTex*c101 legacy receiver','angular bypass','t9 bypass','terminal SAT experiment','chroma manipulation'],
 'patched':patched,'dxbc_valid':'48/48','all_dormant_alternates_rebased_to_generic':True,'section_diffs':sec,'changed_bytes':len(changed),
 'expected_runtime':{'C100_REG':18,'TIER0':18,'TIER1':0,'TIER2':0,'UNMAPPED':562,'C101_EXACT_REG':1,'C101_SIBLING_REG':0,'note':'C101_EXACT_REG=1 is the existing exact-P_Metal alternate-selector record; the V2.28A shader does not consume PTDE c101 gain.'},
 'final_mod':'PARAM_ONLY; addon is renderer diagnostic/prototype only'
}
audit.write_text(json.dumps(aud,indent=2),encoding='utf8')
readme.write_text(textwrap.dedent('''\
DSRRL V2.28A — CE18 SAFE GATE + P_METAL SPEC-DOMAIN LINEARIZATION INSIDE STOCK DSR PBL

V2.27B established the safe release scope: CE18 character/equipment only; map/world fail-open; stock DSR EnvSpec/dynamic LOD. Its remaining visual residual is that armor still does not reach the full PTDE metal response.

V2.28A changes ONE exact-P_Metal material-domain operator:

  stock DSR:
      Lspec = SpecTex.rgb * cb10.rgb
      F0    = pow(Lspec, 2.2)

  V2.28A:
      Lspec = SpecTex.rgb * cb10.rgb
      F0 input := Lspec

The downstream Remaster receiver stays intact: stock roughness/dynamic LOD, stock angular/horizon, stock t9 BRDF split-sum, native DSR EnvSpec cube.

Important: this does NOT apply PTDE c101=2.5, does NOT use fixed LOD3, does NOT multiply raw SpecTex by 2.5, and does NOT bypass PBL. It only removes the DSR SPEC-domain pow(2.2) for exact base P_Metal, using the confirmed pre-PBL carrier already present in the stock shader.

All map/world/shared/nonhomologous materials remain stock DSR. HemEnvLerp remains deferred. PointLight unchanged.

Expected telemetry:
C100_REG=18 TIER0=18 TIER1=0 TIER2=0 UNMAPPED=562
C101_EXACT_REG=1 C101_SIBLING_REG=0
The C101_EXACT_REG counter is only the exact-P_Metal alternate selector in this architecture; the shader does not consume a PTDE c101 gain.

Diagnostic/runtime prototype only. Final DSR Restored Lighting remains PARAM-only.
'''),encoding='utf8')
with zipfile.ZipFile(zp,'w',zipfile.ZIP_DEFLATED) as z:
    for p in [addon,audit,readme,script]: z.write(p,p.name)
with zipfile.ZipFile(zp) as z: assert z.testzip() is None
print(json.dumps({'zip':str(zp),'zip_sha256':hashlib.sha256(zp.read_bytes()).hexdigest(),'addon_sha256':hashlib.sha256(out).hexdigest(),'dxbc':'48/48','pmetal_alt_index':pm_index,'changed_bytes':len(changed),'section_diffs':sec,'target_hosts':list(TARGETS)},indent=2))
