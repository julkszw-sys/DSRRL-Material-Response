from __future__ import annotations
from pathlib import Path
import hashlib, json, os, shutil, sys, zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT.parent.parent / 'DSRRL_REAL_PTDE_UL_LIVE_SUCCESSOR_2026-09-08.zip'
EXT_AUDIT = ROOT.parent.parent / 'DSRRL_REAL_PTDE_UL_LIVE_SUCCESSOR_2026-09-08_AUDIT.json'
FIXED_DT = (2026, 9, 8, 12, 0, 0)
EXCLUDE_NAMES = {'MANIFEST_SHA256.txt'}
EXCLUDE_PARTS = {'__pycache__', 'build-msvc', '.git'}

def sha_bytes(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()

def sha_file(p: Path) -> str:
    h=hashlib.sha256()
    with p.open('rb') as f:
        for c in iter(lambda:f.read(1<<20),b''): h.update(c)
    return h.hexdigest()

def payload_files():
    out=[]
    for p in ROOT.rglob('*'):
        if not p.is_file(): continue
        rel=p.relative_to(ROOT)
        if p.name in EXCLUDE_NAMES: continue
        if any(x in EXCLUDE_PARTS for x in rel.parts): continue
        out.append((rel,p))
    return sorted(out,key=lambda x:x[0].as_posix())

def write_internal_audit():
    static=json.loads((ROOT/'STATIC_AUDIT.json').read_text())
    hosts=json.loads((ROOT/'data/UL48_LIVE_PAYLOAD_AUDIT.json').read_text())
    a267=ROOT/'INPUT_ARTIFACT267/DSRRL_RENDERER_PTDE_UL_PARAM_LAYER_GAMEPLAY_ONLY_2026-09-08.zip'
    doc={
      'status':'PASS',
      'classification':'CONSTRUCTION_ONLY_OWNER_DELIVERY_REAL_PTDE_UL_LIVE_SUCCESSOR',
      'runtime_validation':'OPEN',
      'final_pixel_fidelity':'OPEN',
      'baseline':'P2.2 surface + stock DSR HDR',
      'operator':'Upper/Lower Ambient only',
      'envspec_live_lane':False,
      'artifact267_sha256':sha_file(a267),
      'static_audit':f"{static['checks_pass']}/{static['checks_total']} PASS",
      'ul48':{
         'hosts':hosts['host_count'],'hemenv':hosts['hemenv'],'hemenvlerp':hosts['hemenvlerp'],
         'rdef_stripped':hosts['rdef_stripped'],'b13_declared':hosts['b13_declared'],
         'p22_exact_base':hosts['p22_exact_base'],'local_ul_island_only':hosts['local_ul_island_only'],
         'unique_payloads':len({r['ul_sha256'] for r in hosts['records']}),
      },
      'producer_contract':{
        'type5_wrapper_rva':'0x1C0BE0','type6_wrapper_rva':'0x1C0C10',
        'blend_rgbm_helper_rva':'0x5642F0','single_rgbm_helper_rva':'0x564510',
        'selector_rva':'0x22BA20','raw_rgbm_layout':'int16 R,G,B,M',
        'decode':'RGB/255 * M/100','blend':'A + beta(B-A)',
        'snapshot_freshness':'owner_context + selector_A + selector_B + bit-exact beta',
      },
      'windows_native_build':'OPEN_OWNER_MSVC_BUILD_REQUIRED',
      'note':'Successful packaging/static validation does not claim Windows runtime execution or PTDE final-pixel fidelity.'
    }
    (ROOT/'FINAL_PACKAGE_AUDIT.json').write_text(json.dumps(doc,indent=2)+'\n')

def write_manifest():
    lines=[]
    for rel,p in payload_files():
        lines.append(f"{sha_file(p)}  {rel.as_posix()}")
    (ROOT/'MANIFEST_SHA256.txt').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')

def zip_once(path:Path):
    files=payload_files()+[(Path('MANIFEST_SHA256.txt'),ROOT/'MANIFEST_SHA256.txt')]
    files=sorted(files,key=lambda x:x[0].as_posix())
    with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for rel,p in files:
            zi=zipfile.ZipInfo(rel.as_posix(),FIXED_DT)
            zi.compress_type=zipfile.ZIP_DEFLATED
            zi.external_attr=(0o100644 & 0xFFFF)<<16
            zi.create_system=3
            z.writestr(zi,p.read_bytes(),compress_type=zipfile.ZIP_DEFLATED,compresslevel=9)

def verify_zip(path:Path):
    with zipfile.ZipFile(path,'r') as z:
        bad=z.testzip()
        if bad is not None: raise RuntimeError(f'CRC failure: {bad}')
        names=z.namelist()
        if len(names)!=len(set(names)): raise RuntimeError('duplicate ZIP member')
        manifest=z.read('MANIFEST_SHA256.txt').decode('utf-8').splitlines()
        for line in manifest:
            h, name=line.split('  ',1)
            got=sha_bytes(z.read(name))
            if got!=h: raise RuntimeError(f'manifest mismatch: {name}')
        return len(names),len(manifest)

def main():
    for p in ROOT.rglob('__pycache__'):
        if p.is_dir(): shutil.rmtree(p)
    if (ROOT/'build-msvc').exists(): shutil.rmtree(ROOT/'build-msvc')
    write_internal_audit(); write_manifest()
    OUT.unlink(missing_ok=True); zip_once(OUT)
    members,manifest_count=verify_zip(OUT)
    tmp=OUT.with_suffix('.rebuild.zip'); tmp.unlink(missing_ok=True); zip_once(tmp)
    deterministic=OUT.read_bytes()==tmp.read_bytes(); tmp.unlink()
    if not deterministic: raise RuntimeError('deterministic rebuild mismatch')
    result={
      'status':'PASS','classification':'CONSTRUCTION_ONLY_OWNER_DELIVERY_REAL_PTDE_UL_LIVE_SUCCESSOR',
      'file_name':OUT.name,'sha256':sha_file(OUT),'size_bytes':OUT.stat().st_size,
      'archive_member_count':members,'manifest_payload_count':manifest_count,
      'zip_crc':'PASS','manifest_verify':'PASS','deterministic_rebuild':'PASS',
      'static_audit':json.loads((ROOT/'STATIC_AUDIT.json').read_text())['status'],
      'mock_api20_compile':'PASS_PREVIOUS_STEP',
      'runtime_validation':'OPEN','final_pixel_fidelity':'OPEN',
      'artifact267_sha256':sha_file(ROOT/'INPUT_ARTIFACT267/DSRRL_RENDERER_PTDE_UL_PARAM_LAYER_GAMEPLAY_ONLY_2026-09-08.zip')
    }
    EXT_AUDIT.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__': main()
