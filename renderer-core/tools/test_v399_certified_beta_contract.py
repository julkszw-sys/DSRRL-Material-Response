from pathlib import Path
root=Path('renderer-core')
s=(root/'src/runtime/pmetal_producer_state.cpp').read_text()
build=(root/'tools/build_v230_production.ps1').read_text()
runtime=(root/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
assert '/DDSRRL_V399_CERTIFIED_ENDPOINT_BETA_CONTINUITY' in build
assert 'DSRRL BUILD_ID V399' in runtime
assert '#if defined(DSRRL_V399_CERTIFIED_ENDPOINT_BETA_CONTINUITY)' in s
part=s[s.index('void pmetal_producer_state_begin(\n    const operators::material_response::material_identity &material,\n    std::uint64_t epoch,'):]
start=part.index('const bool reuse_certified_endpoints')
end=part.index('slot.valid.store(false, std::memory_order_release)',start)
section=part[start:end]
for expected in ('endpoints_same', 'slot.record.valid','same_model(', 'slot.record.source.row_id_a',
                 'slot.record.source.row_id_b','slot.record.source.beta = beta',
                 'slot.selection = next','++slot.selection_revision','return;'):
    assert expected in section,expected
assert 'slot.valid.store(false, std::memory_order_release)' not in section
assert 'std::lock_guard<std::mutex> lock(slot.mutex)' in part[:start]
assert 'latest_hook_source(out)' in runtime
assert 'pmetal_producer_state_actor_tracked(material, epoch)' in runtime
assert 'is_synthetic_zero_descriptor(' in runtime
print('V399 beta-only Source1 endpoint continuity source-contract PASS')
