import struct, hashlib, json, zipfile, textwrap, importlib.util
from pathlib import Path
ROOT=Path('/mnt/data')
BASE=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_28A_CE18_PMETAL_LINEAR_F0_STOCK_PBL_DYNAMIC_LOD.addon64'
base=BASE.read_bytes()
PMETAL='ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b'
REC_OFF=0x81280; REC_STRIDE=0x48; HASH_OFF=0x87a00; HASH_STRIDE=0x50; COUNT=368
TARGETS={894:{'alt_slot':33},913:{'alt_slot':34},932:{'alt_slot':35}}
sp=importlib.util.spec_from_file_location('ck',ROOT/'test_dxbc_ck.py'); ck=importlib.util.module_from_spec(sp); sp.loader.exec_module(ck)
NOP=0x0100003a
F13=struct.unpack('<I',struct.pack('<f',1.3))[0]
F1=struct.unpack('<I',struct.pack('<f',1.0))[0]

def chunks(d):
    assert d[:4]==b'DXBC'; total=struct.unpack_from('<I',d,24)[0]; assert total==len(d)
    n=struct.unpack_from('<I',d,28)[0]; offs=struct.unpack_from('<'+'I'*n,d,32); out={}
    for o in offs:
        tag=d[o:o+4].decode(); sz=struct.unpack_from('<I',d,o+4)[0]; out[tag]=(o,o+8,sz,d[o+8:o+8+sz])
    return out

def write_words(d,words):
    c=chunks(d); k='SHEX' if 'SHEX' in c else 'SHDR'; _,p,sz,_=c[k]
    q=bytearray(d); q[p:p+sz]=struct.pack('<'+'I'*len(words),*words); q[4:20]=b'\0'*16; q[4:20]=ck.dxbc_checksum(bytes(q)); assert q[4:20]==ck.dxbc_checksum(bytes(q)); return bytes(q)

def instrs(w):
    i=2; out=[]
    while i<len(w):
        tok=w[i]; ln=(tok>>24)&0x7f; op=tok&0x7ff
        assert ln>0, (i,hex(tok))
        out.append((i,op,ln))
        i+=ln
    return out

