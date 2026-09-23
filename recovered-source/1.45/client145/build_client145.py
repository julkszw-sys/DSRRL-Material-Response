from pathlib import Path
import struct, hashlib, json, zipfile, shutil, re

ROOT=Path('/mnt/data/client145_work')
BASE=Path('/mnt/data/v157_active_bss_safe/DSRRL_Material_Response_1.3_V15_7_ENVSPEC_EXACT_SLOT_T12_T14_ACTIVE_BIND_BSS_SAFE_RC.addon64')
LOADER=ROOT/'loader_sha.bin'
SIDECAR_SRC=Path('/mnt/data/_sidecar_extract/DSRRL/EnvSpec/PackedGI')
OUT=ROOT/'DSRRL_Material_Response_1.45.addon64'
AUD=Path('/mnt/data/DSRRL_Material_Response_1.45_RELEASE_AUDIT.json')
PKG=Path('/mnt/data/DSRRL_Material_Response_1.45_CLIENT.zip')
STAGE=ROOT/'stage'

PACK_SIZE=33619968
PACK_SHA='c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3'
PACK_RVA=0x1bf000
PACK_FILE_OFF=0x1b8400
LOADER_RVA=0x1bc100
RESOURCE_GUARD_RVA=0x1bc5ae

def sha(b):return hashlib.sha256(b).hexdigest()
def parse_sections(data):
    e=struct.unpack_from('<I',data,0x3c)[0]
    n=struct.unpack_from('<H',data,e+6)[0]
    opt=struct.unpack_from('<H',data,e+20)[0]
    sec=e+24+opt
    arr=[]
    for i in range(n):
        o=sec+i*40
        name=data[o:o+8].rstrip(b'\0').decode(errors='ignore')
        vs,va,rs,rp=struct.unpack_from('<IIII',data,o+8)
        ch=struct.unpack_from('<I',data,o+36)[0]
        arr.append({'i':i,'hdr':o,'name':name,'vs':vs,'va':va,'rs':rs,'rp':rp,'ch':ch})
    return e,arr
def r2o(data,rva):
    _,secs=parse_sections(data)
    for s in secs:
        if s['va']<=rva<s['va']+max(s['vs'],s['rs']):
            return s['rp']+(rva-s['va'])
    raise ValueError(hex(rva))
def relcall(src,dst):
    return b'\xE8'+struct.pack('<i',dst-(src+5))
def write_cstr(buf, off, capacity, text):
    b=text.encode('ascii')+b'\0'
    if len(b)>capacity: raise ValueError((text,len(b),capacity))
    buf[off:off+capacity]=b+b'\0'*(capacity-len(b))

def ascii_strings(data,minlen=5):
    out=[]; i=0
    while i<len(data):
        if 32<=data[i]<127:
            j=i
            while j<len(data) and 32<=data[j]<127:j+=1
            if j-i>=minlen and j<len(data) and data[j]==0: out.append((i,bytes(data[i:j])))
            i=j+1
        else:i+=1
    return out


def align_up(v,a): return (v+a-1)&~(a-1)

def build_version_blob():
    # Minimal valid VS_VERSION_INFO with VS_FIXEDFILEINFO. ReShade queries this
    # via the Win32 version APIs for the public add-on version string.
    key='VS_VERSION_INFO'.encode('utf-16le')+b'\0\0'
    fixed=struct.pack('<13I',
        0xFEEF04BD,0x00010000,
        (1<<16)|45,0,
        (1<<16)|45,0,
        0x3F,0,
        0x00040004, # VOS_NT_WINDOWS32
        0x00000002, # VFT_DLL
        0,0,0)
    head=bytearray(struct.pack('<HHH',0,52,0)+key)
    while len(head)%4: head+=b'\0'
    blob=head+fixed
    struct.pack_into('<H',blob,0,len(blob))
    return bytes(blob)

def add_version_resource(data):
    # Reuse verified zero cave in existing .srgbmt raw range; avoids changing section layout.
    e,secs=parse_sections(data)
    optsz=struct.unpack_from('<H',data,e+20)[0]
    opt=e+24
    rva=0x1be800
    raw=r2o(data,rva)
    vb=build_version_blob()
    data_off=align_up(88,4)
    body=bytearray(data_off+len(vb))
    struct.pack_into('<IIHHHH',body,0,0,0,0,0,0,1)
    struct.pack_into('<II',body,16,16,0x80000000|24)
    struct.pack_into('<IIHHHH',body,24,0,0,0,0,0,1)
    struct.pack_into('<II',body,40,1,0x80000000|48)
    struct.pack_into('<IIHHHH',body,48,0,0,0,0,0,1)
    struct.pack_into('<II',body,64,0x0409,72)
    struct.pack_into('<IIII',body,72,rva+data_off,len(vb),1200,0)
    body[data_off:data_off+len(vb)]=vb
    if any(data[raw:raw+len(body)]): raise RuntimeError('VERSIONINFO cave is not zero')
    data[raw:raw+len(body)]=body
    # DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE=2].
    dd=opt+112+2*8
    old_rsrc=struct.unpack_from('<II',data,dd)
    if old_rsrc!=(0,0): raise RuntimeError(f'Unexpected existing resource directory {old_rsrc}')
    struct.pack_into('<II',data,dd,rva,len(body))
    return {'rva':rva,'raw':raw,'size':len(body),'version':'1.45.0.0','container_section':'.srgbmt'}

