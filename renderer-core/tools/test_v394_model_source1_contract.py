#!/usr/bin/env python3
"""V394 exact model-instance Source1 vs downstream material policy source contract."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
i=(root/'include/dsrrl/operators/material_response/material_response_island.hpp').read_text()
f=(root/'src/runtime/flver_engine_hooks.cpp').read_text()
s=(root/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
c=(root/'src/runtime/pmetal_producer_state.cpp').read_text()
m=(root/'src/runtime/material_owner_selection.cpp').read_text()
p=(root/'include/dsrrl/runtime/pmetal_selector_policy.hpp').read_text()
assert 'std::uintptr_t model_owner_instance = 0u' in i
producer=f[f.index('void observe_model_lightbank_source('):f.index('bool publish_exact_selector_identity(')]
assert 'model.model_owner_instance' in producer and 'model.flver_sha256' in producer
assert 'material_slot' not in producer and 'raw_mtd' not in producer
assert 'material_owner_selection_publish(' in f
assert 'observe_model_lightbank_source(' in f
assert f.index('observe_model_lightbank_source(',f.index('if(owner_lookup_ok){')) < f.index('enrich_exact_owner_mtd_identity(',f.index('if(owner_lookup_ok){'))
assert f.index('observe_model_lightbank_source(',f.index('if(cache_hit){')) < f.index('publish_exact_selector_identity(',f.index('if(cache_hit){'))
assert 'model_identity.model_owner_instance' in f and 'material_owner_selection_publish(\n            model_identity)' in f
sel=s[s.index('void pmetal_env_source_selector_event('):s.index('bool pmetal_env_source_runtime::latest(')]
assert 'is_synthetic_zero_descriptor(' in sel
assert sel.index('is_synthetic_zero_descriptor(') < sel.index('pmetal_producer_state_begin(')
assert '!exact_v285_or_pmetal_source(' not in sel
latest=s[s.index('bool pmetal_env_source_runtime::latest('):]
assert '!exact_v285_or_pmetal_source(' not in latest
assert 'pmetal_producer_state_latest(' in latest and 'latest_hook_source(out)' in latest
assert latest.index('pmetal_producer_state_latest(')<latest.index('latest_hook_source(out)')
key=c[c.index('std::uint64_t model_key('):c.index('std::uint64_t source_key(')]
for absent in ['material_slot','raw_mtd_sha256','route_index','semantic_name_hash','material_family_hash']:
    assert absent not in key,absent
for present in ['model_owner_instance','flver_sha256']:
    assert present in key,present
assert 'same_model(' in c and 'same_material(' not in c
assert 'same_model(g_record.material, material)' in c
assert 'same_model(\n            slot.record.material,\n            material)' in c
assert 'is_synthetic_zero_descriptor(' in p
# MTD and receiver gates are preserved independently at draw stage.
assert 'owner_tuple_authenticated_cached(' in m
draw=(root/'src/runtime/pmetal_envspec_draw_runtime.cpp').read_text()
assert 'exact_pmetal_envspec_candidate(' in draw
assert 'exact_pmetal_material_selection' in s
assert 'source.consumer_source_kind' in draw
print('V394 MODEL-WIDE SOURCE1 / OPERATOR-LOCAL MTD CONTRACT PASS')
