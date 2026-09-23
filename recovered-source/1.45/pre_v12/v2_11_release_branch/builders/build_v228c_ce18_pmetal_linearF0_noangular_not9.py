import struct, hashlib, json, zipfile, textwrap, importlib.util
from pathlib import Path
ROOT=Path('/mnt/data')
BASE=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_28B_CE18_PMETAL_LINEAR_F0_NO_ANGULAR_STOCK_BRDF_DYNAMIC_LOD.addon64'
base=BASE.read_bytes()
PMETAL='ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b'
REC_OFF=0x81280; REC_STRIDE=0x48; HASH_OFF=0x87a00; HASH_STRIDE=0x50; COUNT=368
TARGETS={894:{'alt_slot':33},913:{'alt_slot':34},932:{'alt_slot':35}}
sp=importlib.util.spec_from_file_location('ck',ROOT/'test_dxbc_ck.py'); ck=importlib.util.module_from_spec(sp); sp.loader.exec_module(ck)
NOP=0x0100003a


def chunks(d):
    assert d[:4]==b'DXBC'; total=struct.unpack_from('<I',d,24)[0]; assert total==len(d)
    n=struct.unpack_from('<I',d,28)[0]; offs=struct.unpack_from('<'+'I'*n,d,32); out={}
    for o in offs:
        tag=d[o:o+4].decode(); sz=struct.unpack_from('<I',d,o+4)[0]; out[tag]=(o,o+8,sz,d[o+8:o+8+sz])
    return out


def write_words(d,words):
    c=chunks(d); k='SHEX' if 'SHEX' in c else 'SHDR'; _,p,sz,_=c[k]
    q=bytearray(d); q[p:p+sz]=struct.pack('<'+'I'*len(words),*words)
    q[4:20]=b'\0'*16; q[4:20]=ck.dxbc_checksum(bytes(q)); assert q[4:20]==ck.dxbc_checksum(bytes(q))
    return bytes(q)


def instrs(w):
    i=2; out=[]
    while i<len(w):
        tok=w[i]; ln=(tok>>24)&0x7f; op=tok&0x7ff
        assert ln>0,(i,hex(tok))
        out.append((i,op,ln))
        i+=ln
    return out


