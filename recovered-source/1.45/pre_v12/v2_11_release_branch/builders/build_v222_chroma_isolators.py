import struct, hashlib, json, zipfile, textwrap, importlib.util
from pathlib import Path
ROOT=Path('/mnt/data')
BASE=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_21A_PMETAL_FIXED_LOD3_LEGACY_RECEIVER.addon64'
BASE_AUD=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_21A_PMETAL_FIXED_LOD3_LEGACY_RECEIVER_AUDIT.json'
CK=ROOT/'pmetal_work/dxbc_checksum.py'; MD=ROOT/'pmetal_work/dxbc_minidis.py'
sp=importlib.util.spec_from_file_location('ck',CK); ck=importlib.util.module_from_spec(sp); sp.loader.exec_module(ck)
sp=importlib.util.spec_from_file_location('md',MD); md=importlib.util.module_from_spec(sp); sp.loader.exec_module(md)
ba=json.load(open(BASE_AUD,encoding='utf-8')); base=BASE.read_bytes()
SIZES={894:19872,913:19572,932:18076}

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

def replace_shex_words(data,words):
    c=chunks(data); key='SHEX' if 'SHEX' in c else 'SHDR'; o,p,sz,blob=c[key]
    q=bytearray(data); q[p:p+sz]=struct.pack('<'+'I'*len(words),*words); q[4:20]=b'\0'*16; q[4:20]=ck.dxbc_checksum(bytes(q))
    assert ck.dxbc_checksum(bytes(q))==bytes(q[4:20]); return bytes(q)

def op_slices(raw,nops):
    words=raw; pos=1; out=[]
    def consume(pos):
        st=pos; tok=words[pos]; pos+=1
        if tok>>31:
            while True:
                e=words[pos]; pos+=1
                if not e>>31: break
        ncomp=tok&3; typ=(tok>>12)&0xff; dim=(tok>>20)&3
        if typ==4:
            pos += 1 if ncomp==1 else 4 if ncomp==2 else 0; return st,pos
        for d in range(dim):
            rep=(tok>>(22+3*d))&7
            if rep==0: pos+=1
            elif rep==1: pos+=2
            elif rep==2: _,pos=consume(pos)
            elif rep==3: pos+=1; _,pos=consume(pos)
            elif rep==4: pos+=2; _,pos=consume(pos)
            else: raise RuntimeError(rep)
        return st,pos
    for _ in range(nops):
        st,pos=consume(pos); out.append((st,pos,words[st:pos]))
    assert pos==len(words); return out

def patch_banner(blob,tag):
    b=bytearray(blob)
    old=b'PTDE Material Response V2.21A active: exact P_Metal stable HemEnv fixed LOD3 legacy receiver; DSR dynamic LOD/angular/t9 bypassed; PTDE terminal SAT retained; HemEnvLerp stock DSR.\0'
    msg=(f'PTDE Material Response {tag} active: exact P_Metal fixed-LOD3 chroma isolator; diagnostic only; HemEnvLerp stock DSR.\0').encode()
    p=bytes(b).find(old); assert p>=0 and len(msg)<=len(old); b[p:p+len(old)]=msg+b'\0'*(len(old)-len(msg)); return bytes(b)

