from pathlib import Path
import struct, hashlib, json, shutil, zipfile, sys
sys.path.insert(0,'/mnt/data/blackfix_re')
from dxbc_checksum import checksum_dxbc

BASE=Path('/mnt/data/v3_work/DSRRL_Material_Response_1.45_PMETAL_PTDE_SURFACE_ISLAND_V3_EXACT.addon64')
PATCHDIR=Path('/mnt/data/v4_domain_work/dxbc')
OUTDIR=Path('/mnt/data/DSRRL_Material_Response_1.45_PMETAL_PTDE_DOMAIN_BRIDGE_V4B_2026-09-21')
ZIP=Path('/mnt/data/DSRRL_Material_Response_1.45_PMETAL_PTDE_DOMAIN_BRIDGE_V4B_RUNTIME_2026-09-21.zip')

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

if OUTDIR.exists(): shutil.rmtree(OUTDIR)
OUTDIR.mkdir(parents=True)
base=BASE.read_bytes(); emb=scan(base)
assert len(emb)==78
addon=bytearray(base)
changed=[]; target_audit=[]
for idx,(off,sz,old) in enumerate(emb):
    new=(PATCHDIR/f'{idx:02d}.dxbc').read_bytes()
    assert len(new)==sz and new[4:20]==checksum_dxbc(new)
    if new!=old:
        assert idx in (33,34,35)
        changed.append(idx); addon[off:off+sz]=new
        w=shex_words(new)
        target_audit.append({
            'index':idx,
            'old_sha256':hashlib.sha256(old).hexdigest(),
            'new_sha256':hashlib.sha256(new).hexdigest(),
            'terminal_sat':bool(w[{33:2822,34:2741,35:2394}[idx]] & 0x2000),
            'checksum_valid':True})
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
 'changed_dxbc_indices':changed,'embedded_dxbc_count':78,
 'all_dxbc_checksums_valid':True,'targets':target_audit}
(OUTDIR/'V4B_RUNTIME_AUDIT.json').write_text(json.dumps(audit,indent=2),encoding='utf-8')
if ZIP.exists(): ZIP.unlink()
with zipfile.ZipFile(ZIP,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in sorted(OUTDIR.iterdir()):
        if p.is_file(): z.write(p,p.name)
with zipfile.ZipFile(ZIP) as z: assert z.testzip() is None
print(hashlib.sha256(out_addon.read_bytes()).hexdigest())
print(hashlib.sha256(ZIP.read_bytes()).hexdigest())
