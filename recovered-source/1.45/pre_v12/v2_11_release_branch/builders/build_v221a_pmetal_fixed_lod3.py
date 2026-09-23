import struct, hashlib, json, zipfile, textwrap, importlib.util
from pathlib import Path
ROOT=Path('/mnt/data')
BASE=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_20B_PMETAL_TERMINAL_SAT.addon64'
BASE_AUD=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_20A_PMETAL_LEGACY_SPEC_RESPONSE_AUDIT.json'
OUT=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_21A_PMETAL_FIXED_LOD3_LEGACY_RECEIVER.addon64'
AUD=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_21A_PMETAL_FIXED_LOD3_LEGACY_RECEIVER_AUDIT.json'
README=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_21A_PMETAL_FIXED_LOD3_LEGACY_RECEIVER_README.txt'
ZIP=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_21A_PMETAL_FIXED_LOD3_LEGACY_RECEIVER_RUNTIME_TEST.zip'
CK=ROOT/'pmetal_work/dxbc_checksum.py'; MD=ROOT/'pmetal_work/dxbc_minidis.py'
base=BASE.read_bytes(); b=bytearray(base)
BASE_SHA=hashlib.sha256(base).hexdigest()
sp=importlib.util.spec_from_file_location('ck',CK); ck=importlib.util.module_from_spec(sp); sp.loader.exec_module(ck)
sp=importlib.util.spec_from_file_location('md',MD); md=importlib.util.module_from_spec(sp); sp.loader.exec_module(md)
ba=json.load(open(BASE_AUD,encoding='utf-8'))

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
    d=bytes(b[off:off+sz]); dis=md.disasm(d)
    t12=[x for x in dis if x['name']=='sample_l' and any('t12' in o for o in x['ops'])]
    assert len(t12)==1; s=t12[0]
    di=dis.index(s); seq=dis[di:di+6]
    assert [x['name'] for x in seq]==['sample_l','mul','dp3','mad','mul','mul']
    assert seq[3]['sat'] is True
    assert seq[4]['len']==7 and seq[4]['ops'][0]=='r2.x' and seq[4]['ops'][1]=='r2.x' and seq[4]['ops'][2]=='r2.x'
    key,o,p,ssz,words=shex_words(d)
    # 1) DSR material/state-dependent EnvSpec LOD -> fixed LOD3, matching the previously runtime-successful PMR structure.
    raw=s['raw']; assert raw[-2:]==[0x10003a,0x2]
    st=s['word']; words[st+11]=0x4001; words[st+12]=0x40400000  # l(3.0)
    # 2) Bypass DSR angular/horizon multiplier without changing instruction length: square result becomes exactly 1.
    sq=seq[4]; q=sq['word']; before=words[q:q+7]
    assert before==sq['raw']
    words[q:q+7]=[before[0], before[1], before[2], 0x4001, 0x3f800000, 0x4001, 0x3f800000]
    nd=replace_shex_words(d,words)
    nds=md.disasm(nd)
    ns=[x for x in nds if x['name']=='sample_l' and any('t12' in o for o in x['ops'])]
    assert len(ns)==1 and ns[0]['ops'][-1]=='l(3)'
    ndi=nds.index(ns[0]); nseq=nds[ndi:ndi+6]
    assert nseq[4]['ops']==['r2.x','l(1)','l(1)']
    # t9 must remain absent from V2.20A/B legacy-tail bypass.
    assert not any(x['name']=='sample_l' and any('t9' in o for o in x['ops']) for x in nds)
    # terminal SAT from V2.20B must remain.
    outs=[x for x in nds if x['name']=='mov' and x['ops'] and x['ops'][0]=='o0.xyz']
    assert len(outs)==1 and outs[0]['sat'] is True
    b[off:off+sz]=nd
    patched.append({'shader_index':idx,'shader_name':h['shader_name'],'alternate_slot_offset':h['alternate_slot_offset'],'t12_sample_word':st,'lod_before':'r2.w','lod_after':'l(3)','angular_square_word':q,'angular_before':sq['ops'],'angular_after':['r2.x','l(1)','l(1)'],'terminal_sat':True,'dxbc_sha256_before':hashlib.sha256(d).hexdigest(),'dxbc_sha256_after':hashlib.sha256(nd).hexdigest()})

# banner update only
old=b'PTDE Material Response V2.20B active: V2.20A exact P_Metal linear legacy material tail plus PTDE terminal RGB SAT on 894/913/932; native DSR cube/dynamic LOD/angular retained; HemEnvLerp stock DSR.\0'
new=b'PTDE Material Response V2.21A active: exact P_Metal stable HemEnv fixed LOD3 legacy receiver; DSR dynamic LOD/angular/t9 bypassed; PTDE terminal SAT retained; HemEnvLerp stock DSR.\0'
pos=bytes(b).find(old); assert pos>=0 and len(new)<=len(old)
b[pos:pos+len(old)]=new+b'\0'*(len(old)-len(new))

def sections(blob):
    e=struct.unpack_from('<I',blob,0x3c)[0]; n=struct.unpack_from('<H',blob,e+6)[0]; osz=struct.unpack_from('<H',blob,e+20)[0]; st=e+24+osz; out=[]
    for i in range(n):
        o=st+i*40; name=blob[o:o+8].rstrip(b'\0').decode(); vs,va,rs,raw=struct.unpack_from('<IIII',blob,o+8); out.append((name,raw,rs,va,vs))
    return out
