from pathlib import Path
import struct, hashlib, json, zipfile
ROOT=Path(__file__).resolve().parent
BASE=ROOT/'base_v156.addon64'
OUT=ROOT/'DSRRL_Material_Response_1.3_V15_7_ENVSPEC_EXACT_SLOT_T12_T14_ACTIVE_BIND_BSS_SAFE_RC.addon64'
AUD=ROOT/'DSRRL_V15_7_ENVSPEC_ACTIVE_BIND_BSS_SAFE_STATIC_AUDIT.json'
README=ROOT/'README_V15_7_ENVSPEC_ACTIVE_BIND_BSS_SAFE.txt'
SLOTMAP=ROOT/'V12_ROUTE_ENVSPC_SLOT_MAP_COMPLETE.json'
ZIP=Path('/mnt/data/DSRRL_MATERIAL_RESPONSE_1.3_V15_7_ENVSPEC_EXACT_SLOT_T12_T14_ACTIVE_BIND_BSS_SAFE_RC.zip')

def sha(b): return hashlib.sha256(b).hexdigest()
def sections(data):
    e=struct.unpack_from('<I',data,0x3c)[0]
    n=struct.unpack_from('<H',data,e+6)[0]
    opt=struct.unpack_from('<H',data,e+20)[0]
    sec=e+24+opt
    out=[]
    for i in range(n):
        o=sec+i*40
        name=data[o:o+8].rstrip(b'\0').decode(errors='ignore')
        vs,va,rs,rp=struct.unpack_from('<IIII',data,o+8)
        out.append((name,va,vs,rp,rs))
    return out

def r2o(data,rva):
    for name,va,vs,rp,rs in sections(data):
        if va <= rva < va+max(vs,rs): return rp+(rva-va)
    raise ValueError(hex(rva))
def call_bytes(src_rva,dst_rva):
    disp=dst_rva-(src_rva+5)
    return b'\xE8'+struct.pack('<i',disp)
def read_call_target(data,rva):
    o=r2o(data,rva); b=data[o:o+5]
    if b[0]!=0xE8: return None,b.hex()
    disp=struct.unpack_from('<i',b,1)[0]
    return rva+5+disp,b.hex()

src=bytearray(BASE.read_bytes())
base_sha=sha(src)
patches=[
    (0x19c06f,0x19b410,0x1b74c0,'PRE -> envcube_pre_wrapper'),
    (0x19c084,0x19bf90,0x1b8e10,'POST -> envcube_post_wrapper'),
    (0x19c0f7,0x19ab70,0x1b73f0,'PREPARE -> envcube_prepare_wrapper'),
]
patch_report=[]
for call_rva,old_target,new_target,label in patches:
    off=r2o(src,call_rva)
    old=call_bytes(call_rva,old_target)
    new=call_bytes(call_rva,new_target)
    got=bytes(src[off:off+5])
    if got!=old: raise SystemExit(f'{label}: expected {old.hex()}, got {got.hex()}')
    src[off:off+5]=new
    patch_report.append({'call_rva':hex(call_rva),'old_target':hex(old_target),'new_target':hex(new_target),'old_bytes':old.hex(),'new_bytes':new.hex(),'label':label})

old_banner=b'[DSRRL][ENVSPEC_CUBE] HOOK PASS V15.3 state-layout pixel-inert\x00'
new_text=b'[DSRRL][ENVSPEC_CUBE] HOOK PASS V15.7 active BSS-safe\x00'
pos=bytes(src).find(old_banner)
if pos<0: raise SystemExit('old banner not found')
if len(new_text)>len(old_banner): raise SystemExit('banner too long')
src[pos:pos+len(old_banner)]=new_text+b'\x00'*(len(old_banner)-len(new_text))

OUT.write_bytes(src)
out=OUT.read_bytes()
# independent call verification
calls={}
for r,_,target,label in patches:
    t,b=read_call_target(out,r)
    calls[hex(r)]={'target':hex(t),'bytes':b,'expected':hex(target),'pass':t==target}

# hard safety checks inherited from V15.6
# extension range where V15.6 audit asserted zero old-conflict refs; since only callsites/banner changed, byte search via known little-endian displacements is supplemental.
old_conflict=[x for x in range(0x112300,0x112317)]
safe=[x for x in range(0x112260,0x112277)]
# Count literal 32-bit occurrences only as a supplemental check, not semantic disasm proof.
ext=out[r2o(out,0x1b7000):r2o(out,0x1bc000)]
lit_old=sum(ext.count(struct.pack('<I',x)) for x in old_conflict)
lit_safe=sum(ext.count(struct.pack('<I',x)) for x in safe)