base=bytearray(BASE.read_bytes())
base_sha=sha(base)
# Validate exact embedded pack before transforming.
pack=SIDECAR_SRC/'PTDE_GI_ENVSPEC_PACK_RGBA.bin'
pb=pack.read_bytes()
assert len(pb)==PACK_SIZE and sha(pb)==PACK_SHA
assert bytes(base[PACK_FILE_OFF:PACK_FILE_OFF+PACK_SIZE])==pb
assert PACK_FILE_OFF+PACK_SIZE==len(base)

# Validate injection cave is zero and inject external sidecar loader.
lo=r2o(base,LOADER_RVA); loader=LOADER.read_bytes()
assert len(loader)<0xF00
assert set(base[lo:lo+len(loader)]) <= {0}
base[lo:lo+len(loader)]=loader

patches=[]
def patch_rva(rva, expected, new, label):
    o=r2o(base,rva); got=bytes(base[o:o+len(expected)])
    if got!=expected: raise RuntimeError(f'{label}: got {got.hex()} expected {expected.hex()}')
    if len(new)!=len(expected): raise RuntimeError('length mismatch')
    base[o:o+len(new)]=new
    patches.append({'label':label,'rva':hex(rva),'old':expected.hex(),'new':new.hex()})

# 1) Disable the ReShade log callback store globally: release has no internal telemetry/log spam.
patch_rva(0x10c374, bytes.fromhex('488905955c0000'), b'\x90'*7, 'disable_internal_log_callback_store')
# 2) Init: replace HOOK PASS diagnostic LEA with external pack loader call, then skip log dispatch block.
patch_rva(0x1b919c, bytes.fromhex('488d3d71520000'), relcall(0x1b919c,LOADER_RVA)+bytes.fromhex('eb3a'), 'external_pack_loader_call_and_skip_init_log')
# 3) Resource realization: only expose state pointer when external pack validated; existing test fails open otherwise.
patch_rva(0x1b827f, bytes.fromhex('4c8ba988221100'), relcall(0x1b827f,RESOURCE_GUARD_RVA)+b'\x90\x90', 'external_pack_ready_guard')

# Client-facing metadata/version.
write_cstr(base,0xdc58,0x48, 'DSRRL Material Response 1.45')
write_cstr(base,0xdca0,0x153, 'DSRRL Material Response 1.45: PTDE material response, SpecRGB, Subsurf, equipment Normal/Diffuse and exact-slot EnvSpec resource bridge for Dark Souls Remastered. Unsupported or unmapped routes fail open to stock DSR.')
write_cstr(base,0x10387a,0x2a, 'DSRRL_Material_Response_1.45.addon64')

# Scrub diagnostic/telemetry strings. Functional paths and shader/resource names are preserved.
telemetry_re=[
    re.compile(br'^\[DSRRL\]\[(?:ASSET|ASSET_DIAG|PTDE_SPEC|ENVSPEC|FAILOPEN|ACTIVE)'),
    re.compile(br'^DSRRL Material Response 1\.3(?: active|$)'),
]
telemetry_fragments=(b'C100_REG=',b'TIER0=',b'TIER1=',b'TIER2=',b'C101_EXACT_REG=',b'C101_SIBLING_REG=',b'UNMAPPED=',b'SELECTOR=',b'DONOR_SELECTOR=',b'TARGET_BINDS=',b'PTDE_DIFFUSE_DRAWS=',b'PTDE_C101_DRAWS=',b'C100_ONLY_DRAWS=',b'LERP_BYPASS=',b'B12_CREATE=',b'B12_HIT=',b'FAILOPEN=')
# protect release metadata offsets from scrub
protected={(0xdc58,0xdc58+0x48),(0xdca0,0xdca0+0x153),(0x10387a,0x10387a+0x2a)}
def is_protected(i,j): return any(i<e and j>s for s,e in protected)
scrubbed=[]
for i,s in ascii_strings(base):
    j=i+len(s)
    if is_protected(i,j): continue
    match=any(r.search(s) for r in telemetry_re) or any(f in s for f in telemetry_fragments) or b'V15.7 active BSS-safe' in s
    if match:
        base[i:j]=b'\0'*(j-i)
        scrubbed.append({'offset':hex(i),'text':s.decode(errors='replace')[:180]})

