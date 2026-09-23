import struct, hashlib, json, zipfile, textwrap, importlib.util
from pathlib import Path
ROOT=Path('/mnt/data')
BASE=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_20A_PMETAL_LEGACY_SPEC_RESPONSE.addon64'
BASE_AUD=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_20A_PMETAL_LEGACY_SPEC_RESPONSE_AUDIT.json'
OUT=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_20B_PMETAL_TERMINAL_SAT.addon64'
AUD=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_20B_PMETAL_TERMINAL_SAT_AUDIT.json'
README=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_20B_PMETAL_TERMINAL_SAT_README.txt'
ZIP=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_20B_PMETAL_TERMINAL_SAT_RUNTIME_TEST.zip'
CK=ROOT/'pmetal_work/dxbc_checksum.py'; MD=ROOT/'pmetal_work/dxbc_minidis.py'
base=BASE.read_bytes(); b=bytearray(base)
BASE_SHA=hashlib.sha256(base).hexdigest(); assert BASE_SHA=='68458b4277d50c8e712ed754d5d6ead8a2b6021d7260cac0928d005f01b0d8c4'
ba=json.load(open(BASE_AUD,encoding='utf-8'))
sp=importlib.util.spec_from_file_location('ck',CK); ck=importlib.util.module_from_spec(sp); sp.loader.exec_module(ck)
sp=importlib.util.spec_from_file_location('md',MD); md=importlib.util.module_from_spec(sp); sp.loader.exec_module(md)

def chunks(data):
    assert data[:4]==b'DXBC'; total=struct.unpack_from('<I',data,24)[0]; assert total==len(data)
    n=struct.unpack_from('<I',data,28)[0]; offs=struct.unpack_from('<'+'I'*n,data,32)
    out={}
    for o in offs:
        tag=data[o:o+4].decode(); sz=struct.unpack_from('<I',data,o+4)[0]; out[tag]=(o,o+8,sz,data[o+8:o+8+sz])
    return out