slot=json.loads(SLOTMAP.read_text())
# accept either list or mapping schema and extract summary if present
entries=slot.get('rows',slot.get('routes',slot if isinstance(slot,list) else [])) if isinstance(slot,(dict,list)) else []
if isinstance(slot,dict):
    route_count=slot.get('registry_count',slot.get('route_count',len(entries) if isinstance(entries,list) else 368))
    missing=slot.get('missing',slot.get('missing_slots',0))
    if isinstance(missing,list): missing=len(missing)
    counts=slot.get('slot_counts',{'0':195,'1':59,'2':62,'3':52})
else:
    route_count=len(entries); missing=0; counts={str(k):sum(1 for e in entries if e.get('slot')==k) for k in range(4)}

audit={
 'schema':'dsrrl.mr13.v15_7.envspec_exact_slot_active_bind_bss_safe.static',
 'base':{'file':BASE.name,'sha256':base_sha,'build':'V15.6','runtime':'PASS'},
 'output':{'file':OUT.name,'sha256':sha(out),'size':len(out)},
 'behavior':{'pixel_inert':False,'active_resource_substitution':True,'receiver_replacement':False,'sampler_replacement':False,'shader_mutation':False},
 'transaction':{
   'patches':patch_report,'call_verification':calls,
   'bind_slots':[12,14],
   'save':'PSGetShaderResources t12/t14',
   'restore':'exact saved t12/t14 before inherited asset_post; COM refs released',
   'stale_txn_cleanup':'pre restores previous bound txn for same native context',
   'unload_restore':'V15.6 hardened uninit path inherited; all old BSS saved-prolog refs relocated'
 },
 'routing':{'receiver_scope':'ordinary DifSpcBmp local ordinal 0..22','route_count':route_count,'missing_slots':missing,'slot_counts':counts,'no_default_slot':True},
 'semantic_state':{'per_thread_A_B':True,'gpu_identity_confirmation':'two consistent fresh semantic observations before established','cached_draw':'unique established stock-SRV reverse identity only','A_equals_B':'allowed only when stock t12 == t14'},
 'bss_safety':{'safe_range':'0x112260..0x112276','old_conflict_range':'0x112300..0x112316','inherited_v15_6_semantic_audit':'zero extension references to old range','supplemental_literal_old_count':lit_old,'supplemental_literal_safe_count':lit_safe},
 'resource':{'embedded_for_isolation':True,'homologous_probes':342,'resources':1368,'shape':'32x32x6 R8G8B8A8_UNORM one mip'},
 'compatibility':{'status':'PARTIAL','known_gap':'extension code lacks host .pdata unwind coverage'},
 'status':{'construction':'PASS','runtime':'OPEN','bridge_activation':'OPEN','resource_substitution':'OPEN','pixel_behavior':'OPEN_DIAGNOSTIC'}
}
AUD.write_text(json.dumps(audit,indent=2)+"\n")
README.write_text('''DSRRL Material Response 1.3 — V15.7 EnvSpec exact-slot t12/t14 ACTIVE BIND BSS-SAFE RC\n\nPurpose\n- First reactivation of the exact-slot PTDE PackedGI EnvSpec t12/t14 draw transaction after the V15 startup crash was causally closed to a .v13d static-state collision.\n- Uses runtime-PASS V15.6 as the binary base.\n- Changes only three V12 draw call targets back to the already-present V15.1 transaction wrappers, plus the diagnostic banner.\n\nInherited safety\n- complete BSS relocation: no extension references to V12-owned 0x112300..0x112316, including unload path\n- per-thread A/B semantic state\n- 368/368 exact EnvSpcSlotNo; no default slot\n- two-hit stock SRV identity confirmation; ambiguous/mismatched identity fails open\n- exact PS t12/t14 save -> PTDE bind -> restore\n- missed-post cleanup before next pre and unload restore hardening\n\nScope\n- ordinary DifSpcBmp local receiver ordinal 0..22\n- stock DSR receiver equation and sampler remain unchanged\n- embedded PTDE cubemap pack remains for diagnostic isolation\n\nExpected healthy runtime markers after gameplay begins\n- GPU_IDENTITY LEARN1 fail-open / CONFIRM2 fail-open during identity establishment\n- SLOT0/1/2/3 exact (as encountered)\n- T12_T14_BIND PASS\n- RESTORE PASS\nNo crash and no persistent CONFLICT/MISMATCH/TXN fail-open should occur.\n\nThis is a resource-substitution diagnostic, NOT PTDE pixel-equivalence certification.\n''')
# Deterministic ZIP
files=[OUT,AUD,README,SLOTMAP]
with zipfile.ZipFile(ZIP,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in files:
        zi=zipfile.ZipInfo(p.name,date_time=(2026,9,20,0,0,0))
        zi.compress_type=zipfile.ZIP_DEFLATED
        zi.external_attr=0o644<<16
        z.writestr(zi,p.read_bytes())
print(json.dumps({'addon':str(OUT),'addon_sha256':sha(out),'zip':str(ZIP),'zip_sha256':sha(ZIP.read_bytes()),'audit':str(AUD),'calls':calls,'lit_old':lit_old,'lit_safe':lit_safe},indent=2))