# Strip embedded pack physically while retaining .srgbmt VirtualSize so loader gets zero-filled reserve at same RVA.
e,secs=parse_sections(base)
srgbmt=next(s for s in secs if s['name']=='.srgbmt')
assert srgbmt['rp']==0x12d400 and srgbmt['va']==0x134000
new_raw=PACK_FILE_OFF-srgbmt['rp']
assert new_raw==0x8b000
struct.pack_into('<I',base,srgbmt['hdr']+16,new_raw) # SizeOfRawData
# Keep VirtualSize unchanged (0x209b000), so PACK_RVA memory remains mapped/zero-filled.
base=base[:PACK_FILE_OFF]
version_resource=add_version_resource(base)
OUT.write_bytes(base)

# Prepare release sidecar tree and updated manifest.
if STAGE.exists(): shutil.rmtree(STAGE)
(STAGE/'DSRRL/EnvSpec/PackedGI').mkdir(parents=True)
shutil.copy2(OUT, STAGE/OUT.name)
for fn in ['PTDE_GI_ENVSPEC_PACK_RGBA.bin','PTDE_GI_ENVSPEC_PACK_INDEX.csv']:
    shutil.copy2(SIDECAR_SRC/fn, STAGE/'DSRRL/EnvSpec/PackedGI'/fn)