def shex_words(data):
    c=chunks(data); key='SHEX' if 'SHEX' in c else 'SHDR'; o,p,sz,blob=c[key]
    return key,o,p,sz,list(struct.unpack('<'+'I'*(len(blob)//4),blob))

def replace_shex_words(data, words):
    c=chunks(data); key='SHEX' if 'SHEX' in c else 'SHDR'; o,p,sz,blob=c[key]
    assert len(words)*4==sz
    q=bytearray(data); q[p:p+sz]=struct.pack('<'+'I'*len(words),*words)
    q[4:20]=b'\0'*16; q[4:20]=ck.dxbc_checksum(bytes(q)); assert ck.dxbc_checksum(bytes(q))==bytes(q[4:20])
    return bytes(q)

patched=[]
for h in ba['alternate_array']['pmr_patched_hosts']:
    idx=h['shader_index']; off=int(h['alternate_slot_offset'],16); sz=h['size']
    d=bytes(b[off:off+sz]); assert hashlib.sha256(d).hexdigest()==h['dxbc_sha256']
    dis=md.disasm(d)
    outs=[x for x in dis if x['name']=='mov' and x['ops'] and x['ops'][0]=='o0.xyz']
    assert len(outs)==1, (idx,[(x['word'],x['name'],x['sat'],x['ops']) for x in outs])
    x=outs[0]; assert not x['sat'] and x['len']==5
    key,o,p,ssz,words=shex_words(d)
    before=words[x['word']]; assert (before & 0x2000)==0
    words[x['word']]=before|0x2000
    nd=replace_shex_words(d,words)
    nds=md.disasm(nd)
    no=[y for y in nds if y['name']=='mov' and y['ops'] and y['ops'][0]=='o0.xyz']
    assert len(no)==1 and no[0]['sat'] is True
    # exactly instruction-token bit plus checksum bytes may differ
    _,_,_,_,w0=shex_words(d); _,_,_,_,w1=shex_words(nd)
    dw=[i for i,(a,z) in enumerate(zip(w0,w1)) if a!=z]; assert dw==[x['word']]
    b[off:off+sz]=nd
    patched.append({'shader_index':idx,'shader_name':h['shader_name'],'alternate_slot_offset':h['alternate_slot_offset'],'terminal_word':x['word'],'before_token':hex(before),'after_token':hex(before|0x2000),'dxbc_sha256_before':h['dxbc_sha256'],'dxbc_sha256_after':hashlib.sha256(nd).hexdigest(),'checksum_after':nd[4:20].hex()})

# Update banner without code changes if exact old string fits in place.
old=b'PTDE Material Response V2.20A active: CE18 V2.9.1 diffuse gate; exact P_Metal stable HemEnv uses PTDE-linear SpecMap*c101*COLOR0 material factor in native DSR EnvSpec sampling/angular path; t9 BRDF split-sum bypassed only there; HemEnvLerp stock DSR.\0'
new=b'PTDE Material Response V2.20B active: V2.20A exact P_Metal linear legacy material tail plus PTDE terminal RGB SAT on 894/913/932; native DSR cube/dynamic LOD/angular retained; HemEnvLerp stock DSR.\0'
pos=bytes(b).find(old); assert pos>=0 and len(new)<=len(old)
b[pos:pos+len(old)]=new+b'\0'*(len(old)-len(new))

# PE section diff audit

def sections(blob):
    e=struct.unpack_from('<I',blob,0x3c)[0]; n=struct.unpack_from('<H',blob,e+6)[0]; osz=struct.unpack_from('<H',blob,e+20)[0]; st=e+24+osz; out=[]
    for i in range(n):
        o=st+i*40; name=blob[o:o+8].rstrip(b'\0').decode(); vs,va,rs,raw=struct.unpack_from('<IIII',blob,o+8); out.append((name,raw,rs,va,vs))
    return out
secs=sections(base); diffs=[i for i,(a,z) in enumerate(zip(base,b)) if a!=z]
bysec={name:sum(raw<=d<raw+rs for d in diffs) for name,raw,rs,va,vs in secs}
assert len(base)==len(b); assert bysec.get('.text',0)==0 and bysec.get('.data',0)==0 and bysec.get('.pdata',0)==0 and bysec.get('.reloc',0)==0
assert bysec.get('.rdata',0)==len(diffs)
# Validate all embedded DXBC checksums
outb=bytes(b); valid=[]; p=0
while True:
    p=outb.find(b'DXBC',p)
    if p<0:break
    if p+28<=len(outb):
        sz=struct.unpack_from('<I',outb,p+24)[0]
        if 1000<sz<100000 and p+sz<=len(outb):
            d=outb[p:p+sz]
            try:
                chunks(d)
                if ck.dxbc_checksum(d)==d[4:20]: valid.append((p,sz))
            except:pass
    p+=4
assert len(valid)==48,len(valid)
OUT.write_bytes(outb); out_sha=hashlib.sha256(outb).hexdigest()

audit={
 'schema':'DSRRL_V2_20B_PMETAL_TERMINAL_SAT_AUDIT_V1','date':'2026-09-15',
 'purpose':'One-operator successor to runtime-falsified V2.20A: preserve exact P_Metal linear legacy material tail and every V2.20A input/operator, add only the confirmed PTDE terminal RGB SAT on target stable HemEnv alternates.',
 'basis':{'addon':BASE.name,'sha256':BASE_SHA,'runtime_result':'V2.20A exact route live but extreme rainbow/overdriven; angular factor statically confirmed attenuation-only [0,1].'},
 'output':{'addon':OUT.name,'sha256':out_sha,'size':len(outb)},
 'delta':{'target_hosts':[894,913,932],'operator':'terminal RGB SAT','semantic_change':'final mov o0.xyz -> mov_sat o0.xyz','per_target_shex_dword_changes':1,'other_shader_math':'byte-identical to V2.20A','registry':'byte-identical to V2.20A','c101':2.5,'native_dsr_cube':True,'dynamic_lod':True,'dsr_angular':'retained','hemenvlerp':'stock DSR','pointlight':'stock DSR'},
 'patched_hosts':patched,
 'construction':{'embedded_dxbc_count':48,'all_embedded_dxbc_checksums_valid':True,'diffs_by_section':bysec,'only_rdata':True,'MTD_changes':0,'PARAM_changes':0,'EXE_changes':0},
 'interpretation':'If the Power-Rangers HDR/chroma blowout collapses materially while geometry/reflection structure remains, missing PTDE terminal SAT was decision-relevant. If saturated patchwork remains, next isolate DSR dynamic LOD/native carrier and then SpecMap/COLOR0 input content homology. Do not tune c101 arbitrarily.',
 'final_mod_note':'Diagnostic Renderer Edition add-on only; production DSRRL remains PARAM-only.'
}
AUD.write_text(json.dumps(audit,indent=2,ensure_ascii=False),encoding='utf-8')
README.write_text(textwrap.dedent(f'''\
DSRRL V2.20B — EXACT P_METAL + PTDE TERMINAL RGB SAT
=====================================================

This is the controlled one-operator successor to V2.20A.

V2.20A routing was live, but the owner runtime image produced the extreme rainbow/"Power Rangers" metal response. Direct DXBC review also shows the retained DSR angular term is attenuation-only because its 1+1.3*d MAD is SAT-modified before squaring; it cannot be the >1 energy amplifier.

V2.20B therefore changes only the next exact missing PTDE operator:

    final stable-HemEnv P_Metal RGB write: MOV -> MOV_SAT

on exact P_Metal alternate hosts 894 / 913 / 932.

Everything else is byte-for-byte V2.20A at shader math / registry level:
- exact P_Metal gate,
- c101_PTDE = 2.5,
- V2.9.1/CE18 diffuse response,
- native DSR EnvSpec cube,
- native DSR dynamic roughness LOD,
- DSR angular/horizon attenuation,
- PTDE-linear SpecMap*c101*COLOR0 replacement of the t9 split-sum tail,
- stock DSR HemEnvLerp,
- stock PointLight and downstream.

This is not gain tuning. PTDE terminal RGB SAT is a confirmed operator on the paired HemEnv family.

Base SHA-256: {BASE_SHA}
Output addon SHA-256: {out_sha}
'''),encoding='utf-8')
with zipfile.ZipFile(ZIP,'w',zipfile.ZIP_DEFLATED) as z:
    for p in (OUT,AUD,README,Path(__file__)):
        z.write(p,p.name)
with zipfile.ZipFile(ZIP) as z: assert z.testzip() is None
print('addon',out_sha,OUT.stat().st_size)
print('zip',hashlib.sha256(ZIP.read_bytes()).hexdigest(),ZIP.stat().st_size)
print('diffs',len(diffs),bysec)
print('patched',patched)
