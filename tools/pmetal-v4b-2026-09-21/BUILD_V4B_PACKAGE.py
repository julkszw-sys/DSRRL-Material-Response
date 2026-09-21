from pathlib import Path
import struct, hashlib, json, shutil, zipfile, sys
sys.path.insert(0,'/mnt/data/blackfix_re')
from dxbc_checksum import checksum_dxbc

BASE=Path('/mnt/data/v3_work/DSRRL_Material_Response_1.45_PMETAL_PTDE_SURFACE_ISLAND_V3_EXACT.addon64')
PATCHDIR=Path('/mnt/data/v4_domain_work/dxbc')
OUTDIR=Path('/mnt/data/DSRRL_Material_Response_1.45_PMETAL_PTDE_DOMAIN_BRIDGE_V4B_2026-09-21')
ZIP=Path('/mnt/data/DSRRL_Material_Response_1.45_PMETAL_PTDE_DOMAIN_BRIDGE_V4B_RUNTIME_2026-09-21.zip')
if OUTDIR.exists(): shutil.rmtree(OUTDIR)
OUTDIR.mkdir(parents=True)

def scan(data):
    arr=[]; pos=0
    while True:
        i=data.find(b'DXBC',pos)
        if i<0: break
        if i+32<=len(data):
            sz=struct.unpack_from('<I',data,i+24)[0]; n=struct.unpack_from('<I',data,i+28)[0]
            if 32<=sz<=200000 and i+sz<=len(data) and 1<=n<=32:
                arr.append((i,sz,data[i:i+sz]))
        pos=i+4
    return arr

