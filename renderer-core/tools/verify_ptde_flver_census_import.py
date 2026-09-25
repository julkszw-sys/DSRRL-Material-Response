#!/usr/bin/env python3
import json
import pathlib

root=pathlib.Path(__file__).resolve().parents[1]
d=json.loads((root/'data/census/ptde_flver_census_import_v1.json').read_text(encoding='utf-8'))
assert d['schema']==1
assert d['game']=='PTDE'
assert d['runtime_activation']=='OPEN'
assert d['pixel_equivalence']=='OPEN'
c=d['counts']
assert c['canonical_rows']==87151
assert c['flver_instances']==4998
assert c['flver_identities']==4757
assert c['unique_flver_payloads']==4688
assert c['material_instances']==22063
assert c['unresolved_rows']==0
assert d['mtd_resolution']=={'EXACT_BASENAME_SHA':87151}
s=d['texture_semantics']
assert sum(s.values())==87151
assert set(s)=={'g_Diffuse','g_DetailBumpmap','g_Bumpmap','g_Specular','g_Lightmap','g_Diffuse_2','g_Bumpmap_2','g_Specular_2'}
p=d['exact_mtd_identity_projection']
assert p['identities_observed_in_flver_rows']==261
assert p['diffuse_use_identities']==239
assert p['normal_bump_use_identities']==240
assert p['specular_resource_use_identities']==153
assert p['lightmap_resource_use_identities']==60
assert 'never becomes NO_USE' in p['classification_rule']
assert d['policies']['missing_or_ambiguous_mtd']=='UNRESOLVED_NOT_NO_USE'
assert d['policies']['missing_texture_semantic']=='UNKNOWN_FAIL_OPEN'
print('ptde_flver_census_import_v1: PASS')
