from pathlib import Path
p=Path(__file__).resolve().parents[1]
s=(p/'include/dsrrl/runtime/v411_owner_sequence_journal.hpp').read_text()
draw=(p/'include/dsrrl/runtime/v411_rx34_owner_draw_trace.hpp').read_text()
native=(p/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
integrated=(p/'integrated/integrated_addon.cpp').read_text()
for term in ('history_depth=8','std::unique_ptr<entry[]>',
    'new (std::nothrow)','s.value.native_sequence',
    'status::ambiguous_context_identity','status::history_evicted',
    'r.superseded_by_newer','same_source_tuple','same_witness_semantics',
    'history_overwrites_.fetch_add'):
    if term=='s.value.native_sequence':
        assert term not in s
    else:assert term in s,term
assert 'superseded_by_newer?1u:0u' in draw
assert 'if(same_witness_semantics(latest,w))return true;' in s
assert 'history_has_evictions?1u:0u' in draw
assert 'history_overwrites()' in draw
assert 'd.receiver_id!=34u' in draw
assert 'if(issued && !hemenvlerp_bound)' in integrated
assert 'if (latest_hook_source(out))' in native
assert 'DSRRL_V411_OWNER_SEQUENCE_DIAG' in native
print('V412 SOURCE CONTRACT PASS: bounded history, source tuple conflict, fail-open, R43 unchanged')