secs=sections(base); diffs=[i for i,(a,z) in enumerate(zip(base,b)) if a!=z]
bysec={name:sum(raw<=d<raw+rs for d in diffs) for name,raw,rs,va,vs in secs}
assert len(base)==len(b); assert bysec.get('.text',0)==0 and bysec.get('.data',0)==0 and bysec.get('.pdata',0)==0 and bysec.get('.reloc',0)==0
assert bysec.get('.rdata',0)==len(diffs)
# all embedded DXBC valid
outb=bytes(b); valid=[]; p=0
while True:
    p=outb.find(b'DXBC',p)
    if p<0: break
    if p+28<=len(outb):
        sz=struct.unpack_from('<I',outb,p+24)[0]
        if 1000<sz<100000 and p+sz<=len(outb):
            d=outb[p:p+sz]
            try:
                chunks(d)
                if ck.dxbc_checksum(d)==d[4:20]: valid.append((p,sz))
            except: pass
    p+=4
assert len(valid)==48,len(valid)
OUT.write_bytes(outb); out_sha=hashlib.sha256(outb).hexdigest()
audit={
 'schema':'DSRRL_V2_21A_PMETAL_FIXED_LOD3_LEGACY_RECEIVER_AUDIT_V1','date':'2026-09-15',
 'purpose':'Restore the previously runtime-successful PMR receiver structure on the validated CE18/exact-P_Metal dispatch after V2.20A/B rainbow falsifiers.',
 'basis':{'addon':BASE.name,'sha256':BASE_SHA,'runtime_result':'V2.20B target path live but rainbow persists; terminal SAT insufficient.'},
 'output':{'addon':OUT.name,'sha256':out_sha,'size':len(outb)},
 'delta':{'target_hosts':[894,913,932],'fixed_envspec_lod':3,'dsr_dynamic_lod':'BYPASSED_AT_SAMPLE','dsr_angular_horizon':'BYPASSED_EFFECTIVELY_TO_1','t9_split_sum':'ALREADY_BYPASSED_FROM_V2.20A','ptde_linear_material_tail':'PRESERVED','c101':2.5,'terminal_rgb_sat':'PRESERVED_FROM_V2.20B','native_dsr_cube':'PRESERVED','hemenvlerp':'STOCK_DSR','pointlight':'STOCK_DSR'},
 'patched_hosts':patched,
 'construction':{'embedded_dxbc_count':48,'all_embedded_dxbc_checksums_valid':True,'diffs_by_section':bysec,'only_rdata':True,'MTD_changes':0,'PARAM_changes':0,'EXE_changes':0},
 'interpretation':'This is not a new gain guess. It returns the exact-P_Metal stable-HemEnv receiver toward the already owner-validated PMR/V1.1 structure: native DSR environment carrier at fixed LOD3, linear PTDE material response, no DSR roughness-driven dynamic LOD, no DSR angular/horizon shaping, no t9 split-sum, terminal SAT downstream.',
 'final_mod_note':'Diagnostic Renderer Edition add-on only; production DSRRL remains PARAM-only.'
}
AUD.write_text(json.dumps(audit,indent=2,ensure_ascii=False),encoding='utf-8')
README.write_text(textwrap.dedent(f'''\
DSRRL V2.21A — EXACT P_METAL FIXED-LOD3 LEGACY RECEIVER
========================================================

V2.20B proves terminal SAT is not the missing cause: the exact P_Metal branch still renders rainbow while the selector is live.

V2.21A therefore stops composing the PTDE-linear material tail with the DSR-only roughness receiver. It restores the previously runtime-successful PMR receiver structure on exact base P_Metal stable HemEnv hosts 894 / 913 / 932:

- native DSR EnvSpec cube remains the environment carrier,
- EnvSpec sampling is fixed to LOD3 (PTDE slot2 spatial proxy),
- PTDE-linear SpecMap * c101(2.5) * COLOR0 material factor remains,
- DSR dynamic SpecTex-alpha roughness/LOD is bypassed at the sample,
- DSR angular/horizon multiplier is neutralized to 1,
- DSR t9 BRDF split-sum remains bypassed,
- PTDE terminal RGB SAT remains,
- CE18/V2.9.1 diffuse response remains,
- HemEnvLerp and PointLight remain stock DSR.

This is not arbitrary tuning; it returns to the receiver topology already validated as visually strong/desired in the prior P_Metal PMR work while preserving the safer exact-material gate.

Base SHA-256: {BASE_SHA}
Output addon SHA-256: {out_sha}
'''),encoding='utf-8')
with zipfile.ZipFile(ZIP,'w',zipfile.ZIP_DEFLATED) as z:
    for pth in (OUT,AUD,README,Path(__file__)):
        z.write(pth,pth.name)
with zipfile.ZipFile(ZIP) as z: assert z.testzip() is None
print('addon',out_sha,OUT.stat().st_size)
print('zip',hashlib.sha256(ZIP.read_bytes()).hexdigest(),ZIP.stat().st_size)
print('diffs',len(diffs),bysec)
print(json.dumps(patched,indent=2))
