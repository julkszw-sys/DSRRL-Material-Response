from __future__ import annotations
from pathlib import Path
import argparse, hashlib, json, struct, zipfile, re, sys
ROOT=Path(__file__).resolve().parents[1]
EXPECTED_EXE='a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b'
EXPECTED_BINDER='ad180732ac79d5d98783aa504c789c2bab15e515c3b0f66b237d8b8c69113394'
A267='2c99a74b45cbc9627a9d3181063cfa6138ca1b6668a7a923d02aa3aa40c569fb'
HOOKS={
  0x1C0BE0: bytes.fromhex('48 83 ec 38 4d 8b c8 f3 0f 11 5c 24 20 4c 8b 41 40'),
  0x1C0C10: bytes.fromhex('48 83 ec 38 4d 8b c8 f3 0f 11 5c 24 20 4c 8b 41 40'),
  0x5642F0: bytes.fromhex('48 8b c4 48 89 58 08 48 89 70 10 57 48 81 ec c0 00 00 00'),
  0x564510: bytes.fromhex('48 89 5c 24 08 57 48 83 ec 40 0f bf 42 06'),
  0x22BA20: bytes.fromhex('40 53 48 83 ec 30 49 63 c0 45 8b d1 48 8b da'),
}
def sha(p:Path): return hashlib.sha256(p.read_bytes()).hexdigest()
def pe_sections(data:bytes):
    pe=struct.unpack_from('<I',data,0x3c)[0]; assert data[pe:pe+4]==b'PE\0\0'; coff=pe+4; n=struct.unpack_from('<H',data,coff+2)[0]; optsz=struct.unpack_from('<H',data,coff+16)[0]; sec=coff+20+optsz; out=[]
    for i in range(n):
        o=sec+40*i; name=data[o:o+8].split(b'\0')[0].decode('ascii','replace'); vs,va,rs,rp=struct.unpack_from('<IIII',data,o+8); out.append((name,va,vs,rp,rs))
    return out
def rva_off(data:bytes,rva:int):
    for _,va,vs,rp,rs in pe_sections(data):
        if va<=rva<va+max(vs,rs): return rp+(rva-va)
    raise KeyError(hex(rva))