def build(kind):
    b=bytearray(base); rec=[]
    for h in ba['patched_hosts']:
        idx=h['shader_index']; off=int(h['alternate_slot_offset'],16); sz=SIZES[idx]
        d=bytes(b[off:off+sz]); dis=md.disasm(d); key,o,p,ssz,words=shex_words(d)
        if kind=='CUBE_ACHROMA':
            # Find t12 sample, then first cube*profile multiplication. Existing source r?.yyzw contains sampled RGB.
            ti=[i for i,x in enumerate(dis) if x['name']=='sample_l' and any('t12' in q for q in x['ops'])]; assert len(ti)==1
            i=ti[0]; mul=dis[i+1]; assert mul['name']=='mul'
            ops=op_slices(mul['raw'],3); src=ops[1]
            # yyzw swizzle token -> yyyy, preserving register index. 0x100e56 -> 0x100556.
            assert src[2][0]==0x100e56, (idx,mul['ops'],src)
            inst=mul['word']; words[inst+src[0]]=0x100556
            semantic=f'{mul["ops"][1]} -> same register .yyyy at cube RGB profile multiply'
        else:
            # Achromatize material carriers only: SpecMap RGB -> RRR and COLOR0 RGB -> XXX.
            mi=[i for i,x in enumerate(dis) if x['name']=='mul' and any('cb12[0]' in q for q in x['ops'])]; assert len(mi)==1
            i=mi[0]; m1=dis[i]; m2=dis[i+1]; assert m2['name']=='mul'
            s1=op_slices(m1['raw'],3); s2=op_slices(m2['raw'],3)
            assert s1[1][2][0]==0x100796, (idx,m1['ops'],s1)
            assert s2[2][2][0]==0x101246, (idx,m2['ops'],s2)
            words[m1['word']+s1[1][0]]=0x100556  # temp spec source yyyy
            words[m2['word']+s2[2][0]]=0x101006  # COLOR0 xxxx
            semantic=f'SpecMap {m1["ops"][1]} -> .yyyy; COLOR0 {m2["ops"][2]} -> .xxxx'
        nd=replace_shex_words(d,words); nds=md.disasm(nd)
        assert not any(x['name']=='sample_l' and any('t9' in q for q in x['ops']) for x in nds)
        outs=[x for x in nds if x['name']=='mov' and x['ops'] and x['ops'][0]=='o0.xyz']; assert len(outs)==1 and outs[0]['sat']
        b[off:off+sz]=nd
        rec.append({'shader_index':idx,'offset':hex(off),'semantic_delta':semantic,'sha_before':hashlib.sha256(d).hexdigest(),'sha_after':hashlib.sha256(nd).hexdigest()})
    tag='V2.22A' if kind=='CUBE_ACHROMA' else 'V2.22B'
    b=bytearray(patch_banner(bytes(b),tag))
    # PE diff containment
    def sections(blob):
        e=struct.unpack_from('<I',blob,0x3c)[0]; n=struct.unpack_from('<H',blob,e+6)[0]; osz=struct.unpack_from('<H',blob,e+20)[0]; st=e+24+osz; out=[]
        for i in range(n):
            q=st+i*40; name=blob[q:q+8].rstrip(b'\0').decode(); vs,va,rs,raw=struct.unpack_from('<IIII',blob,q+8); out.append((name,raw,rs))
        return out
    diffs=[i for i,(a,z) in enumerate(zip(base,b)) if a!=z]; by={n:sum(raw<=d<raw+rs for d in diffs) for n,raw,rs in sections(base)}
    assert by.get('.text',0)==0 and by.get('.data',0)==0 and by.get('.pdata',0)==0 and by.get('.reloc',0)==0 and by.get('.rdata',0)==len(diffs)
    # validate all 48 embedded DXBC
    outb=bytes(b); valid=0; q=0
    while True:
        q=outb.find(b'DXBC',q)
        if q<0: break
        if q+28<=len(outb):
            sz=struct.unpack_from('<I',outb,q+24)[0]
            if 1000<sz<100000 and q+sz<=len(outb):
                d=outb[q:q+sz]
                try:
                    chunks(d)
                    if ck.dxbc_checksum(d)==d[4:20]: valid+=1
                except: pass
        q+=4
    assert valid==48,valid
    stem='DSRRL_PTDE_MATERIAL_RESPONSE_'+tag.replace('.','_')+('_PMETAL_CUBE_CHROMA_ISOLATOR' if kind=='CUBE_ACHROMA' else '_PMETAL_MATERIAL_CHROMA_ISOLATOR')
    addon=ROOT/(stem+'.addon64'); audit=ROOT/(stem+'_AUDIT.json'); readme=ROOT/(stem+'_README.txt'); zpath=ROOT/(stem+'_RUNTIME_TEST.zip')
    addon.write_bytes(outb); sha=hashlib.sha256(outb).hexdigest()
    audit.write_text(json.dumps({'schema':'DSRRL_V2_22_CHROMA_ISOLATOR_AUDIT_V1','version':tag,'kind':kind,'basis':BASE.name,'basis_sha256':hashlib.sha256(base).hexdigest(),'output':addon.name,'output_sha256':sha,'target_hosts':[894,913,932],'receiver_invariants':['exact P_Metal gate','fixed LOD3','DSR angular bypassed','t9 split-sum bypassed','terminal SAT','CE18 diffuse baseline','HemEnvLerp stock','PointLight stock'],'single_diagnostic_axis':('native DSR cube RGB chroma collapsed to sampled R channel before EnvSpec profile multiplication' if kind=='CUBE_ACHROMA' else 'material chroma collapsed: SpecMap RGB->R and COLOR0 RGB->R, native DSR cube chroma untouched'),'patched':rec,'embedded_dxbc_valid':valid,'diffs_by_section':by,'final_mod':'PARAM_ONLY; diagnostic addon only'},indent=2),encoding='utf-8')
    readme.write_text(textwrap.dedent(f'''DSRRL {tag} — {kind}\n\nPurpose: isolate the chroma carrier responsible for the persistent rainbow P_Metal response after V2.21A.\n\n{('A collapses only the sampled native DSR EnvSpec cube RGB to one channel before the existing profile multiplication. SpecMap/COLOR0 remain chromatic.' if kind=='CUBE_ACHROMA' else 'B leaves the native DSR EnvSpec cube fully chromatic and collapses only the material chroma carriers: SpecMap RGB and COLOR0 RGB.') }\n\nEverything else is V2.21A: exact P_Metal stable HemEnv, fixed LOD3, no DSR angular/t9 tail, terminal SAT, CE18 diffuse baseline, stock HemEnvLerp and PointLight.\n\nInterpretation:\n- If A removes the rainbow while B does not, the dominant chroma carrier is the native DSR EnvSpec cube/resource representation.\n- If B removes the rainbow while A does not, the dominant chroma carrier is the DSR SpecMap/COLOR0 material input content.\n- If both reduce it, both carriers contribute; if neither does, the splice/downstream composition is wrong rather than either input chroma alone.\n\nDiagnostic only; not final mod.\n'''),encoding='utf-8')
    with zipfile.ZipFile(zpath,'w',zipfile.ZIP_DEFLATED) as z:
        for pth in (addon,audit,readme,Path(__file__)): z.write(pth,pth.name)
    with zipfile.ZipFile(zpath) as z: assert z.testzip() is None
    return {'tag':tag,'kind':kind,'addon':str(addon),'addon_sha':sha,'zip':str(zpath),'zip_sha':hashlib.sha256(zpath.read_bytes()).hexdigest(),'diffs':len(diffs)}

print(json.dumps([build('CUBE_ACHROMA'),build('MATERIAL_ACHROMA')],indent=2))
