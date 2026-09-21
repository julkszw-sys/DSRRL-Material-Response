from pathlib import Path
import struct, hashlib, json, shutil, zipfile, sys
sys.path.insert(0,'/mnt/data/blackfix_re')
from dxbc_checksum import checksum_dxbc

BASE_ADDON=Path('/mnt/data/v3_work/DSRRL_Material_Response_1.45_PMETAL_PTDE_SURFACE_ISLAND_V3_EXACT.addon64')
V3DX=Path('/mnt/data/blackfix_re')
OUTDIR=Path('/mnt/data/DSRRL_Material_Response_1.45_PMETAL_FRESH_SPECRGB_V5_2026-09-21')
ZIP=Path('/mnt/data/DSRRL_Material_Response_1.45_PMETAL_FRESH_SPECRGB_V5_RUNTIME_2026-09-21.zip')
TARGET={
 33:{'sample_old':1316,'nop_start':1652,'mul':1667,'terminal':2822,'uv':'v7'},
 34:{'sample_old':1225,'nop_start':1561,'mul':1576,'terminal':2741,'uv':'v8'},
 35:{'sample_old':885,'nop_start':1221,'mul':1236,'terminal':2394,'uv':'v7'},
}
NOP=0x0100003a
SAT_BIT=0x2000

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

def shex(blob):
    n=struct.unpack_from('<I',blob,28)[0]
    for k in range(n):
        o=struct.unpack_from('<I',blob,32+4*k)[0]; tag=blob[o:o+4]; sz=struct.unpack_from('<I',blob,o+4)[0]
        if tag in (b'SHEX',b'SHDR'):
            return o+8,list(struct.unpack_from('<'+'I'*(sz//4),blob,o+8))
    raise RuntimeError('no SHEX/SHDR')

def valid(blob): return blob[4:20]==checksum_dxbc(blob)

def patch_v3_blob(blob,idx):
    assert valid(blob), f'input checksum bad {idx}'
    base=bytes(blob); b=bytearray(blob); off,w=shex(blob); t=TARGET[idx]
    old_sample=w[t['sample_old']:t['sample_old']+11]
    assert len(old_sample)==11
    assert (old_sample[0]&0x7ff)==0x45
    assert old_sample[7]==0x00107936 and old_sample[8]==0x0000000a
    assert old_sample[9]==0x00106000 and old_sample[10]==0x00000001
    assert all(x==NOP for x in w[t['nop_start']:t['mul']])
    assert t['mul']-t['nop_start']==15
    fresh=list(old_sample)
    fresh[3]=0x00100072
    fresh[4]=0x00000002
    w[t['nop_start']:t['nop_start']+11]=fresh
    assert all(x==NOP for x in w[t['nop_start']+11:t['mul']])
    mul=w[t['mul']:t['mul']+8]
    assert mul[0]==0x08000038 and mul[1]==0x00100072 and mul[2]==2
    assert mul[4] in (9,10,11)
    w[t['mul']+4]=2
    assert (w[t['terminal']] & 0x7ff)==0x36
    w[t['terminal']] |= SAT_BIT
    struct.pack_into('<'+'I'*len(w),b,off,*w)
    b[4:20]=checksum_dxbc(bytes(b))
    out=bytes(b); assert valid(out); assert len(out)==len(base)
    return out, {
      'index':idx,
      'old_sample_word':t['sample_old'],
      'fresh_sample_word':t['nop_start'],
      'final_material_mul_word':t['mul'],
      'old_wrong_material_register':{33:'r11',34:'r10',35:'r9'}[idx],
      'new_material_register':'r2 fresh t10 SpecRGB',
      'terminal_word':t['terminal'],
      'terminal_sat':True,
      'sha256_before':hashlib.sha256(base).hexdigest(),
      'sha256_after':hashlib.sha256(out).hexdigest(),
      'checksum_valid':True,
    }

if OUTDIR.exists(): shutil.rmtree(OUTDIR)
OUTDIR.mkdir(parents=True)
base_addon=BASE_ADDON.read_bytes(); emb=scan(base_addon); assert len(emb)==78
addon=bytearray(base_addon); audit_targets=[]; changed=[]
for idx,(eo,sz,old) in enumerate(emb):
    if idx in TARGET:
        stand=(V3DX/f'v3_{idx}.dxbc').read_bytes(); assert stand==old
        new,info=patch_v3_blob(old,idx)
        addon[eo:eo+sz]=new; audit_targets.append(info); changed.append(idx)
assert changed==[33,34,35]
allowed=[(emb[i][0],emb[i][0]+emb[i][1]) for i in changed]
assert all(any(a<=j<b for a,b in allowed) for j,(x,y) in enumerate(zip(base_addon,addon)) if x!=y)
emb2=scan(bytes(addon)); assert len(emb2)==78
changed2=[]
for i,(a,b) in enumerate(zip(emb,emb2)):
    if a[2]!=b[2]: changed2.append(i)
    assert valid(b[2]), f'bad output checksum {i}'
assert changed2==changed

out_addon=OUTDIR/'DSRRL_Material_Response_1.45.addon64'; out_addon.write_bytes(addon)
addon_sha=hashlib.sha256(addon).hexdigest()

audit={
 'build':'Material Response 1.45 P_Metal Fresh SpecRGB V5',
 'basis':'V3 exact EnvSpec island (build107)',
 'basis_addon_sha256':hashlib.sha256(base_addon).hexdigest(),
 'output_addon_sha256':addon_sha,
 'construction_status':'PASS','runtime_status':'NOT_TESTED','pixel_status':'OPEN',
 'root_cause':'V3 final material multiply consumed stale DSR temp r11/r10/r9 after original t10 SpecRGB sample had been overwritten.',
 'fix':'reacquire t10 at final material cut into r2.xyz; then fresh SpecRGB * cb12[0].xyz(c101) * COLOR0; terminal RGB SAT',
 'changed_dxbc_indices':changed,
 'all_78_dxbc_checksums_valid':True,
 'addon_diff_confined_to_target_dxbc':True,
 'v4b_atmosphere_domain_patch_inherited':False,
 'ul_changed':False,'pointlight_changed':False,'envdiffuse_changed':False,
 'targets':audit_targets,
}
(OUTDIR/'V5_RUNTIME_AUDIT.json').write_text(json.dumps(audit,indent=2),encoding='utf-8')
shutil.copy2(__file__,OUTDIR/'BUILD_V5_FRESH_SPECRGB.py')
for n in ['VERIFY_ENVSPC_PACK.ps1','VERIFY_ENVSPC_PACK.cmd']:
    src=Path('/mnt/data/DSRRL_Material_Response_1.45_PMETAL_PTDE_DOMAIN_BRIDGE_V4B_2026-09-21')/n
    if src.exists(): shutil.copy2(src,OUTDIR/n)
files=sorted(p for p in OUTDIR.iterdir() if p.is_file() and p.name!='MANIFEST_SHA256.txt')
with (OUTDIR/'MANIFEST_SHA256.txt').open('w',encoding='utf-8') as f:
    for p in files: f.write(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n')
if ZIP.exists(): ZIP.unlink()
with zipfile.ZipFile(ZIP,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in sorted(OUTDIR.iterdir()):
        if p.is_file(): z.write(p,p.name)
with zipfile.ZipFile(ZIP) as z: assert z.testzip() is None
print(json.dumps({'addon':str(out_addon),'addon_sha256':addon_sha,'zip':str(ZIP),'zip_sha256':hashlib.sha256(ZIP.read_bytes()).hexdigest(),'zip_size':ZIP.stat().st_size,'changed_dxbc':changed,'targets':audit_targets},indent=2))
