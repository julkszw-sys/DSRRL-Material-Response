#!/usr/bin/env python3
"""Policy verifier for PTDE/DSR MTD semantic census.
Construction/static-routing only; never promotes runtime or pixel equivalence.
"""
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
p=ROOT/'data/census/ptde_mtd_semantic_census_v1.json'
d=json.loads(p.read_text(encoding='utf-8'))
assert d['identity_policy'].startswith('exact semantic MTD identity'), 'exact identity policy drift'
assert d['claim_scope']=='CONSTRUCTION_AND_STATIC_ROUTING_ONLY'
assert d['runtime_activation']=='OPEN' and d['pixel_equivalence']=='OPEN'
assert set(d['states'])=={'USE','NO_USE','UNKNOWN'}
cols=set(d['operator_columns'])
required={'material_response','spec_rgb','env_spec','subsurface','diffuse','normal_bump','upper_lower','hemenv','hemenv_lerp','pointlight','alpha_blend','parallax','emissive_lightmap','texture_resource_consumers'}
assert required<=cols, f'missing semantic columns: {sorted(required-cols)}'
assert any('Never infer NO_USE' in x for x in d['guardrails'])
assert any('UNKNOWN preserves stock DSR' in x for x in d['guardrails'])
for cohort in d.get('cohorts',[]):
    for op,v in cohort.get('classification',{}).items():
        assert v['state'] in d['states'], (cohort['key'],op)
        if v['state']=='NO_USE':
            basis=v.get('basis','').lower()
            assert basis and not ('name' in basis and 'receiver' not in basis and 'route' not in basis), (cohort['key'],op,'NO_USE lacks routed evidence')
for row in d.get('exact_overrides',[]):
    assert row.get('ptde_mtd_name') and len(row.get('ptde_mtd_sha256',''))==64, 'exact override must carry name+raw SHA'
    for op,v in row.get('operators',{}).items(): assert v['state'] in d['states']
print('MTD_SEMANTIC_CENSUS_POLICY_PASS exact_identity=true unknown=fail_open runtime=OPEN pixel=OPEN')
