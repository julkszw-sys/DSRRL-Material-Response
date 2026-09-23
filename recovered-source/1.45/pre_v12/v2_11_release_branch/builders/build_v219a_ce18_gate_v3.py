import json, struct, hashlib, zipfile, textwrap
from pathlib import Path
ROOT=Path('/mnt/data')
BASE=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_11.addon64'
GATE=ROOT/'DSRRL_CHARACTER_EQUIPMENT_MATERIAL_GATE_V3_2026-09-15.json'
OUT=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_19A_CE18_DIFFUSE_GATE.addon64'
AUD=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_19A_CE18_DIFFUSE_GATE_AUDIT.json'
README=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_19A_CE18_DIFFUSE_GATE_README.txt'
ZIP=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_19A_CE18_DIFFUSE_GATE_RUNTIME_TEST.zip'

orig=BASE.read_bytes(); b=bytearray(orig)
base_sha=hashlib.sha256(orig).hexdigest()
assert base_sha=='1cfa2154059575d2dbf3f34124e7d51c4c63e8ab8f22347ae56755bc1e5f697a'
gate=json.load(open(GATE,encoding='utf-8'))
active_gate=gate['active_ce18']
assert len(active_gate)==18
allow_hash={e['effective_dsr_mtd_sha256'] for e in active_gate}
assert len(allow_hash)==18
assert all(e['owner_class']=='CHARACTER_EQUIPMENT_ONLY' for e in active_gate)
assert all(e['homology_class']=='HOMOLOGOUS_SPC' for e in active_gate)
assert all(e['dsr_spx']==e['ptde_spx'] for e in active_gate)
assert all(e['obj_asset_count']==0 and e['map_asset_count']==0 for e in active_gate)

# V2.11 donor registry: 368 records x 0x48 at file 0x81280; raw hash string is pointer-referenced.
REG_OFF=0x81280; REC=0x48; N=368
IMAGE=0x180000000; RDATA_RAW=0xd400; RDATA_RVA=0xe000

def va_to_raw(va): return RDATA_RAW + ((va-IMAGE)-RDATA_RVA)
records=[]
for i in range(N):
    off=REG_OFF+i*REC
    ptr,n=struct.unpack_from('<QQ',orig,off); assert n==64
    ho=va_to_raw(ptr)
    hs=orig[ho:ho+64].decode('ascii')
    rec={
      'index':i,'off':off,'hash_off':ho,'sha256':hs,
      'tier':orig[off+0x1c],'has_spec':orig[off+0x44],
      'c100':struct.unpack_from('<3f',orig,off+0x10),
      'raw_c101':struct.unpack_from('<3f',orig,off+0x20),
      'q':struct.unpack_from('<3f',orig,off+0x2c),
      'slot':struct.unpack_from('<I',orig,off+0x38)[0],
      'c102':struct.unpack_from('<f',orig,off+0x3c)[0],
    }
    records.append(rec)
byhash={r['sha256']:r for r in records}
assert len(byhash)==N
assert allow_hash <= byhash.keys()
# Strong rollout property: every CE18 target is an exact Tier0 paired donor, no sibling/signature inference.
assert all(byhash[h]['tier']==0 for h in allow_hash)
assert all(byhash[h]['has_spec']==1 for h in allow_hash)

# Materialize gate without changing executable code or shader payloads.
# CE18 records remain valid but has_spec=0 => existing V2.11 selector uses diffuse/V2.9.1 shader array.
# Every non-target key is replaced by a deterministic VALID hexadecimal SHA-256 sentinel.
# This avoids relying on the registry accepting non-hex strings at addon initialization.
# Sentinel = SHA256('DSRRL_DISABLED_V219A:' + original_sha), verified unique and absent from the full known DSR MTD corpus.
known_dsr_hashes=set()
# The homology registry enumerates the full DSR Mtd binder; use it only as a collision set.
hom=json.load(open(ROOT/'DSRRL_MATERIAL_HOMOLOGY_REGISTRY_V1_2026-09-15.json',encoding='utf-8'))
known_dsr_hashes={x['dsr_mtd_sha256'] for x in hom['materials']}
sentinels={}
for r in records:
    if r['sha256'] in allow_hash:
        b[r['off']+0x44]=0
    else:
        sh=hashlib.sha256(('DSRRL_DISABLED_V219A:'+r['sha256']).encode('ascii')).hexdigest()
        assert sh not in known_dsr_hashes and sh not in allow_hash
        assert sh not in sentinels.values()
        b[r['hash_off']:r['hash_off']+64]=sh.encode('ascii')
        sentinels[r['sha256']]=sh

