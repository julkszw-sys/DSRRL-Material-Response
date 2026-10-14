#!/usr/bin/env python3
"""V274D Cloth/Leather MTD and R19 operator-local beta sweep. Not a pixel simulator."""
import csv,json,struct,math,re
from pathlib import Path
from collections import Counter,defaultdict
ROOT=Path(__file__).resolve().parents[3]
C=ROOT/'renderer-core/data/census'
A=json.loads((C/'ptde_mtd_envspec_router_v1.json').read_text())['records']
B=json.loads((ROOT/'renderer-core/data/material_response_routes_v1.json').read_text())['routes']
X=json.loads((C/'ptde_mtd_semantic_exact_bindings_v1.json').read_text())['records']
rows=[x for x in A if re.search('cloth|leather|fabric',x['mtd_name'],re.I)]
direct={x['sha256']:x for x in B}
extension={x['raw_sha256']:x for x in X}
groups=defaultdict(list)
for r in rows:groups[r['dsr_mtd_sha256']].append(r)
def num(s):return struct.unpack('<f',struct.pack('<I',int(s,16)))[0]
def sweep(c101):
    tests=0;overshoot=0.;curvature=0.
    for tex in (0.,.25,.5,.75,1.):
      for vertex in (.25,.75,1.):
       for a,b in ((0.,1.),(.25,.75),(1.,.2)):
        def val(beta):return min(1.,max(0.,c101*tex*vertex*(a*(1-beta)+b*beta)))
        left,right=val(0),val(1)
        for j in range(31):
          beta=j/30;v=val(beta);tests+=1
          overshoot=max(overshoot,v-max(left,right))
          curvature=max(curvature,abs(v-(left*(1-beta)+right*beta)))
    return tests,overshoot,curvature
out=[]
for r in rows:
 sha=r['dsr_mtd_sha256'];base=direct.get(sha);extra=extension.get(sha)
 c101=num(r['c101_f32_bits'][0]);c102=num(r['c102_f32_bits'])
 flags=[]
 if len(set((x['mtd_name'],x['c101_f32_bits'][0],x['c102_f32_bits']) for x in groups[sha]))>1:flags.append('MTD_SHA_SEMANTIC_COLLISION')
 if r['dsr_spx'].lower()!=r['ptde_spx'].lower():flags.append('PTDE_DSR_SPX_DIFFERENCE')
 if 'SpcBmpLit' in r['dsr_spx']:flags.append('LIT_OUTSIDE_R19_RX33_35')
 if base is None and extra is None:flags.append('NO_DIRECT_OR_EXTENSION_PROFILE')
 if extra and extra['gate_policy']=='PTDE_COMPANION_REQUIRED':flags.append('PTDE_COMPANION_MANDATORY')
 if sum(x['mtd_name']==r['mtd_name'] for x in rows)>1:flags.append('SEMANTIC_NAME_MULTI_SHA')
 if c101>2:flags.append('HIGH_PTDE_AUTHORED_C101_NOT_PROOF_OF_BUG')
 n,over,curve=sweep(c101)
 out.append(dict(name=r['mtd_name'],sha=sha,c100=','.join(str(num(t)) for t in r['c100_f32_bits']),c101=c101,c102=c102,env_slot=r['envspc_slot'],host_spx=r['dsr_spx'].split(chr(92))[-1],ptde_spx=r['ptde_spx'].split(chr(92))[-1],base_route=base['route_index'] if base else '',extension=extra['bridge_binding_id'] if extra else '',gate=extra['gate_policy'] if extra else '',flags=';'.join(flags) or 'SOURCE_ROUTING_NO_ALERT',synthetic_cases=n,local_lerp_max_overshoot=over,local_sat_chord_curvature=round(curve,7),angular_p08_canary=round(.8**c102,8),saturation_threshold_tex_if_full_probe=round(1/c101,7) if c101 else ''))
assert len(out)==27 and sum(bool(x['base_route']!='') for x in out)==15
assert sum('MTD_SHA_SEMANTIC_COLLISION' in x['flags'] for x in out)==4
assert sum('PTDE_DSR_SPX_DIFFERENCE' in x['flags'] for x in out)==1
assert all(x['local_lerp_max_overshoot']==0 for x in out)
dest=ROOT/'renderer-core/tools/v274d/out';dest.mkdir(parents=True,exist_ok=True)
with (dest/'cloth_leather_27_matrix.csv').open('w',newline='',encoding='utf8') as f:
 w=csv.DictWriter(f,fieldnames=list(out[0]));w.writeheader();w.writerows(out)
report={'base':'V272 ae9ac314d06ad8758e1aa7430cb236eef0eefe27','scope':'27 canonical Cloth/Leather MTD records; 31 beta steps; 45 controlled synthetic texel/vertex/probe conditions per material','records':len(out),'registered_base':sum(x['base_route']!='' for x in out),'registered_extension':sum(x['extension']!='' for x in out),'flags':dict(Counter(y for x in out for y in x['flags'].split(';'))),'cases':sum(x['synthetic_cases'] for x in out),'max_overshoot_from_isolated_documented_R19_subcut':max(x['local_lerp_max_overshoot'] for x in out),'pixel':'OPEN: no actual t1/t10 texels, GPU CB, normal or PTDE pixel','interpretation':'Isolated known local SpecRGB*c101*COLOR0*linear(EnvSpecA,EnvSpecB,beta), even with isolated SAT, cannot exceed its two endpoint values for a fixed texel/material. This does not simulate entire PTDE or DSR shader nor exclude mixed-domain downstream peaks.'}
(dest/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
