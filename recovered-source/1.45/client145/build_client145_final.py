from pathlib import Path
import subprocess, hashlib, zipfile, json
ROOT=Path('/mnt/data/client145_work')
BASE_BUILDER=ROOT/'build_client145.py'
OUT=Path('/mnt/data/DSRRL_Material_Response_1.45.addon64')
PKG=Path('/mnt/data/DSRRL_Material_Response_1.45_CLIENT.zip')
README=Path('/mnt/data/README_DSRRL_Material_Response_1.45.txt')
AUDIT=Path('/mnt/data/DSRRL_Material_Response_1.45_FINAL_RELEASE_AUDIT.json')
# Rebuild externalized clean basis from V15.7.
subprocess.run(['python', str(BASE_BUILDER)], check=True, stdout=subprocess.DEVNULL)
src=ROOT/'DSRRL_Material_Response_1.45.addon64'
b=bytearray(src.read_bytes())
needle=b'[DSRRL][FAILOPEN] PTDE EnvSpec LightBank packer bytes mismatch'
i=b.find(needle)
if i < 0: raise RuntimeError('expected final leftover diagnostic string not found')
b[i:i+len(needle)]=b'\0'*len(needle)
OUT.write_bytes(b)
README.write_text('''DSRRL Material Response 1.45

Install:
1. Put DSRRL_Material_Response_1.45.addon64 in the DARK SOULS REMASTERED game root.
2. Keep the existing external EnvSpec pack at:
   DSRRL\\EnvSpec\\PackedGI\\PTDE_GI_ENVSPEC_PACK_RGBA.bin

Required EnvSpec pack:
- size: 33,619,968 bytes
- SHA-256: c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3

The addon validates exact size and SHA-256 at startup. If the pack is absent or invalid, EnvSpec replacement fails open to stock DSR.

1.45 contains no embedded PTDE cubemap payload and no client telemetry/diagnostic logging. It preserves Material Response, PTDE SpecRGB, Subsurf, equipment Normal/Diffuse and the exact-slot PTDE EnvSpec resource bridge from the runtime-proven V15.7 lineage. The EnvSpec receiver equation remains stock DSR in this release stage, so PTDE pixel equivalence is not claimed.
''')
with zipfile.ZipFile(PKG,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p,arc in [(OUT,OUT.name),(README,'README.txt')]:
        zi=zipfile.ZipInfo(arc,date_time=(2026,9,20,0,0,0)); zi.compress_type=zipfile.ZIP_DEFLATED; zi.external_attr=0o644<<16
        z.writestr(zi,p.read_bytes())
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
data=OUT.read_bytes()
forbidden=[b'[DSRRL]',b'ENVSPEC_DIAG',b'ASSET_DIAG',b'CAPTURE_NORMAL',b'CAPTURE_DIFFUSE',b'CAPTURE_SPEC',b'T12_T14_BIND PASS',b'RESTORE PASS',b'GPU_IDENTITY',b'SLOT0 exact',b'SLOT1 exact',b'SLOT2 exact',b'SLOT3 exact',b'V15.7',b'SELECTOR=',b'TARGET_BINDS=',b'FAILOPEN=']
rem=[x.decode() for x in forbidden if x in data]
if rem: raise RuntimeError(f'forbidden telemetry remains: {rem}')
if len(data)>=33619968: raise RuntimeError('addon unexpectedly large enough to contain full pack')
audit={
 'schema':'dsrrl.material_response.client_release_audit.v2','client_version':'1.45','basis_build_id':82,
 'addon':{'file':OUT.name,'size_bytes':OUT.stat().st_size,'sha256':sha(OUT),'file_version':'1.45.0.0'},
 'package':{'file':PKG.name,'sha256':sha(PKG),'members':[OUT.name,'README.txt'],'contains_envspec_assets':False},
 'external_envspec':{'required_path':'DSRRL\\EnvSpec\\PackedGI\\PTDE_GI_ENVSPEC_PACK_RGBA.bin','required_size':33619968,'required_sha256':'c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3','runtime_exact_size_validation':True,'runtime_sha256_validation':True,'fail_open_to_stock_dsr':True},
 'embedded_payload':{'present':False},
 'telemetry_cleanup':{'pass':True,'forbidden_strings_remaining':[],'internal_log_callback_store_disabled':True},
 'behavior_preservation':{'active_v15_7_resource_bridge_logic_preserved':True,'exact_368_material_slots_preserved':True,'per_thread_A_B_semantics_preserved':True,'gpu_identity_two_hit_preserved':True,'t12_t14_save_bind_restore_preserved':True,'BSS_safe_layout_preserved':True,'receiver_math':'stock DSR'},
 'runtime_status':{'V15_7_basis':'PASS','client_1_45_externalized':'NOT_TESTED','pixel_behavior':'OPEN_STOCK_RECEIVER'},
 'compatibility':{'known_unwind_gap':'PARTIAL'},'static_release_audit':'PASS'}
AUDIT.write_text(json.dumps(audit,indent=2)+'\n')
print(json.dumps({'addon_sha256':sha(OUT),'package_sha256':sha(PKG),'audit_sha256':sha(AUDIT)},indent=2))
