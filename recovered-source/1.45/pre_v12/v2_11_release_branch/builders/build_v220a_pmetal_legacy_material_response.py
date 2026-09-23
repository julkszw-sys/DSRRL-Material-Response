import struct, hashlib, json, zipfile, textwrap, importlib.util
from pathlib import Path
ROOT=Path('/mnt/data')
BASE=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_19A_CE18_DIFFUSE_GATE.addon64'
OUT=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_20A_PMETAL_LEGACY_SPEC_RESPONSE.addon64'
AUD=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_20A_PMETAL_LEGACY_SPEC_RESPONSE_AUDIT.json'
README=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_20A_PMETAL_LEGACY_SPEC_RESPONSE_README.txt'
ZIP=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_20A_PMETAL_LEGACY_SPEC_RESPONSE_RUNTIME_TEST.zip'
BND=ROOT/'pmetal_work/flver.bnd'
CK=ROOT/'pmetal_work/dxbc_checksum.py'
MD=ROOT/'pmetal_work/dxbc_minidis.py'
base=BASE.read_bytes(); b=bytearray(base)
BASE_SHA=hashlib.sha256(base).hexdigest()
assert BASE_SHA=='3bfb47c3b0ffe1ed48db856fe1a0228610e7e07f2f78b4351065faa99233702c'
# load helpers
spec=importlib.util.spec_from_file_location('ck',CK); ck=importlib.util.module_from_spec(spec); spec.loader.exec_module(ck)
spec=importlib.util.spec_from_file_location('md',MD); md=importlib.util.module_from_spec(spec); spec.loader.exec_module(md)

# ---------- DXBC helpers ----------
def chunks(data):
    assert data[:4]==b'DXBC'
    total=struct.unpack_from('<I',data,24)[0]; assert total==len(data)
    n=struct.unpack_from('<I',data,28)[0]; offs=struct.unpack_from('<'+'I'*n,data,32)
    out={}
    for o in offs:
        tag=data[o:o+4].decode(); sz=struct.unpack_from('<I',data,o+4)[0]; assert o+8+sz<=len(data)
        out[tag]=(o,o+8,sz,data[o+8:o+8+sz])
    return out

def sigkey(data):
    c=chunks(data); x=b''
    for t in ('ISGN','ISG1','OSGN','OSG1','STAT'):
        if t in c: x+=t.encode()+c[t][3]
    return hashlib.sha256(x).hexdigest()

def color_reg(data):
    c=chunks(data); key='ISGN' if 'ISGN' in c else 'ISG1'; blob=c[key][3]
    n=struct.unpack_from('<I',blob,0)[0]
    for i in range(n):
        e=8+i*24
        nameoff,semidx,sysval,ctype,reg,maskrw=struct.unpack_from('<6I',blob,e)
        end=blob.find(b'\0',nameoff); name=blob[nameoff:end].decode('ascii')
        if name=='COLOR' and semidx==0: return reg
    raise RuntimeError('COLOR0 not found')

