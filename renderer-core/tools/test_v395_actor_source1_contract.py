#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1]
f=(r/'src/runtime/flver_engine_hooks.cpp').read_text()
c=(r/'src/runtime/pmetal_producer_state.cpp').read_text()
s=(r/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
t=(r/'tests/pmetal_producer_state_tests.cpp').read_text()
m=(r/'src/runtime/material_owner_selection.cpp').read_text()
h=f[f.index('void observe_model_lightbank_source('):f.index('bool publish_exact_selector_identity(')]
assert 'model.model_owner_instance' in h
for forbidden in ('flver_sha256','raw_mtd','material_slot','route_index'):
    assert forbidden not in h, forbidden
selector=f[f.index('void flver_selector'):] if 'void flver_selector' in f else f
early=f.index('observe_model_lightbank_source(\n     owner,ret,r14,r15,selector_stack);')
lookup=f.index('selector_identity_cache_lookup(',early)
assert early < lookup
assert f.count('observe_model_lightbank_source(')==2
key=c[c.index('std::uint64_t model_key('):c.index('std::uint64_t source_key(')]
same=c[c.index('bool same_model('):c.index('bool same_source_payload(')]
for x in ('flver_sha256','material_slot','raw_mtd_sha256','route_index','semantic_name_hash'):
    assert x not in key and x not in same, x
assert 'model_owner_instance' in key and 'model_owner_instance' in same
assert 'if (key == 0u) return false;' in c
assert 'pre_parser.flver_sha256 = {};' in t
assert 'swapped_model.flver_sha256[0] ^= 0x01u;' in t
producer=s[s.index('void pmetal_env_source_selector_event('):s.index('bool pmetal_env_source_runtime::latest(')]
latest=s[s.index('bool pmetal_env_source_runtime::latest('):]
assert 'material.flver_sha256 == core::sha256_digest{}' not in producer
assert 'material.flver_sha256 == core::sha256_digest{}' not in latest
assert 'descriptor_owner != owner' in producer
assert 'parent_return !=' in producer
assert 'is_synthetic_zero_descriptor(' in producer
assert producer.index('descriptor_owner != owner') < producer.index('pmetal_producer_state_begin(')
assert latest.index('pmetal_producer_state_latest(') < latest.index('latest_hook_source(out)')
assert 'owner_tuple_authenticated_cached(' in m
draw=(r/'src/runtime/pmetal_envspec_draw_runtime.cpp').read_text()
assert 'exact_pmetal_envspec_candidate(' in draw
assert 'source.consumer_source_kind' in draw
print('V395 actor Source1 pre-parser / cross-FLVER / downstream material isolation PASS')