# PE section diff containment.
def sections(blob):
    e=struct.unpack_from('<I',blob,0x3c)[0]
    nsec=struct.unpack_from('<H',blob,e+6)[0]
    optsz=struct.unpack_from('<H',blob,e+20)[0]
    st=e+24+optsz; out=[]
    for i in range(nsec):
        o=st+i*40
        name=blob[o:o+8].rstrip(b'\0').decode()
        vs,va,rs,raw=struct.unpack_from('<IIII',blob,o+8)
        out.append((name,raw,rs,va,vs))
    return out
secs=sections(orig)
diffs=[i for i,(a,c) in enumerate(zip(orig,b)) if a!=c]
bysec={name:sum(raw<=d<raw+rs for d in diffs) for name,raw,rs,va,vs in secs}
assert len(orig)==len(b)
assert bysec.get('.text',0)==0 and bysec.get('.data',0)==0 and bysec.get('.pdata',0)==0 and bysec.get('.reloc',0)==0
assert bysec.get('.rdata',0)==len(diffs)

# Reparse output and assert exactly 18 reachable real SHA keys, all diffuse-only.
out_records=[]
for i in range(N):
    off=REG_OFF+i*REC
    ptr,n=struct.unpack_from('<QQ',b,off); ho=va_to_raw(ptr)
    hs=bytes(b[ho:ho+64]).decode('ascii')
    out_records.append((hs,b[off+0x44]))
assert len({h for h,f in out_records})==N
assert all(len(h)==64 and all(c in '0123456789abcdef' for c in h) for h,f in out_records)
reachable=[(h,f) for h,f in out_records if h in allow_hash]
assert len(reachable)==18
assert {h for h,f in reachable}==allow_hash
assert all(f==0 for h,f in reachable)
# Every non-target output key is one of the deterministic sentinels and cannot match any known DSR MTD payload.
out_non_target={h for h,f in out_records if h not in allow_hash}
assert out_non_target==set(sentinels.values())
assert not (out_non_target & known_dsr_hashes)

# .text selector bytes are inherited exactly. Disassembly already established:
# donor +0x44 -> testb -> cmove diffuse shader array 0x180107888 when zero, c101 array 0x180107948 otherwise.
allow_rows=[]
for e in sorted(active_gate,key=lambda x:x['mtd_name'].lower()):
    r=byhash[e['effective_dsr_mtd_sha256']]
    allow_rows.append({
      'mtd_name':e['mtd_name'],'sha256':r['sha256'],'record_index':r['index'],'record_file_offset':hex(r['off']),
      'tier':r['tier'],'original_has_spec':r['has_spec'],'patched_has_spec':0,
      'c100':list(r['c100']),'raw_c101':list(r['raw_c101']),'q_v211':list(r['q']),'envspc_slot':r['slot'],'c102':r['c102'],
      'parts_asset_count':e['parts_asset_count'],'chr_asset_count':e['chr_asset_count'],
      'obj_asset_count':e['obj_asset_count'],'map_asset_count':e['map_asset_count'],
      'duplicate_name_hash_correction':e['duplicate_name_hash_correction']
    })

