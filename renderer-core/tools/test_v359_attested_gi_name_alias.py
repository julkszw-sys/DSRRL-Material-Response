from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'src/runtime/envspec_resource_runtime.cpp').read_text('utf8')
h=(r/'include/dsrrl/operators/env_spec/generated_gi_name_binder_v359.hpp').read_text('utf8')
for v in ('v359_verified_native_name_probe_locked(', 'v359_match_original_gi_name(', 'g_v359_map_conflict', 'g_v359_seen_count[r.source_map]<2u', 'texture_identity_transport::snapshot_raw(', '[DSRRL V359 GI NAME RECOVERED]'):
 assert v in s,v
assert h.count('    {L"GI_EnvSpc_')==342
for marker in ('gi_view_exact', 'srv->GetResource(&backing)', 'srv->GetDevice(&device)', 'g_v359_map_conflict[w.source_map]', 'v359_match_original_gi_name('):
 assert marker in s,marker
print('V359_EXACT_SOURCE_NAME_MAP_CONTRACT_PASS')
