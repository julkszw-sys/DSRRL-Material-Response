from pathlib import Path
p=Path('renderer-core')
s=(p/'src/runtime/pmetal_producer_state.cpp').read_text()
r=(p/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
build=(p/'tools/build_v230_production.ps1').read_text()
tests=(p/'tests/v398_actor_selector_transaction_tests.cpp').read_text()
assert '/DDSRRL_V398_ACTOR_TRANSACTIONAL_SOURCE' in build
assert 'slot.selection_revision' in s and 'g_pending' in s
assert 'slot.selection.a == next.a' in s
assert 'slot.selection.beta_bits == next.beta_bits' in s
assert 'slot.selection_revision != ticket.revision' in s
assert 'pmetal_producer_state_actor_tracked' in s
assert '#if !defined(DSRRL_V398_ACTOR_TRANSACTIONAL_SOURCE)' in s
assert 'pmetal_producer_state_begin(\n        material, epoch, source_a' in r
assert 'pmetal_producer_state_actor_tracked(material, epoch)' in r
assert r.index('pmetal_producer_state_actor_tracked(material, epoch)') < r.index('if (latest_hook_source(out))')
assert 'latest_hook_source(out)' in r
assert 'is_synthetic_zero_descriptor(' in r
assert 'return false;' in r[r.index('pmetal_producer_state_actor_tracked(material, epoch)'):r.index('if (latest_hook_source(out))')]
for x in ('std::thread old_writer','row_id_a=0u','swapped','1311','1283'):
    if x=='swapped': continue
    assert x in tests, x
print('V398 actor-coherent selector contract PASS')
