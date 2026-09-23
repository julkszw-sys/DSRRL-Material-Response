import struct, hashlib, json, zipfile, textwrap
from pathlib import Path
ROOT=Path('/mnt/data')
SRC=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_25_PMETAL_CORRECT_LIVE_SPECMAP_LEGACY_RECEIVER.addon64'
base=SRC.read_bytes()
# In the addon the first 24 payloads are the accepted generic V2.9.1 c100/diffuse-linear payloads.
# The second 24 are exact-P_Metal alternates. For the three live P_Metal stable-HemEnv hosts,
# rebuild the alternate from the byte-identical paired generic payload, then change only LOD source -> 3.0.
PAIRS={
  894:{'generic_off':0x3acd0,'alt_off':0xbbb40,'size':19872,'lod_word':1616},
  913:{'generic_off':0x3fa70,'alt_off':0xc08e0,'size':19572,'lod_word':1525},
  932:{'generic_off':0x446f0,'alt_off':0xc5560,'size':18076,'lod_word':1185},
}
MASK=0xffffffff

def rol(x,n): return ((x<<n)|(x>>(32-n))) & MASK
def F(x,y,z): return ((x&y)|((~x)&z)) & MASK
def G(x,y,z): return ((x&z)|(y&(~z))) & MASK
def H(x,y,z): return (x^y^z) & MASK
def I(x,y,z): return (y^(x|(~z))) & MASK
OPS=[
(F,0,7,3614090360),(F,1,12,3905402710),(F,2,17,606105819),(F,3,22,3250441966),(F,4,7,4118548399),(F,5,12,1200080426),(F,6,17,2821735955),(F,7,22,4249261313),(F,8,7,1770035416),(F,9,12,2336552879),(F,10,17,4294925233),(F,11,22,2304563134),(F,12,7,1804603682),(F,13,12,4254626195),(F,14,17,2792965006),(F,15,22,1236535329),
(G,1,5,4129170786),(G,6,9,3225465664),(G,11,14,643717713),(G,0,20,3921069994),(G,5,5,3593408605),(G,10,9,38016083),(G,15,14,3634488961),(G,4,20,3889429448),(G,9,5,568446438),(G,14,9,3275163606),(G,3,14,4107603335),(G,8,20,1163531501),(G,13,5,2850285829),(G,2,9,4243563512),(G,7,14,1735328473),(G,12,20,2368359562),
(H,5,4,4294588738),(H,8,11,2272392833),(H,11,16,1839030562),(H,14,23,4259657740),(H,1,4,2763975236),(H,4,11,1272893353),(H,7,16,4139469664),(H,10,23,3200236656),(H,13,4,681279174),(H,0,11,3936430074),(H,3,16,3572445317),(H,6,23,76029189),(H,9,4,3654602809),(H,12,11,3873151461),(H,15,16,530742520),(H,2,23,3299628645),
(I,0,6,4096336452),(I,7,10,1126891415),(I,14,15,2878612391),(I,5,21,4237533241),(I,12,6,1700485571),(I,3,10,2399980690),(I,10,15,4293915773),(I,1,21,2240044497),(I,8,6,1873313359),(I,15,10,4264355552),(I,6,15,2734768916),(I,13,21,1309151649),(I,4,6,4149444226),(I,11,10,3174756917),(I,2,15,718787259),(I,9,21,3951481745)]
def transform(buf,X):
    a,b,c,d=buf
    def step(fn,a,b,c,d,x,s,ac):
        a=(a+fn(b,c,d)+x+ac)&MASK; a=rol(a,s); return (a+b)&MASK
    for j,(fn,k,s,ac) in enumerate(OPS):
        m=j&3
        if m==0:a=step(fn,a,b,c,d,X[k],s,ac)
        elif m==1:d=step(fn,d,a,b,c,X[k],s,ac)
        elif m==2:c=step(fn,c,d,a,b,X[k],s,ac)
        else:b=step(fn,b,c,d,a,X[k],s,ac)
    return [(buf[0]+a)&MASK,(buf[1]+b)&MASK,(buf[2]+c)&MASK,(buf[3]+d)&MASK]
def dxbc_checksum(data):
    p=data[0x14:]; nbits=(len(p)*8)&MASK; buf=[0x67452301,0xefcdab89,0x98badcfe,0x10325476]; full=len(p)&~63
    for o in range(0,full,64): buf=transform(buf,list(struct.unpack('<16I',p[o:o+64])))
    last=p[full:]
    if len(last)>=56:
        block=last+b'\x80'+b'\x00'*(63-len(last)); buf=transform(buf,list(struct.unpack('<16I',block))); X=[0]*16; X[0]=nbits; X[15]=((nbits>>2)|1)&MASK; buf=transform(buf,X)
    else:
        block=bytearray(struct.pack('<I',nbits)+last); block+=b'\x80'+b'\x00'*(64-len(block)-1); struct.pack_into('<I',block,60,((nbits>>2)|1)&MASK); buf=transform(buf,list(struct.unpack('<16I',block)))
    return struct.pack('<4I',*buf)