def patch_angular_to_one(d):
    c=chunks(d); k='SHEX' if 'SHEX' in c else 'SHDR'; _,_,sz,blob=c[k]
    w=list(struct.unpack('<'+'I'*(sz//4),blob)); ins=instrs(w)
    pos={i:(op,ln) for i,op,ln in ins}
    matches=[]
    for i,op,ln in ins:
        if op==50 and ln==9 and F13 in w[i:i+ln] and F1 in w[i:i+ln]:
            # expected angular chain: MAD_SAT -> MUL square -> MUL apply
            n1=i+ln
            if pos.get(n1)==(56,7):
                n2=n1+7
                if pos.get(n2)==(56,7):
                    sq=w[n1:n1+7]; apply=w[n2:n2+7]
                    # square must be x*x same register and next mul must consume same scalar register.
                    if sq[4]==sq[6] and sq[2]==sq[4] and apply[6]==sq[2]:
                        matches.append((i,n1,n2,sq,apply))
    assert len(matches)==1, [(a,b,c) for a,b,c,_,__ in matches]
    mad,sq_i,apply_i,sq,apply=matches[0]
    # Replace square with MOV scalar=1 and two NOPs. Keep dp3/mad present but their result is overwritten.
    mov=[0x05000036, sq[1], sq[2], 0x00004001, F1]
    w[sq_i:sq_i+7]=mov+[NOP,NOP]
    nd=write_words(d,w)
    cc=chunks(nd); ww=list(struct.unpack('<'+'I'*(cc[k][2]//4),cc[k][3]))
    assert (ww[sq_i]&0x7ff)==54 and ((ww[sq_i]>>24)&0x7f)==5
    assert ww[sq_i+3:sq_i+5]==[0x00004001,F1]
    assert ww[sq_i+5:sq_i+7]==[NOP,NOP]
    assert ww[apply_i:apply_i+7]==apply
    return nd, {'mad_word':mad,'angular_square_word':sq_i,'angular_apply_word':apply_i,'dest_reg_word':sq[2]}

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
    ao,asz=entries[s['alt_slot']]; old=bytes(b[ao:ao+asz]); nd,meta=patch_angular_to_one(old); b[ao:ao+asz]=nd
    patched.append({'shader_index':shader,'alt_slot':s['alt_slot'],'offset':hex(ao),'size':asz,**meta,'before_sha256':hashlib.sha256(old).hexdigest(),'after_sha256':hashlib.sha256(nd).hexdigest()})
# banner
oldmsg=b'PTDE Material Response V2.28A active: CE18 + exact P_Metal linear pre-PBL F0 input; stock BRDF/EnvSpec/dynamic LOD.\0'
pos=bytes(b).find(oldmsg); assert pos>=0
reserve=len(oldmsg)+96
assert all(x==0 for x in b[pos+len(oldmsg):pos+reserve])
msg=b'PTDE Material Response V2.28B active: CE18 + exact P_Metal linear F0 + angular bypass only; stock t9/dynamic LOD.\0'
assert len(msg)<=reserve
b[pos:pos+reserve]=msg+b'\0'*(reserve-len(msg))
out=bytes(b)
# validate all DXBC
outs=scan_dxbc(out); assert len(outs)==48
for q,sz in outs:
    d=out[q:q+sz]; assert ck.dxbc_checksum(d)==d[4:20]
# verify effective alt gate still exact P_Metal only
flags=[]
for i in range(COUNT):
    h=out[HASH_OFF+i*HASH_STRIDE:HASH_OFF+i*HASH_STRIDE+64].decode('ascii',errors='ignore')
    a=struct.unpack_from('<I',out,REC_OFF+i*REC_STRIDE+0x44)[0]
    if a: flags.append((i,h))
# There may be dormant historical alt flags, but exact active CE18 architecture retained from V2.28A. Require P_Metal flag exists.
assert any(h==PMETAL for _,h in flags)
# section containment
changed=[i for i,(x,y) in enumerate(zip(base,out)) if x!=y]
sec={n:sum(raw<=i<raw+rs for i in changed) for n,raw,rs in sections(base)}
assert sec.get('.rdata',0)==len(changed),sec
for n in ['.text','.data','.pdata','.reloc']: assert sec.get(n,0)==0,sec
stem='DSRRL_PTDE_MATERIAL_RESPONSE_V2_28B_CE18_PMETAL_LINEAR_F0_NO_ANGULAR_STOCK_BRDF_DYNAMIC_LOD'
addon=ROOT/(stem+'.addon64'); audit=ROOT/(stem+'_AUDIT.json'); readme=ROOT/(stem+'_README.txt'); script=Path(__file__); zp=ROOT/(stem+'_RUNTIME_TEST.zip')
addon.write_bytes(out)
aud={
 'schema':'DSRRL_V2_28B_CE18_PMETAL_LINEAR_F0_NO_ANGULAR_AUDIT_V1','version':'V2.28B',
 'basis':BASE.name,'basis_sha256':hashlib.sha256(base).hexdigest(),'output':addon.name,'output_sha256':hashlib.sha256(out).hexdigest(),
 'visual_motivation':'Owner comparison places PTDE reference on lower display and reports substantially stronger armor reflection than current candidate. V2.28A still retains a DSR-only angular/horizon attenuation absent from PTDE legacy HemEnv; V2.28B isolates that one downstream operator without changing source gain, roughness LOD or BRDF LUT.',
 'operator':{'V2.28A':'linear pre-PBL P_Metal F0 input + stock angular + stock t9 + stock dynamic LOD','V2.28B':'same, but angular/horizon scalar forced to 1 immediately before cube multiplication; t9 and dynamic LOD remain stock'},
 'preserved':['CE18-only c100/diffuse-linear gate','map/world/shared/nonhomologous stock DSR fail-open','exact base P_Metal selector','V2.28A SPEC pow2.2->identity','P_Metal cb10 stock authored value (no x2.5)','native SpecTex RGB/alpha','stock roughness-driven dynamic EnvSpec LOD','stock t9 BRDF split-sum','native DSR EnvSpec cube','HemEnvLerp deferred/stock','PointLight unchanged'],
 'absent':['fixed LOD3','PTDE c101 2.5 gain','raw SpecTex*c101 legacy receiver','t9 bypass','cube replacement','chroma manipulation'],
 'patched':patched,'dxbc_valid':'48/48','section_diffs':sec,'changed_bytes':len(changed),
 'expected_runtime':{'C100_REG':18,'TIER0':18,'TIER1':0,'TIER2':0,'UNMAPPED':562,'C101_EXACT_REG':1,'C101_SIBLING_REG':0},
 'final_mod':'PARAM_ONLY; diagnostic addon only'
}
audit.write_text(json.dumps(aud,indent=2),encoding='utf8')
readme.write_text(textwrap.dedent('''\
DSRRL V2.28B — CE18 + EXACT P_METAL LINEAR F0 + ANGULAR/HORIZON BYPASS ONLY

The owner comparison shows PTDE armor has materially stronger broad reflection than the current result. V2.28A already removes only the P_Metal SPEC-domain pow(2.2), but still retains two DSR-only receiver factors absent from PTDE: angular/horizon attenuation and t9 BRDF split-sum.

V2.28B isolates the first of those factors only.

Preserved from V2.28A:
- CE18-only diffuse-linear bridge; map/world fail-open.
- Exact P_Metal material gate.
- Linear pre-PBL P_Metal material signal (SPEC pow2.2 -> identity).
- Native SpecTex, stock roughness-driven dynamic EnvSpec LOD.
- Stock t9 BRDF LUT/split-sum.
- Native DSR EnvSpec cube.
- No PTDE c101=2.5 gain and no fixed LOD3.

Only change:
    DSR angular/horizon scalar A_D = sat(1 + 1.3*d)^2
becomes
    A_test = 1
immediately before the EnvSpec cube term is multiplied by it.

This is an operator-isolation diagnostic, not an arbitrary brightness gain. If PTDE-like reflection strength moves in the correct direction without the prior artifacts, the DSR-only angular/horizon attenuation is a decision-relevant part of the remaining receiver mismatch. t9 remains untouched for the next isolated step if needed.

HemEnvLerp remains deferred. PointLight unchanged. Final production mod remains PARAM-only.
'''),encoding='utf8')
with zipfile.ZipFile(zp,'w',zipfile.ZIP_DEFLATED) as z:
    for p in [addon,audit,readme,script]: z.write(p,p.name)
with zipfile.ZipFile(zp) as z: assert z.testzip() is None
print(json.dumps({'zip':str(zp),'zip_sha256':hashlib.sha256(zp.read_bytes()).hexdigest(),'addon_sha256':hashlib.sha256(out).hexdigest(),'changed_bytes':len(changed),'section_diffs':sec,'dxbc':'48/48','patched':patched},indent=2))