OUT.write_bytes(b)
out_sha=hashlib.sha256(b).hexdigest(); gate_sha=hashlib.sha256(GATE.read_bytes()).hexdigest()
audit={
 'schema':'DSRRL_V2_19A_CE18_DIFFUSE_GATE_AUDIT_V3','date':'2026-09-15',
 'purpose':'First runtime rollout: accepted PTDE-like diffuse material response only for CE-exclusive exact-SPX homologous spec materials; all other materials fail open to stock DSR.',
 'basis':{'addon':BASE.name,'sha256':base_sha,'gate':GATE.name,'gate_sha256':gate_sha,'accepted_operator':'V2.9.1 c100 + diffuse material-domain identity carried by V2.11 diffuse shader array'},
 'output':{'addon':OUT.name,'sha256':out_sha,'size':len(b)},
 'registry':{'file_offset':hex(REG_OFF),'record_size':REC,'record_count':N,'active_real_sha_keys':18,'disabled_non_target_sha_keys':350,'target_has_spec_1_to_0':18,'all_active_tier0_exact':True},
 'selector':{'text_unchanged':True,'rva':'~0x63ba..0x6418','semantics':'donor +0x44 has_spec == 0 selects diffuse shader array 0x180107888; nonzero selects c101 array 0x180107948'},
 'diffs':{'total':len(diffs),'by_section':bysec,'only_rdata':True},
 'allowlist':allow_rows,
 'excluded':{
   'shared_character_world':[x['mtd_name'] for x in gate['deferred_shared_character_world']],
   'family_variant':[x['mtd_name'] for x in gate['quarantine_family_variant']]
 },
 'safety':{'c101_active_for_allowlist':False,'world_shared_materials':'excluded','non_target':'350 registry keys replaced by valid-hex deterministic sentinel SHA values absent from the full known DSR MTD hash corpus -> selector fail-open','HemEnvLerp':'stock DSR','PointLight':'unchanged','MTD_assets':'unchanged','PARAM':'unchanged','EXE':'unchanged','final_mod':'PARAM-only; addon diagnostic only'}
}
AUD.write_text(json.dumps(audit,indent=2,ensure_ascii=False),encoding='utf-8')
README.write_text(textwrap.dedent(f'''\
DSRRL V2.19A — CE18 DIFFUSE MATERIAL RESPONSE GATE
===================================================

Target
------
Validate the accepted V2.9.1 PTDE-like diffuse material response on character/equipment materials while ordinary world/object materials remain stock DSR.
This is not an elevator test and it does not attempt c101/specular translation.

Active cohort
-------------
Exactly 18 raw DSR MTD identities. Every active material is:
- observed only in retained parts/chr assets, with zero obj/map use,
- HOMOLOGOUS_SPC PTDE<->DSR,
- exact same declared SPX family across PTDE and DSR,
- exact Tier0 PTDE donor in the V2.11 registry.

P_DullLeather duplicate-name correction
----------------------------------------
DSR contains two P_DullLeather[DSB].mtd entries. V2.19A uses the effective first-entry/runtime donor SHA 7ea7d6... rather than the later shadowed 9376d8... entry.

Runtime behavior
----------------
For the 18 active hashes, donor has_spec is forced 1 -> 0. V2.11's unchanged selector therefore chooses its separate diffuse/V2.9.1 shader array.
For all other 350 donor records, the embedded key is replaced by a valid hexadecimal deterministic sentinel SHA-256 that is verified absent from the complete known DSR MTD hash corpus, so they fail open without relying on non-hex parser behavior.

Preserved stock DSR behavior
----------------------------
- c101 bridge disabled for active cohort
- stock DSR SPEC/PBL/EnvSpec/cubemap/roughness/LOD
- HemEnvLerp stock
- PointLight unchanged
- no MTD, PARAM, EXE or game-asset edits

Base V2.11 SHA-256: {base_sha}
Gate V3 SHA-256: {gate_sha}
Output addon SHA-256: {out_sha}

Diagnostic/research addon only. Final DSRRL remains PARAM-only.
'''),encoding='utf-8')
with zipfile.ZipFile(ZIP,'w',zipfile.ZIP_DEFLATED) as z:
    for p in (OUT,AUD,README,Path(__file__),GATE): z.write(p,p.name)
with zipfile.ZipFile(ZIP) as z: assert z.testzip() is None
print('addon',out_sha,OUT.stat().st_size)
print('zip',hashlib.sha256(ZIP.read_bytes()).hexdigest(),ZIP.stat().st_size)
print('gate',gate_sha)
print('diff',len(diffs),bysec)
print('active tier0',len(allow_rows),all(r['tier']==0 for r in allow_rows))
