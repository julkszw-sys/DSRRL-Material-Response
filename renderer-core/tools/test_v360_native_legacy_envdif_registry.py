from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=(root/'src/runtime/envspec_resource_runtime.cpp').read_text('utf8')
h=(root/'include/dsrrl/operators/env_spec/legacy_envdiffuse_verified_identity_v360.hpp').read_text('utf8')
d=(root/'tools/materialize_v230_envdiffuse_cinematic_trace.py').read_text('utf8')
b=(root/'tools/build_v230_envdiffuse_cinematic_trace.ps1').read_text('utf8')
assert h.count('EnvDif_m18_500')==1 and h.count('EnvDif_m18_510')==1
assert 'exact_name(name,count,entry.name)' in h and 'exact_digest(digest,entry.sha256_hex)' in h
for x in ('g_legacy_diffuse_by_resource','g_legacy_diffuse_resource_by_view','g_legacy_diffuse_by_resource.erase','g_legacy_diffuse_resource_by_view.erase','g_legacy_diffuse_by_resource.clear','g_legacy_diffuse_resource_by_view.clear'):
    assert x in p,x
assert 'native_legacy_envdiffuse_for_view(' in p
assert 'ptde_donor=UNMAPPED t12=REJECTED' in d
assert '[DSRRL V360 LEGACY ENVDIF ROUTED]' in d
assert 'DSRRL_V359_LEGACY_ENVDIF_ID_REGISTRY' in b
print('V360_NATIVE_LEGACY_ID_REGISTRY_SOURCE_PASS runtime_pixels_OPEN')