def patch_t9_split_sum_to_f0(d):
    c=chunks(d); k='SHEX' if 'SHEX' in c else 'SHDR'; _,_,sz,blob=c[k]
    w=list(struct.unpack('<'+'I'*(sz//4),blob)); ins=instrs(w); pos={i:(op,ln) for i,op,ln in ins}
    matches=[]
    for i,op,ln in ins:
        # DSR BRDF LUT sample: SAMPLE_L t9/s9, followed immediately by MUL then MAD that forms split-sum.
        if op==72 and ln==13:
            ww=w[i:i+ln]
            if ww[7:11]==[0x00107e46,0x00000009,0x00106000,0x00000009]:
                m=i+ln
                if pos.get(m)==(56,7):
                    a=m+7
                    if pos.get(a)==(50,9):
                        matches.append((i,m,a,ww,w[m:m+7],w[a:a+9]))
    assert len(matches)==1, [(x[0],x[1],x[2]) for x in matches]
    samp,mul_i,mad_i,sample,mul,mad=matches[0]
    # The stock MAD is the split-sum output. Keep its destination and first source (the current F0-like vector),
    # replace the t9-dependent MAD with MOV dst,F0, and NOP the t9 sample+helper MUL.
    # Direct-register operands are two dwords each: token + register index.
    assert (mad[1]&0xfff00000)==0x00100000 and (mad[3]&0xfff00000)==0x00100000
    mov=[0x05000036,mad[1],mad[2],mad[3],mad[4]]
    # preserve assignment timing: NOP SAMPLE_L and helper MUL, then MOV at original MAD point, pad remaining 4 dwords.
    w[samp:mad_i]=[NOP]*(mad_i-samp)
    w[mad_i:mad_i+9]=mov+[NOP]*4
    nd=write_words(d,w)
    # audit invariants
    cc=chunks(nd); ww=list(struct.unpack('<'+'I'*(cc[k][2]//4),cc[k][3]))
    assert all(x==NOP for x in ww[samp:mad_i])
    assert (ww[mad_i]&0x7ff)==54 and ((ww[mad_i]>>24)&0x7f)==5
    assert ww[mad_i+1:mad_i+5]==mov[1:]
    assert ww[mad_i+5:mad_i+9]==[NOP]*4
    # t9 SAMPLE_L no longer exists in patched target.
    for j,op,ln in instrs(ww):
        if op==72 and ln==13:
            z=ww[j:j+ln]
            assert z[7:11] != [0x00107e46,0x00000009,0x00106000,0x00000009]
    return nd, {
        't9_sample_word':samp,'split_helper_mul_word':mul_i,'split_sum_mad_word':mad_i,
        'result_dest_reg':mad[2],'f0_source_reg':mad[4],
        'semantic':'BRDF LUT split-sum F0*LUT.x + k*LUT.y -> direct current F0-like vector; dynamic EnvSpec LOD and cube source unchanged'
    }


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
    ao,asz=entries[s['alt_slot']]; old=bytes(b[ao:ao+asz]); nd,meta=patch_t9_split_sum_to_f0(old); b[ao:ao+asz]=nd
    patched.append({'shader_index':shader,'alt_slot':s['alt_slot'],'offset':hex(ao),'size':asz,**meta,'before_sha256':hashlib.sha256(old).hexdigest(),'after_sha256':hashlib.sha256(nd).hexdigest()})

# Update runtime banner within the existing padded reservation.
oldmsg=b'PTDE Material Response V2.28B active: CE18 + exact P_Metal linear F0 + angular bypass only; stock t9/dynamic LOD.\0'
pos=bytes(b).find(oldmsg); assert pos>=0
reserve=len(oldmsg)+96
assert all(x==0 for x in b[pos+len(oldmsg):pos+reserve])
msg=b'PTDE Material Response V2.28C active: CE18 + exact P_Metal linear F0 + no angular + no t9 split-sum; dynamic LOD retained.\0'
assert len(msg)<=reserve
b[pos:pos+reserve]=msg+b'\0'*(reserve-len(msg))
out=bytes(b)

# Validate all embedded DXBC.
outs=scan_dxbc(out); assert len(outs)==48
for q,sz in outs:
    d=out[q:q+sz]; assert ck.dxbc_checksum(d)==d[4:20]

# Exact active alt gate remains P_Metal. Use effective first-entry MTD hashes if corpus is available.
flags=[]
effective_hashes=None
mp=ROOT/'v291_recon/materials_parsed.json'
if mp.exists():
    mats=json.loads(mp.read_text(encoding='utf8'))['dsr']; first={}
    for m in mats: first.setdefault(m['name'],m)
    effective_hashes={m['sha'] for m in first.values()}
for i in range(COUNT):
    h=out[HASH_OFF+i*HASH_STRIDE:HASH_OFF+i*HASH_STRIDE+64].decode('ascii',errors='ignore')
    a=struct.unpack_from('<I',out,REC_OFF+i*REC_STRIDE+0x44)[0]
    if a and (effective_hashes is None or h in effective_hashes): flags.append((i,h))
assert any(h==PMETAL for _,h in flags),flags
if effective_hashes is not None: assert flags==[(next(i for i,h in flags if h==PMETAL),PMETAL)],flags

# Section containment.
changed=[i for i,(x,y) in enumerate(zip(base,out)) if x!=y]
sec={n:sum(raw<=i<raw+rs for i in changed) for n,raw,rs in sections(base)}
assert sec.get('.rdata',0)==len(changed),sec
for n in ['.text','.data','.pdata','.reloc']: assert sec.get(n,0)==0,sec

stem='DSRRL_PTDE_MATERIAL_RESPONSE_V2_28C_CE18_PMETAL_LINEAR_F0_NO_ANGULAR_NO_T9_DYNAMIC_LOD'
addon=ROOT/(stem+'.addon64'); audit=ROOT/(stem+'_AUDIT.json'); readme=ROOT/(stem+'_README.txt'); script=Path(__file__); zp=ROOT/(stem+'_RUNTIME_TEST.zip')
addon.write_bytes(out)
aud={
 'schema':'DSRRL_V2_28C_CE18_PMETAL_NO_T9_AUDIT_V1','version':'V2.28C',
 'basis':BASE.name,'basis_sha256':hashlib.sha256(base).hexdigest(),'output':addon.name,'output_sha256':hashlib.sha256(out).hexdigest(),
 'visual_motivation':'Owner reports V2.28B angular/horizon bypass changed the visible armor reflection only slightly. Therefore angular attenuation is not the dominant remaining strength residual in that tested case. V2.28C isolates the next DSR-only receiver operator: t9 BRDF LUT split-sum.',
 'operator':{
   'V2.28B':'Source_D(dynamic LOD cube) * 1 * [F0_linear*LUT_t9.x + k*LUT_t9.y]',
   'V2.28C':'Source_D(dynamic LOD cube) * 1 * F0_linear',
   'changed':'remove only t9 BRDF LUT/split-sum on exact P_Metal stable HemEnv; retain dynamic roughness LOD and native DSR cube'
 },
 'preserved':['CE18-only c100/diffuse-linear routing','map/world/shared/nonhomologous stock DSR fail-open','exact base P_Metal selector','V2.28A SPEC pow2.2->identity','V2.28B angular/horizon bypass','P_Metal cb10 stock authored value (no x2.5)','native SpecTex RGB/alpha','stock roughness-driven dynamic EnvSpec LOD','native DSR EnvSpec cube','HemEnvLerp deferred/stock','PointLight unchanged'],
 'absent':['fixed LOD3','PTDE c101=2.5 gain','raw SpecTex*c101 legacy multiplication','BRDF LUT t9 split-sum','cube replacement','chroma manipulation'],
 'patched':patched,'dxbc_valid':'48/48','section_diffs':sec,'changed_bytes':len(changed),
 'expected_runtime':{'C100_REG':18,'TIER0':18,'TIER1':0,'TIER2':0,'UNMAPPED':562,'C101_EXACT_REG':1,'C101_SIBLING_REG':0},
 'final_mod':'PARAM_ONLY; diagnostic addon only'
}
audit.write_text(json.dumps(aud,indent=2),encoding='utf8')
readme.write_text(textwrap.dedent('''\
DSRRL V2.28C — CE18 + EXACT P_METAL LINEAR F0 + NO ANGULAR + NO t9 SPLIT-SUM; DYNAMIC LOD RETAINED

Owner result for V2.28B: removing the DSR-only angular/horizon attenuation changed the visible armor reflection only slightly. That operator is therefore not the dominant remaining reflection-strength residual in the tested case.

V2.28C isolates the next DSR-only receiver difference: the t9 BRDF LUT split-sum.

V2.28B exact P_Metal broad EnvSpec tail:
    E = Source_D(dynamic LOD cube) * 1 * [F0_linear * LUT_t9.x + k * LUT_t9.y]

V2.28C:
    E = Source_D(dynamic LOD cube) * 1 * F0_linear

Only the t9-dependent split-sum is removed. This is not an arbitrary brightness multiplier.

Still retained:
- CE18-only diffuse bridge; map/world fail-open to stock DSR.
- Exact base P_Metal material gate.
- Linear pre-PBL P_Metal material signal from V2.28A.
- Angular/horizon bypass from V2.28B.
- Native SpecTex RGB/alpha.
- Stock roughness-driven dynamic EnvSpec LOD.
- Native DSR EnvSpec cubemap.
- No PTDE c101=2.5 gain.
- No fixed LOD3.
- HemEnvLerp deferred; PointLight unchanged.

This diagnostic asks whether the DSR BRDF LUT/split-sum is the dominant receiver term suppressing the PTDE-like broad reflection strength while keeping the DSR environment carrier and dynamic LOD intact.

Final production mod remains PARAM-only.
'''),encoding='utf8')
with zipfile.ZipFile(zp,'w',zipfile.ZIP_DEFLATED) as z:
    for p in [addon,audit,readme,script]: z.write(p,p.name)
with zipfile.ZipFile(zp) as z: assert z.testzip() is None
print(json.dumps({'zip':str(zp),'zip_sha256':hashlib.sha256(zp.read_bytes()).hexdigest(),'addon_sha256':hashlib.sha256(out).hexdigest(),'changed_bytes':len(changed),'section_diffs':sec,'dxbc':'48/48','patched':patched},indent=2))