manifest={
  'schema':'dsrrl.envspec.packedgi.client.1.45',
  'namespace':'DSRRL\\EnvSpec\\PackedGI',
  'client_version':'1.45',
  'target':'PTDE packed-GI EnvSpec exact sample-equivalent resource corpus',
  'loader':{
    'mode':'external sidecar; loaded into reserved PE virtual range at startup',
    'path':'DSRRL\\EnvSpec\\PackedGI\\PTDE_GI_ENVSPEC_PACK_RGBA.bin',
    'runtime_validation':['exact file size','SHA-256 via Windows CryptoAPI'],
    'fail_open':True
  },
  'pack':{
    'file':'PTDE_GI_ENVSPEC_PACK_RGBA.bin','size_bytes':PACK_SIZE,'sha256':PACK_SHA,
    'probe_count':342,'slots_per_probe':4,'resource_count':1368,'resource_stride_bytes':24576,
    'width':32,'height':32,'faces':6,'mip_levels':1,'gpu_format':'DXGI_FORMAT_R8G8B8A8_UNORM','row_pitch':128,'face_pitch':4096
  },
  'index':{'file':'PTDE_GI_ENVSPEC_PACK_INDEX.csv','rows':1368,'sha256':sha((SIDECAR_SRC/'PTDE_GI_ENVSPEC_PACK_INDEX.csv').read_bytes())},
  'routing':{'probe_identity':'DSR CPU semantic EnvSpec A/B provenance','material_slot':'368/368 exact authored g_EnvSpcSlotNo 0..3','no_nearest_probe':True,'no_cross_version_numeric_guess':True},
  'status':{'V15_7_resource_bridge_runtime':'PASS','client_1_45_external_loader_runtime':'NOT_TESTED','pixel_equivalence':'OPEN_STOCK_DSR_RECEIVER'}
}
(STAGE/'DSRRL/EnvSpec/PackedGI/manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(STAGE/'DSRRL/EnvSpec/PackedGI/README.txt').write_text('DSRRL Material Response 1.45 — external PTDE EnvSpec PackedGI sidecar. Keep this directory structure unchanged. If the exact pack is absent or fails size/SHA-256 validation, the EnvSpec replacement fails open to stock DSR.\n')
(STAGE/'README_INSTALL.txt').write_text('''DSRRL Material Response 1.45\n\nInstall into the DARK SOULS REMASTERED game directory:\n- DSRRL_Material_Response_1.45.addon64 -> game root (next to DarkSoulsRemastered.exe / dxgi.dll)\n- DSRRL\\EnvSpec\\PackedGI\\... -> keep exactly as packaged\n\nThis client build removes diagnostic telemetry and no longer embeds the 33.6 MB PTDE EnvSpec cubemap pack inside the addon. Existing DSRRL equipment Specular/Normal/Diffuse sidecars from the main mod remain separate and are not duplicated in this package.\n\nEnvSpec scope in 1.45: exact PTDE cubemap resources + exact material slot routing are active; the receiver equation is still stock DSR, so PTDE pixel-equivalence is not claimed.\n''')

out=OUT.read_bytes()
# Static release assertions.
_,osecs=parse_sections(out); osrgb=next(s for s in osecs if s['name']=='.srgbmt')
allstr=[s for _,s in ascii_strings(out)]
forbidden=[s.decode(errors='replace') for s in allstr if (b'[DSRRL][ASSET' in s or b'[DSRRL][ENVSPEC' in s or b'CAPTURE_' in s or b'T12_T14_BIND PASS' in s or b'FAILOPEN=' in s or b'V15.7' in s)]
assert not forbidden, forbidden[:10]
assert pb[:64] not in out
assert len(out)>=PACK_FILE_OFF and out[PACK_FILE_OFF:PACK_FILE_OFF+2] != pb[:2]
assert osrgb['rs']==0x8b000 and osrgb['vs']==0x209b000
assert out[r2o(out,LOADER_RVA):r2o(out,LOADER_RVA)+len(loader)]==loader
assert b'DSRRL Material Response 1.45\0' in out
assert b'DSRRL_Material_Response_1.45.addon64\0' in out
assert b'DSRRL Material Response 1.3 active' not in out

# deterministic client ZIP
files=[]
for p in sorted(STAGE.rglob('*')):
    if p.is_file(): files.append(p)
with zipfile.ZipFile(PKG,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in files:
        rel=p.relative_to(STAGE).as_posix()
        zi=zipfile.ZipInfo(rel,date_time=(2026,9,20,0,0,0));zi.compress_type=zipfile.ZIP_DEFLATED;zi.external_attr=0o644<<16
        z.writestr(zi,p.read_bytes())

# Release audit
final_strings=[s.decode(errors='replace') for _,s in ascii_strings(out)]
audit={
 'schema':'dsrrl.material_response.client_release_audit.v1',
 'client_version':'1.45',
 'basis':{'build_id':82,'version':'V15.7','sha256':base_sha,'runtime_liveness':'PASS','bridge_activation':'PASS','pixel_behavior':'OPEN_STOCK_RECEIVER'},
 'release_addon':{'file':OUT.name,'size_bytes':len(out),'sha256':sha(out),'embedded_pack':False,'registration_name_present':b'DSRRL Material Response 1.45\0' in out,'file_version':'1.45.0.0'},
 'envspec_sidecar':{'path':'DSRRL/EnvSpec/PackedGI/PTDE_GI_ENVSPEC_PACK_RGBA.bin','size_bytes':PACK_SIZE,'sha256':PACK_SHA,'runtime_exact_size_validation':True,'runtime_sha256_validation':True,'fail_open_guard':True,'reserved_pack_rva':hex(PACK_RVA)},
 'pe':{'srgbmt_virtual_size':hex(osrgb['vs']),'srgbmt_raw_size':hex(osrgb['rs']),'physical_pack_removed_bytes':PACK_SIZE,'loader_rva':hex(LOADER_RVA),'resource_guard_rva':hex(RESOURCE_GUARD_RVA),'version_resource':version_resource},
 'telemetry_cleanup':{'internal_log_callback_store_disabled':True,'scrubbed_string_count':len(scrubbed),'forbidden_runtime_diagnostic_strings_remaining':forbidden,'periodic_output_possible':False,'note':'internal counters may remain as inert implementation state but no ReShade telemetry callback is retained'},
 'behavior_preservation':{'three_active_V15_7_PRE_PREPARE_POST_calls_unchanged':True,'exact_368_route_slot_table_unchanged':True,'per_thread_A_B_semantics_unchanged':True,'gpu_identity_two_hit_unchanged':True,'t12_t14_save_bind_restore_unchanged':True,'BSS_fix_unchanged':True,'receiver_math':'stock DSR unchanged'},
 'metadata':{'display':'DSRRL Material Response 1.45','addon_filename':'DSRRL_Material_Response_1.45.addon64','diagnostic_V15_strings_removed':not any('V15.7' in s for s in final_strings)},
 'compatibility':{'known_gap':'extension code still lacks host .pdata unwind coverage','status':'PARTIAL','client_runtime_after_externalization':'NOT_TESTED'},
 'package':{'file':PKG.name,'sha256':sha(PKG.read_bytes()),'files':[p.relative_to(STAGE).as_posix() for p in files]},
 'status':{'construction':'PASS','static_release_audit':'PASS','runtime_liveness':'OPEN_AFTER_EXTERNALIZATION','bridge_activation':'OPEN_AFTER_EXTERNALIZATION','pixel_behavior':'OPEN'}
}
AUD.write_text(json.dumps(audit,indent=2)+'\n')
print(json.dumps({'addon':str(OUT),'addon_size':len(out),'addon_sha256':sha(out),'package':str(PKG),'package_sha256':sha(PKG.read_bytes()),'audit':str(AUD),'scrubbed':len(scrubbed),'patches':patches},indent=2))