def shex_words(blob):
    n=struct.unpack_from('<I',blob,28)[0]
    for k in range(n):
        o=struct.unpack_from('<I',blob,32+4*k)[0]; tag=blob[o:o+4]; sz=struct.unpack_from('<I',blob,o+4)[0]
        if tag in (b'SHEX',b'SHDR'):
            return list(struct.unpack_from('<'+'I'*(sz//4),blob,o+8))
    raise RuntimeError('no SHEX')

base=BASE.read_bytes(); emb=scan(base)
assert len(emb)==78
addon=bytearray(base)
changed=[]; target_audit=[]
for idx,(off,sz,old) in enumerate(emb):
    new=(PATCHDIR/f'{idx:02d}.dxbc').read_bytes()
    assert len(new)==sz
    assert new[4:20]==checksum_dxbc(new)
    if new!=old:
        assert idx in (33,34,35)
        changed.append(idx)
        addon[off:off+sz]=new
        w=shex_words(new)
        target_audit.append({
            'index':idx,
            'old_sha256':hashlib.sha256(old).hexdigest(),
            'new_sha256':hashlib.sha256(new).hexdigest(),
            'terminal_sat': bool(w[{33:2822,34:2741,35:2394}[idx]] & 0x2000),
            'checksum_valid':True,
        })
assert changed==[33,34,35]
out_addon=OUTDIR/'DSRRL_Material_Response_1.45.addon64'
out_addon.write_bytes(addon)
emb2=scan(bytes(addon)); assert len(emb2)==78
changed2=[]
for i,(a,b) in enumerate(zip(emb,emb2)):
    if a[2]!=b[2]: changed2.append(i)
    assert b[2][4:20]==checksum_dxbc(b[2])
assert changed2==changed
allowed=[(emb[i][0],emb[i][0]+emb[i][1]) for i in changed]
assert all(any(a<=j<b for a,b in allowed) for j,(x,y) in enumerate(zip(base,addon)) if x!=y)

audit={
 'build':'Material Response 1.45 P_Metal PTDE Domain Bridge V4B runtime candidate',
 'basis':'V3 exact EnvSpec island',
 'basis_addon_sha256':hashlib.sha256(base).hexdigest(),
 'output_addon_sha256':hashlib.sha256(bytes(addon)).hexdigest(),
 'construction_status':'PASS','runtime_status':'NOT_TESTED','pixel_status':'OPEN',
 'operator_scope':'shared no-PointLight P_Metal atmosphere-domain + terminal',
 'reason':'Build108/V3 removes hard-black starvation but globally cyan-tints equipment P_Metal. V4B repairs the proven DSR-vs-PTDE ordinary HemEnv domain mismatch rather than applying a gain.',
 'mutation':{
   'prefog':'recover PTDE-linear FogRGB q from DSR cb12.rgb=abs(q)^2.2 using log/mul(1/2.2)/exp; keep surface result linear',
   'fog':'blend linear surface against recovered linear FogRGB with existing homologous Fog weight',
   'lightscattering':'retain existing cb13..20 LightScattering kernel',
   'post_lightscattering':'remove DSR-only abs(x)^2.2 continuation entirely (NOP 21 dwords)',
   'terminal':'set SAT modifier on final separate RGB MOV; alpha untouched'
 },
 'unchanged':['V3 PTDE EnvSpec t12/t14 resources','direct reflection R','LOD0','RGB/A decode','PTDE A/B source and beta','PTDE c101/SpecRGB/COLOR0 path','exact PTDE s12/s14 sampler','Upper/Lower authored inputs','PointLight','Fog Begin/End/strength','LightScattering authored constants','routing/material gate'],
 'known_residual':'PTDE c135.x/c135.y scene-encoding scale is not hardcoded or guessed in V4B; terminal SAT is exact, c135 dynamic scale remains OPEN. This candidate is intended to test the now-closed atmosphere-domain mismatch without conflating it with an unproven 0.5 constant.',
 'changed_dxbc_indices':changed,
 'embedded_dxbc_count':78,'all_dxbc_checksums_valid':True,
 'targets':target_audit
}
(OUTDIR/'V4B_RUNTIME_AUDIT.json').write_text(json.dumps(audit,indent=2),encoding='utf-8')
readme='''DSRRL Material Response 1.45 — P_Metal PTDE Domain Bridge V4B

Drop-in runtime candidate based on the corrected 1.45 / V3 exact EnvSpec island.

V4B changes only the three exact P_Metal alternate pixel shaders (33/34/35):
- recovers PTDE-linear FogRGB from DSR cb12.rgb (which stores q^2.2),
- keeps the P_Metal surface linear into Fog,
- retains the existing homologous LightScattering kernel,
- removes the DSR-only post-LightScattering x^2.2 continuation,
- enables the confirmed PTDE terminal RGB SAT.

NOT changed: U/L, PointLight, EnvSpec resources/source/sample math, c87, c101, SpecRGB, COLOR0, routing.

Important: this build does not hardcode c135.x/c135.y. The typical PTDE 1/2 scene-encoding ratio is not assumed as a universal constant here. Runtime/pixel status remains open until tested.

Keep existing sidecar:
DSRRL\\EnvSpec\\PackedGI\\PTDE_GI_ENVSPEC_PACK_RGBA.bin
size 33,619,968
SHA256 c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3
'''
(OUTDIR/'README_RUNTIME_TEST.txt').write_text(readme,encoding='utf-8')
for n in ['VERIFY_ENVSPC_PACK.ps1','VERIFY_ENVSPC_PACK.cmd']:
    for p in [Path('/mnt/data/DSRRL_Material_Response_1.45_BLACK_ARMOR_EXACT_FIX_RUNTIME_2026-09-21')/n,Path('/mnt/data/DSRRL_Material_Response_1.45_PMETAL_PTDE_SURFACE_ISLAND_V3_EXACT_2026-09-21')/n]:
        if p.exists(): shutil.copy2(p,OUTDIR/n); break
shutil.copy2('/mnt/data/blackfix_re/dxbc_checksum_patch_v4.py',OUTDIR/'PATCH_DXBC_V4B.py')
shutil.copy2('/mnt/data/blackfix_re/build_v4b_domain_bridge.py',OUTDIR/'BUILD_V4B_PACKAGE.py')
files=sorted([p for p in OUTDIR.iterdir() if p.is_file() and p.name!='MANIFEST_SHA256.txt'])
with (OUTDIR/'MANIFEST_SHA256.txt').open('w',encoding='utf-8') as f:
    for p in files: f.write(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n')
if ZIP.exists(): ZIP.unlink()
with zipfile.ZipFile(ZIP,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in sorted(OUTDIR.iterdir()):
        if p.is_file(): z.write(p,p.name)
with zipfile.ZipFile(ZIP) as z: assert z.testzip() is None
print(json.dumps({'addon':str(out_addon),'addon_sha256':hashlib.sha256(out_addon.read_bytes()).hexdigest(),'zip':str(ZIP),'zip_sha256':hashlib.sha256(ZIP.read_bytes()).hexdigest(),'zip_size':ZIP.stat().st_size,'changed':changed},indent=2))