def chunks(d):
    assert d[:4]==b'DXBC'; total=struct.unpack_from('<I',d,24)[0]; assert total==len(d); n=struct.unpack_from('<I',d,28)[0]; offs=struct.unpack_from('<'+'I'*n,d,32); out={}
    for o in offs:
        tag=d[o:o+4].decode(); sz=struct.unpack_from('<I',d,o+4)[0]; out[tag]=(o,o+8,sz,d[o+8:o+8+sz])
    return out
def patch_lod3(d,wi):
    c=chunks(d); key='SHEX' if 'SHEX' in c else 'SHDR'; _,p,sz,blob=c[key]; words=list(struct.unpack('<'+'I'*(sz//4),blob))
    # expected baseline operand is temp r2.x (or equivalent two-dword source); V2.25 used immediate float3 here.
    before=(words[wi],words[wi+1])
    assert before==(0x0010003a,0x00000002),(wi,tuple(hex(x) for x in before))
    words[wi]=0x00004001; words[wi+1]=0x40400000
    q=bytearray(d); q[p:p+sz]=struct.pack('<'+'I'*len(words),*words); q[4:20]=b'\0'*16; q[4:20]=dxbc_checksum(bytes(q)); assert dxbc_checksum(bytes(q))==q[4:20]
    return bytes(q),before

def sections(blob):
    e=struct.unpack_from('<I',blob,0x3c)[0]; n=struct.unpack_from('<H',blob,e+6)[0]; osz=struct.unpack_from('<H',blob,e+20)[0]; st=e+24+osz; out=[]
    for i in range(n):
        q=st+i*40; name=blob[q:q+8].rstrip(b'\0').decode(errors='replace'); vs,va,rs,raw=struct.unpack_from('<IIII',blob,q+8); out.append((name,raw,rs))
    return out

def count_valid(blob):
    q=0; total=valid=0
    while True:
        q=blob.find(b'DXBC',q)
        if q<0: break
        if q+28<=len(blob):
            sz=struct.unpack_from('<I',blob,q+24)[0]
            if 1000<sz<100000 and q+sz<=len(blob):
                d=blob[q:q+sz]
                try:
                    chunks(d); total+=1
                    if dxbc_checksum(d)==d[4:20]: valid+=1
                except: pass
        q+=4
    return total,valid

b=bytearray(base); rec=[]
for idx,s in PAIRS.items():
    generic=bytes(base[s['generic_off']:s['generic_off']+s['size']])
    oldalt=bytes(base[s['alt_off']:s['alt_off']+s['size']])
    assert len(generic)==len(oldalt)==s['size']
    new,oldlod=patch_lod3(generic,s['lod_word'])
    b[s['alt_off']:s['alt_off']+s['size']]=new
    rec.append({'shader_index':idx,'generic_v291_offset':hex(s['generic_off']),'pmetal_alt_offset':hex(s['alt_off']),'size':s['size'],'lod_word':s['lod_word'],'semantic_delta':'paired accepted V2.9.1 c100/diffuse-linear payload + only EnvSpec SAMPLE_L LOD source forced to immediate 3.0; complete stock DSR F0/angular/t9 PBL retained','old_alt_sha256':hashlib.sha256(oldalt).hexdigest(),'generic_sha256':hashlib.sha256(generic).hexdigest(),'new_alt_sha256':hashlib.sha256(new).hexdigest()})
# banner only; fit inside existing V2.25 message
old=b'PTDE Material Response V2.25 active: corrected live SpecMap resample before PTDE c101; fixed LOD3; diagnostic only.\0'
pos=bytes(b).find(old); assert pos>=0
msg=b'PTDE Material Response V2.26A active: V2.9.1 material/PBL + P_Metal fixed LOD3 only; diagnostic.\0'
assert len(msg)<=len(old); b[pos:pos+len(old)]=msg+b'\0'*(len(old)-len(msg))
out=bytes(b)
# containment audit
changed=[i for i,(x,y) in enumerate(zip(base,out)) if x!=y]
by={n:sum(raw<=i<raw+rs for i in changed) for n,raw,rs in sections(base)}
assert by.get('.text',0)==0 and by.get('.data',0)==0 and by.get('.pdata',0)==0 and by.get('.reloc',0)==0 and by.get('.rdata',0)==len(changed),by
total,valid=count_valid(out); assert (total,valid)==(48,48),(total,valid)
# Verify target alt now differs from its paired generic by exactly LOD2 words + checksum only inside DXBC.
for idx,s in PAIRS.items():
    g=bytes(out[s['generic_off']:s['generic_off']+s['size']]); a=bytes(out[s['alt_off']:s['alt_off']+s['size']]);
    # structural same total/chunks and target word immediate 3.0
    cg=chunks(g); ca=chunks(a); kg='SHEX' if 'SHEX' in cg else 'SHDR'; ka='SHEX' if 'SHEX' in ca else 'SHDR';
    wg=list(struct.unpack('<'+'I'*(cg[kg][2]//4),cg[kg][3])); wa=list(struct.unpack('<'+'I'*(ca[ka][2]//4),ca[ka][3]));
    dif=[i for i,(x,y) in enumerate(zip(wg,wa)) if x!=y]; assert dif==[s['lod_word'],s['lod_word']+1],(idx,dif)

stem='DSRRL_PTDE_MATERIAL_RESPONSE_V2_26A_PMETAL_V291_PBL_FIXED_LOD3_ONLY'
addon=ROOT/(stem+'.addon64'); audit=ROOT/(stem+'_AUDIT.json'); readme=ROOT/(stem+'_README.txt'); zp=ROOT/(stem+'_RUNTIME_TEST.zip')
addon.write_bytes(out)
aud={'schema':'DSRRL_V2_26A_V291_PBL_FIXED_LOD3_AUDIT_V1','version':'V2.26A','basis':SRC.name,'basis_sha256':hashlib.sha256(base).hexdigest(),'output':addon.name,'output_sha256':hashlib.sha256(out).hexdigest(),'reason':'V2.25 visible green-channel catastrophe invalidates raw DSR SpecTex -> PTDE-linear legacy material bridge. Rebase exact-P_Metal alternate on accepted V2.9.1 c100/diffuse-linear + stock DSR material/F0/PBL and change one receiver coordinate only: EnvSpec LOD3.','target_hosts':[894,913,932],'preserved':['accepted V2.9.1 c100/diffuse-linear response','stock DSR SpecTex->F0 material-domain transform','stock SpecTex RGB chroma','stock SpecTex alpha/roughness input except its downstream dynamic LOD result is bypassed by fixed LOD3','stock angular/horizon','stock BRDF LUT t9 split-sum','native DSR EnvSpec cube','native COLOR0','HemEnvLerp policy unchanged','PointLight unchanged'],'changed_operator':'exact-P_Metal stable-HemEnv EnvSpec SAMPLE_L scalar LOD only: dynamic material LOD -> fixed 3.0','explicitly_absent':['PTDE c101 multiply','raw live SpecMap legacy multiply','material achromatization','cube achromatization','angular bypass','t9 bypass','terminal SAT addition'],'patched':rec,'embedded_dxbc_total':total,'embedded_dxbc_valid':valid,'diffs_by_section':by,'final_mod':'PARAM_ONLY; diagnostic runtime addon only'}
audit.write_text(json.dumps(aud,indent=2),encoding='utf-8')
readme.write_text(textwrap.dedent('''\
DSRRL V2.26A — V2.9.1 MATERIAL/PBL + P_METAL FIXED LOD3 ONLY

This is a hard reset from the failed V2.20–V2.25 legacy-material bridge experiments.

Base material behavior:
- accepted V2.9.1 PTDE-like c100 / diffuse-linear response;
- stock DSR SpecTex -> F0 material-domain behavior;
- stock DSR angular/horizon and BRDF LUT/t9 split-sum;
- native DSR EnvSpec cubemap and COLOR0;
- no PTDE c101=2.5 direct multiply;
- no raw SpecMap re-sample;
- no chroma manipulation;
- no terminal-SAT experiment.

Only P_Metal stable-HemEnv EnvSpec LOD is forced from the DSR material-dependent dynamic value to LOD3, the existing capture-free spatial proxy for PTDE P_Metal slot2.

Purpose: determine the reflection-shape effect while keeping the last visually accepted material/PBL response intact. Diagnostic only. Final project remains PARAM-only.
'''),encoding='utf-8')
with zipfile.ZipFile(zp,'w',zipfile.ZIP_DEFLATED) as z:
    for p in (addon,audit,readme,Path(__file__)): z.write(p,p.name)
with zipfile.ZipFile(zp) as z: assert z.testzip() is None
print(json.dumps({'zip':str(zp),'zip_sha256':hashlib.sha256(zp.read_bytes()).hexdigest(),'addon_sha256':hashlib.sha256(out).hexdigest(),'dxbc':f'{valid}/{total}','changed_bytes':len(changed),'section_diffs':by},indent=2))