def shex_words(data):
    c=chunks(data); key='SHEX' if 'SHEX' in c else 'SHDR'; o,p,sz,blob=c[key]
    words=list(struct.unpack('<'+'I'*(len(blob)//4),blob)); return key,o,p,sz,words

def replace_shex_words(data, words):
    c=chunks(data); key='SHEX' if 'SHEX' in c else 'SHDR'; o,p,sz,blob=c[key]
    assert len(words)*4==sz
    q=bytearray(data); q[p:p+sz]=struct.pack('<'+'I'*len(words),*words)
    q[4:20]=b'\0'*16
    q[4:20]=ck.dxbc_checksum(bytes(q))
    assert ck.dxbc_checksum(bytes(q))==bytes(q[4:20])
    return bytes(q)

def operand_slices(raw, nops):
    # return raw word slices for operands of ordinary arithmetic instruction; enough for c101 mul.
    words=raw; pos=1; out=[]
    def consume(pos):
        st=pos; tok=words[pos]; pos+=1
        if tok>>31:
            while True:
                e=words[pos]; pos+=1
                if not e>>31: break
        ncomp=tok&3; typ=(tok>>12)&0xff; dim=(tok>>20)&3
        if typ==4:
            pos += 1 if ncomp==1 else 4 if ncomp==2 else 0
            return st,pos
        for d in range(dim):
            rep=(tok>>(22+3*d))&7
            if rep==0: pos+=1
            elif rep==1: pos+=2
            elif rep==2: _,pos=consume(pos)
            elif rep==3: pos+=1; _,pos=consume(pos)
            elif rep==4: pos+=2; _,pos=consume(pos)
            else: raise RuntimeError('unsupported operand index rep')
        return st,pos
    for _ in range(nops):
        st,pos=consume(pos); out.append(words[st:pos])
    assert pos==len(words)
    return out

# ---------- vanilla shader name map ----------
vb=BND.read_bytes(); assert vb[:4]==b'BND3'; n=struct.unpack_from('<I',vb,0x10)[0]
vanilla={}
for i in range(n):
    o=0x28+i*24; do,fid,no,sz,m,tr=struct.unpack_from('<6I',vb,o)
    end=vb.find(b'\0',no); name=vb[no:end].decode('shift_jis','replace').replace('\\','/').split('/')[-1]
    d=vb[do:do+sz]
    try:k=sigkey(d)
    except:continue
    vanilla.setdefault(k,[]).append((i,name,sz))

# ---------- scan embedded 48 shaders ----------
occ=[]; pos=0
while True:
    pos=base.find(b'DXBC',pos)
    if pos<0: break
    if pos+28<=len(base):
        total=struct.unpack_from('<I',base,pos+24)[0]
        if 1000<total<100000 and pos+total<=len(base):
            d=base[pos:pos+total]
            try:
                dis=md.disasm(d); k=sigkey(d)
                ops=' '.join(','.join(x['ops']) for x in dis)
                occ.append({'off':pos,'size':total,'data':d,'dis':dis,'key':k,'c120':'cb12[0]' in ops,'c121':'cb12[1]' in ops})
            except: pass
    pos+=4
assert len(occ)==48, len(occ)
diff_by_key={x['key']:x for x in occ if x['c121'] and not x['c120']}
c101_by_key={x['key']:x for x in occ if x['c121'] and x['c120']}
assert len(diff_by_key)==24 and len(c101_by_key)==24 and diff_by_key.keys()==c101_by_key.keys()
assert all(diff_by_key[k]['size']==c101_by_key[k]['size'] for k in diff_by_key)

# Map unique target host signatures 894/913/932.
target_indices={894,913,932}; target_keys={}
for k,cands in vanilla.items():
    idxs={x[0] for x in cands}
    hit=idxs & target_indices
    if hit:
        assert len(hit)==1
        target_keys[next(iter(hit))]=k
assert set(target_keys)==target_indices, target_keys

# ---------- PMR material-response patch ----------
# V2.20A is intentionally NOT an EnvSpec-shape/cubemap patch.
# Keep DSR t12 dynamic LOD and angular/horizon factor.
# Replace only t9 BRDF split-sum material factor with PTDE-linear material response:
# M_P = SpecMap_linear.rgb * c101_PTDE.rgb * COLOR0.rgb
# E_test = E_DSR_preBRDF * M_P
NOP=(1<<24)|md.OP_NAMES.index('nop')
patched_hosts=[]
for k,cslot in c101_by_key.items():
    dslot=diff_by_key[k]
    # Default alternate = exact diffuse/V2.9.1 payload: prevents legacy V2.11 q->F0 path for any unexpected host.
    alt=dslot['data']
    mapped=vanilla.get(k,[])
    target_hit=next((idx for idx in target_indices if any(c[0]==idx for c in mapped)),None)
    if target_hit is not None:
        diff_dis=md.disasm(alt); orig_c101=cslot['data']; c101_dis=md.disasm(orig_c101)
        # get SpecMap-linear source from original c101 constructor: mul ..., specRGB, cb12[0]
        cm=[x for x in c101_dis if x['name']=='mul' and any('cb12[0]' in o for o in x['ops'])]
        assert len(cm)==1
        cops=operand_slices(cm[0]['raw'],3)
        assert any('cb12[0]' in o for o in cm[0]['ops'])
        spec_src=cops[1]
        assert len(spec_src)==2 and ((spec_src[0]>>12)&0xff)==0  # temp register
        # find the unique t9 BRDF LUT sample and canonical 4-instruction tail
        t9=[j for j,x in enumerate(diff_dis) if x['name']=='sample_l' and any('t9' in o for o in x['ops'])]
        assert len(t9)==1; j=t9[0]
        seq=diff_dis[j:j+4]
        assert [x['name'] for x in seq]==['sample_l','mul','mad','mul']
        assert [x['len'] for x in seq]==[13,7,9,7]
        assert seq[-1]['ops'][0].startswith('r') and seq[-1]['ops'][1].startswith('r')
        # LUT destination register index from sample_l dest raw operand.
        import re
        mreg=re.match(r'r(\d+)',seq[0]['ops'][0]); assert mreg
        lut_reg=int(mreg.group(1))
        col=color_reg(alt)
        # Build 29 words replacing sample_l+mul+mad, then preserve existing final EnvSpec multiply (7 words).
        # mul rL.xyz, <linear SpecMap source>, cb12[0].xyzx
        ins1=[(8<<24)|0x38,0x100072,lut_reg] + spec_src + [0x208246,12,0]
        assert len(ins1)==8
        # mul rL.xyz, rL.xyzx, vCOLOR.xyzx
        ins2=[(7<<24)|0x38,0x100072,lut_reg,0x100246,lut_reg,0x101246,col]
        assert len(ins2)==7
        repl=ins1+ins2+[NOP]*14
        assert len(repl)==29
        keyc,coff,cpay,csz,words=shex_words(alt)
        st=seq[0]['word']; final=seq[-1]
        assert final['word']==st+29
        words[st:st+29]=repl
        alt=replace_shex_words(alt,words)
        # semantic audit
        dd=md.disasm(alt)
        assert not any(x['name']=='sample_l' and any('t9' in o for o in x['ops']) for x in dd)
        custom=[x for x in dd if x['name']=='mul' and any('cb12[0]' in o for o in x['ops'])]
        assert len(custom)==1
        assert any(f'v{col}' in o for x in dd[j:j+4] for o in x['ops']) or any(f'v{col}' in o for x in dd for o in x['ops'])
        # Outside the 29-word replacement and checksum, target derives byte-for-byte from diffuse variant.
        _,_,_,_,w0=shex_words(dslot['data']); _,_,_,_,w1=shex_words(alt)
        difw=[ii for ii,(a,z) in enumerate(zip(w0,w1)) if a!=z]
        assert all(st<=ii<st+29 for ii in difw)
        mapped_name=next(c[1] for c in mapped if c[0]==target_hit)
        patched_hosts.append({'shader_index':target_hit,'shader_name':mapped_name,'alternate_slot_offset':hex(cslot['off']),'diffuse_source_offset':hex(dslot['off']),'size':len(alt),'color0_register':col,'spec_source':cm[0]['ops'][1],'replacement_word_start':st,'replacement_word_count':29,'dxbc_sha256':hashlib.sha256(alt).hexdigest(),'checksum':alt[4:20].hex()})
    assert len(alt)==cslot['size']
    b[cslot['off']:cslot['off']+cslot['size']]=alt

assert {x['shader_index'] for x in patched_hosts}==target_indices

# ---------- registry: exact P_Metal only selects alternate; carrier cb12[0] repurposed to raw PTDE c101=2.5 ----------
REG_OFF=0x81280; REC=0x48; N=368; IMAGE=0x180000000; RDATA_RAW=0xd400; RDATA_RVA=0xe000

def va_to_raw(va): return RDATA_RAW+((va-IMAGE)-RDATA_RVA)
PMETAL='ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b'
pm=None; active_real=[]
# active CE18 real keys in V2.19A are the non-sentinel hashes that appear in gate V3.
gate=json.load(open(ROOT/'DSRRL_CHARACTER_EQUIPMENT_MATERIAL_GATE_V3_2026-09-15.json',encoding='utf-8'))
ce18={x['effective_dsr_mtd_sha256'] for x in gate['active_ce18']}; assert PMETAL in ce18 and len(ce18)==18
for i in range(N):
    off=REG_OFF+i*REC; ptr,n=struct.unpack_from('<QQ',b,off); assert n==64; ho=va_to_raw(ptr); h=bytes(b[ho:ho+64]).decode('ascii')
    if h in ce18:
        active_real.append(h)
        if h==PMETAL:
            pm=(i,off)
            b[off+0x44]=1
            # Existing sidecar upload reads q carrier (+0x2c..+0x34) into cb12[0].
            # Alternate shaders no longer use q as pre-F0 coordinate; set carrier to raw PTDE c101=2.5.
            struct.pack_into('<3f',b,off+0x2c,2.5,2.5,2.5)
        else:
            assert b[off+0x44]==0
assert len(active_real)==18 and pm is not None
pi,po=pm
assert struct.unpack_from('<3f',b,po+0x20)==(2.5,2.5,2.5)
assert struct.unpack_from('<3f',b,po+0x2c)==(2.5,2.5,2.5)
assert b[po+0x44]==1

# ---------- patch misleading active banner only, keeping title/telemetry stable for binary compatibility ----------
old=(b'PTDE Material Response V2.11 active: PTDE c100 + DIFFUSE linear baseline plus analytic PTDE c101 gain with the native pre-F0 SAT projection removed on 24 stable Phn Spc HemEnv hosts; stock DSR x^2.2 and downstream PBL/EnvSpec retained; HemEnvLerp stock DSR.\0')
new=(b'PTDE Material Response V2.20A active: CE18 V2.9.1 diffuse gate; exact P_Metal stable HemEnv uses PTDE-linear SpecMap*c101*COLOR0 material factor in native DSR EnvSpec sampling/angular path; t9 BRDF split-sum bypassed only there; HemEnvLerp stock DSR.\0')
pos=bytes(b).find(old)
assert pos>=0
assert len(new)<=len(old)
b[pos:pos+len(old)]=new+b'\0'*(len(old)-len(new))

# ---------- PE/diff audit ----------
def sections(blob):
    e=struct.unpack_from('<I',blob,0x3c)[0]; nsec=struct.unpack_from('<H',blob,e+6)[0]; osz=struct.unpack_from('<H',blob,e+20)[0]; st=e+24+osz; out=[]
    for i in range(nsec):
        o=st+i*40; name=blob[o:o+8].rstrip(b'\0').decode(); vs,va,rs,raw=struct.unpack_from('<IIII',blob,o+8); out.append((name,raw,rs,va,vs))
    return out
secs=sections(base); diffs=[i for i,(x,y) in enumerate(zip(base,b)) if x!=y]
bysec={name:sum(raw<=d<raw+rs for d in diffs) for name,raw,rs,va,vs in secs}
assert len(base)==len(b); assert bysec.get('.text',0)==0 and bysec.get('.data',0)==0 and bysec.get('.pdata',0)==0 and bysec.get('.reloc',0)==0
assert bysec.get('.rdata',0)==len(diffs)

# Reparse all 48 embedded shader occurrences in output; checksum-valid.
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
assert len(valid)==48, len(valid)
OUT.write_bytes(outb)
out_sha=hashlib.sha256(outb).hexdigest()
zip_sha=None

audit={
 'schema':'DSRRL_V2_20A_PMETAL_LEGACY_SPEC_RESPONSE_AUDIT_V1','date':'2026-09-15',
 'purpose':'Character/equipment material-response continuation: keep validated CE18 V2.9.1 diffuse gate, add exact-base-P_Metal stable-HemEnv PTDE-linear specular material factor while retaining native DSR EnvSpec resource, dynamic LOD, source amplitude and angular term.',
 'basis':{'addon':BASE.name,'sha256':BASE_SHA,'runtime_validation':'V2.19A CE18 routing confirmed by owner log artifact585 / canonical rev6139; visual pixel acceptance remains separate.'},
 'output':{'addon':OUT.name,'sha256':out_sha,'size':len(outb)},
 'registry':{'active_ce18':18,'pmetal_sha256':PMETAL,'pmetal_record_index':pi,'pmetal_has_spec':1,'other_ce17_has_spec':0,'pmetal_raw_c101':[2.5,2.5,2.5],'pmetal_cb12_carrier':[2.5,2.5,2.5]},
 'alternate_array':{'slots':24,'default':'byte-exact copy of V2.9.1 diffuse alternate; prevents V2.11 q->F0 material bridge on unexpected stable HemEnv host','pmr_patched_hosts':patched_hosts},
 'operator':{
   'preserved':['V2.9.1 diffuse material-domain identity','stock DSR Spec/F0 construction outside broad EnvSpec','native DSR t12 EnvSpec resource','stock DSR dynamic roughness/LOD sample','stock DSR EnvSpec profile/source cb4*cb79.x','stock DSR angular/horizon factor','EnvDiffuse merge and downstream lighting/fog/HDR/postprocess','HemEnvLerp stock DSR','PointLight unchanged'],
   'replaced_on_exact_pmetal_hosts_894_913_932':['t9 BRDF LUT sample','split-sum F0*LUT.x + k*LUT.y material tail'],
   'replacement':'M_legacy = SpecMap_linear.rgb * c101_PTDE.rgb * COLOR0.rgb; E_test = E_DSR_preBRDF * M_legacy',
   'not_claimed':['full PTDE EnvSpec resource equivalence','PTDE cubemap/slot shape equivalence','final-pixel 1:1','PointLight/local-spec equivalence']
 },
 'construction':{'embedded_dxbc_count':48,'all_embedded_dxbc_checksums_valid':True,'text_unchanged':True,'diffs_by_section':bysec,'only_rdata':True,'MTD_changes':0,'PARAM_changes':0,'EXE_changes':0},
 'final_mod_note':'Diagnostic Renderer Edition add-on only; production DSRRL remains PARAM-only.'
}
AUD.write_text(json.dumps(audit,indent=2,ensure_ascii=False),encoding='utf-8')
README.write_text(textwrap.dedent(f'''\
DSRRL V2.20A — EXACT P_METAL LEGACY SPECULAR MATERIAL RESPONSE
===============================================================

What this tests
---------------
V2.20A keeps the runtime-validated V2.19A CE18 character/equipment diffuse gate.
Only exact base P_Metal[DSB].mtd ({PMETAL[:12]}...) can select the new stable-HemEnv alternate receiver.

This is deliberately a MATERIAL-RESPONSE test, not an EnvSpec-shape/cubemap test.
On P_Metal stable HemEnv hosts 894 / 913 / 932 it keeps:
- native DSR EnvSpec cubemap/resource,
- native DSR roughness-driven SAMPLE_L LOD,
- native DSR EnvSpec LightBank/profile amplitude,
- native DSR angular/horizon factor,
- V2.9.1 diffuse material response,
- stock DSR HemEnvLerp transition path,
- stock PointLight path.

It removes only the DSR t9 BRDF-LUT split-sum material tail for broad EnvSpec and replaces it with:

    M = SpecMap_linear.rgb * c101_PTDE.rgb * COLOR0.rgb
    E = E_DSR_preBRDF * M

For exact P_Metal, c101_PTDE = 2.5. The existing b12 carrier is intentionally repurposed from the V2.11 rooted q coordinate to raw PTDE c101 because the alternate shader no longer feeds that carrier into stock F0.

Safety
------
- CE18 world exclusion remains unchanged.
- 17 non-P_Metal CE materials remain on V2.9.1 diffuse-only path.
- All 21 non-target stable-HemEnv alternate shader slots are replaced with their safe diffuse/V2.9.1 equivalents, so an unexpected host cannot fall back to the rejected V2.11 c101->F0 experiment.
- HemEnvLerp stays stock DSR.
- No MTD, PARAM, EXE, PointLight or game-asset edits.

This does NOT claim final PTDE 1:1. It isolates whether the missing armor character is primarily the material BRDF/split-sum response while allowing DSR EnvSpec sampling/shape to remain native.

Base SHA-256: {BASE_SHA}
Output addon SHA-256: {out_sha}
'''),encoding='utf-8')
with zipfile.ZipFile(ZIP,'w',zipfile.ZIP_DEFLATED) as z:
    for p in (OUT,AUD,README,Path(__file__)):
        z.write(p,p.name)
with zipfile.ZipFile(ZIP) as z: assert z.testzip() is None
zip_sha=hashlib.sha256(ZIP.read_bytes()).hexdigest()
print('addon',out_sha,OUT.stat().st_size)
print('zip',zip_sha,ZIP.stat().st_size)
print('diffs',len(diffs),bysec)
print('patched hosts',[(x['shader_index'],x['alternate_slot_offset'],x['dxbc_sha256']) for x in patched_hosts])
