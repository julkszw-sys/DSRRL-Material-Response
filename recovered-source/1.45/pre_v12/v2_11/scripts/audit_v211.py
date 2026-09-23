from pathlib import Path
import re,json,hashlib,struct,sys
ROOT=Path(__file__).resolve().parents[1]
FULL=ROOT/'include/dsrrl/generated_ptde_c101_material_response.hpp'
DIFF=ROOT/'include/dsrrl/generated_ptde_diffuse_material_response.hpp'
REG=ROOT/'include/dsrrl/ptde_material_donor_registry.hpp'
ADDON=ROOT/'src/addon/addon.cpp'
A=json.loads((ROOT/'audit/V211_C101_UNBOUNDED_AUDIT.json').read_text())
checks={}
def ck(k,v): checks[k]=bool(v); assert v,k

ck('construction_audit_pass',A['status']=='PASS')
ck('host_count_24',A['host_count']==24 and len(A['hosts'])==24)
ck('v210_c100_baseline_unchanged',A['v210_c100_baseline_unchanged'] is True)
ck('v210_c101_formula_unchanged',A['v210_c101_sidecar_formula_unchanged'] is True)
ck('stock_spec_pow_preserved',A['stock_spec_pow_2_2_preserved'] is True)
ck('stock_downstream_envspec_preserved',A['stock_downstream_pbl_envspec_preserved'] is True)
ck('hemenvlerp_stock',A['hemenvlerp_replacements']==0)
ck('pointlight_unchanged',A['pointlight_changes'] is False)

def parse_blobs(path):
    s=path.read_text();out={}
    for m in re.finditer(r'inline constexpr std::array<std::uint8_t,(\d+)> blob_(\d+) = \{(.*?)\n\};',s,re.S):
        n=int(m.group(1));i=int(m.group(2))
        vals=bytes(int(x) for x in re.findall(r'\b\d+\b',m.group(3)))
        assert len(vals)==n
        out[i]=vals
    return out

def chunks(blob):
    assert blob[:4]==b'DXBC'
    count=struct.unpack_from('<I',blob,28)[0]
    offs=struct.unpack_from('<'+'I'*count,blob,32)
    out={}
    for o in offs:
        tag=blob[o:o+4].decode('ascii')
        sz=struct.unpack_from('<I',blob,o+4)[0]
        out[tag]=(o,blob[o+8:o+8+sz])
    return out

full=parse_blobs(FULL); diff=parse_blobs(DIFF)
ck('blob_counts',len(full)==24 and len(diff)==24)
for h in A['hosts']:
    i=h['index']; b=full[i]
    ck(f'h{i}_sha',hashlib.sha256(b).hexdigest()==h['v211_sha256'])
    ck(f'h{i}_size',len(b)==h['size']==len(diff[i]))
    sh=chunks(b)['SHEX']; words=struct.unpack('<'+'I'*(len(sh[1])//4),sh[1])
    tok=words[h['chain_mul_word']]
    ck(f'h{i}_mul_not_sat',tok==0x08000038)
    ck(f'h{i}_sat_bit_clear',(tok & 0x2000)==0)
    ck(f'h{i}_same_opcode',(tok & 0x7ff)==0x38)

addon=ADDON.read_text()
ck('addon_version','V2.11' in addon and 'V2.10' not in addon)
ck('addon_still_uses_f0q','d.c101_f0q[0]' in addon)
ck('addon_no_pointlight_srv','PSSetShaderResources' not in addon)
ck('addon_hemenvlerp_bypass','host_plan.lerp' in addon)
ck('addon_description_unbounded','MUL_SAT saturation modifier is removed' in addon)
reg=REG.read_text()
ck('registry_unchanged_shape','c101_f0q[3]' in reg and 'c102;' in reg)
ck('reachability_audit_present',(ROOT/'audit/DSRRL_C101_NATIVE_SAT_REACHABILITY_AUDIT_V1.json').exists())

res={'schema':'DSRRL_V2_11_STATIC_AUDIT','status':'PASS','check_count':len(checks),'checks':checks,
     'c101_header_sha256':hashlib.sha256(FULL.read_bytes()).hexdigest(),
     'diffuse_header_sha256':hashlib.sha256(DIFF.read_bytes()).hexdigest(),
     'registry_sha256':hashlib.sha256(REG.read_bytes()).hexdigest(),
     'addon_sha256':hashlib.sha256(ADDON.read_bytes()).hexdigest()}
(ROOT/'audit/V211_STATIC_AUDIT.json').write_text(json.dumps(res,indent=2)+'\n')
print('PASS V2.11 static audit',len(checks),'checks')