def check(cond,name,details,checks): checks.append({'name':name,'pass':bool(cond),'details':details});
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--exe',required=True);ap.add_argument('--binder-dcx',required=True);a=ap.parse_args();checks=[]
    exe=Path(a.exe);binder=Path(a.binder_dcx)
    check(sha(exe)==EXPECTED_EXE,'exact_exe_sha',sha(exe),checks);check(sha(binder)==EXPECTED_BINDER,'exact_flver_binder_sha',sha(binder),checks)
    ed=exe.read_bytes()
    for rva,expected in HOOKS.items():
        off=rva_off(ed,rva); got=ed[off:off+len(expected)];check(got==expected,f'hook_bytes_{rva:06x}',got.hex(),checks)
    z=ROOT/'INPUT_ARTIFACT267/DSRRL_RENDERER_PTDE_UL_PARAM_LAYER_GAMEPLAY_ONLY_2026-09-08.zip'
    check(sha(z)==A267,'artifact267_exact_zip_sha',sha(z),checks)
    with zipfile.ZipFile(z) as q:
        bad=q.testzip();check(bad is None,'artifact267_zip_crc',str(bad),checks); aud=json.loads(q.read('AUDIT_PTDE_UL_PARAM_LAYER.json'))
        check(aud['total_rows_examined']==1246 and aud['total_rows_changed']==173 and aud['total_rows_equal']==899 and aud['total_cutscene_preserved']==174 and aud['total_ul_bytes_changed']==512 and aud['unexpected_raw_bytes_changed']==0,'artifact267_semantic_audit_counts',str({k:aud[k] for k in ('total_rows_examined','total_rows_changed','total_rows_equal','total_cutscene_preserved','total_ul_bytes_changed','unexpected_raw_bytes_changed')}),checks)
        for n in q.namelist():
            if n.startswith('DrawParam/'):
                p=ROOT/'PARAM_INPUT'/n
                check(p.exists() and p.read_bytes()==q.read(n),f'artifact267_extracted_{Path(n).name}',sha(p) if p.exists() else 'missing',checks)
    ua=json.loads((ROOT/'data/UL48_LIVE_PAYLOAD_AUDIT.json').read_text())
    check(ua.get('status')=='PASS' and ua.get('host_count')==48 and ua.get('hemenv')==24 and ua.get('hemenvlerp')==24,'ul48_host_count','48=24+24',checks)
    check(ua.get('rdef_stripped')==48 and ua.get('b13_declared')==48 and ua.get('p22_exact_base')==48 and ua.get('local_ul_island_only')==48,'ul48_payload_invariants','RDEF 48; b13 48; P2.2 base 48; local U/L only 48',checks)
    recs=ua['records'];check(len({r['ul_sha256'] for r in recs})==48,'ul48_unique_payloads','48 unique hashes',checks)
    check(all(sorted(r['b13_refs'])==[6,7,7] for r in recs),'ul48_b13_refs','all exactly [6,7,7]',checks)
    src=(ROOT/'src/addon/addon.cpp').read_text(); asm=(ROOT/'src/addon/selector_hook.asm').read_text(); ps=(ROOT/'scripts/build_windows_real_ptde_ul.ps1').read_text()
    required=['RVA_WRAPPER_TYPE5','RVA_WRAPPER_TYPE6','RVA_BLEND_HELPER','RVA_SINGLE_HELPER','RVA_SELECTOR','RET_SINGLE_UPPER','RET_SINGLE_LOWER','RET_BLEND_UPPER','RET_BLEND_LOWER','desc+0x4C','TUPLE_MISMATCH']
    check(all(x in src for x in required),'runtime_source_contract_markers',','.join(required),checks)
    check('b13[6]=Upper_PTDE' not in src and 's->payload[6]=p.upper' in src and 's->payload[7]=p.lower' in src,'runtime_payload_slots','payload[6]=Upper payload[7]=Lower',checks)
    check('ENVSPEC=0' in src and 'g_envs' not in src and 'PSSetShaderResources' not in src and 'PSSetSamplers' not in src and 'b12' not in src,'envspec_excluded_from_runtime','no b12/t12/t14/sampler EnvSpec runtime lane',checks)
    check('DrawIndexed(index_count,0,0)' in src and 'DrawIndexedInstanced(index_count,native_instances,0,0,0)' in src,'exact_draw_kind_replay','DrawIndexed + DrawIndexedInstanced',checks)
    check('owner+0x24BC' in src,'draw_kind_owner_carrier','owner+0x24BC',checks)
    check('PSSetConstantBuffers(13,1,&owned)' in src and 'PSSetConstantBuffers1' in src,'b13_full_bind_restore','full owned bind + range-aware restore',checks)
    check('mov rcx, qword ptr [rsp+0A8h]' in asm and 'mov rdx, qword ptr [rsp+0C8h]' in asm and 'mov r8, r14' in asm and 'mov r9, r15' in asm,'selector_thunk_capture','owner/ret/R14/R15 captured',checks)
    check('/NOIMPLIB' in ps and '/NOEXP' in ps and '/INCREMENTAL:NO' in ps and 'REAL_PTDE_UL_LIVE' in ps and 'ml64.exe' in ps,'windows_build_pattern','artifact265 short-stage/direct-MSVC + ml64',checks)
    check('static_cast<float>(v.r)/255.0f*s' in src and 'static_cast<float>(v.m)/100.0f' in src,'ptde_linear_rgbm_decode','RGB/255 * M/100',checks)
    check('lerp4(decode_rgbm(ra),decode_rgbm(rb),beta)' in src,'ptde_linear_endpoint_blend','A + beta(B-A)',checks)
    forbidden=['pow(','1.5f','ENVSPEC_RAW','synthetic red','synthetic blue']
    check(all(x not in src for x in forbidden),'no_dsr_ul_inverse_or_synthetic_payload',str(forbidden),checks)
    ok=all(c['pass'] for c in checks); out={'status':'PASS' if ok else 'FAIL','classification':'CONSTRUCTION_ONLY_REAL_PTDE_UL_LIVE_SUCCESSOR','runtime_validation':'OPEN','final_pixel_fidelity':'OPEN','artifact267_sha256':A267,'checks_pass':sum(c['pass'] for c in checks),'checks_total':len(checks),'checks':checks,'source_sha256':{'addon.cpp':sha(ROOT/'src/addon/addon.cpp'),'selector_hook.asm':sha(ROOT/'src/addon/selector_hook.asm'),'generated_ul48_live.hpp':sha(ROOT/'include/dsrrl/generated_ul48_live.hpp'),'build_windows_real_ptde_ul.ps1':sha(ROOT/'scripts/build_windows_real_ptde_ul.ps1')}}
    (ROOT/'STATIC_AUDIT.json').write_text(json.dumps(out,indent=2)+'\n');print(out['status'],out['checks_pass'],'/',out['checks_total']);sys.exit(0 if ok else 1)
if __name__=='__main__':main()
