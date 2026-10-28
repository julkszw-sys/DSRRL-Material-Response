from pathlib import Path
p=Path(__file__).resolve().parents[1]
journal=(p/'include/dsrrl/runtime/v411_owner_sequence_journal.hpp').read_text()
probe=(p/'include/dsrrl/runtime/v413_live_owner_probe.hpp').read_text()
draw=(p/'include/dsrrl/runtime/v411_rx34_owner_draw_trace.hpp').read_text()
integrated=(p/'integrated/integrated_addon.cpp').read_text()
source=(p/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
for x in ('context_runs','matched_context_run','latest_context_run',
          'matching_run!=e.current_context_run',
          'status::ambiguous_context_identity','history_depth=8'):
    assert x in journal,x
for x in ('ReadProcessMemory','GetCurrentProcess','owner+0x2318u',
          'owner+0x2320u','wrapper+0x40u','r.manager_agrees='):
    if x=='wrapper+0x40u':assert 'r.wrapper+0x40u' in probe
    else:assert x in probe,x
assert 'v413::inspect_owner(m.model_owner_instance)' in draw
assert 'live.wrapper,live.manager' in draw
assert 'status::context_unreadable' in draw
assert 'status::native_manager_disagreement' in draw
assert 'joined.prior.context_vtable!=live.vtable' in draw
assert 'matched_run=%llu latest_run=%llu' in draw
assert 'if(issued && !hemenvlerp_bound)' in integrated
assert 'if (latest_hook_source(out))' in source
assert 'pmetal_producer_state_publish(' in source
assert 'pmetal_producer_state_publish(' not in draw
print('V413 SOURCE CONTRACT PASS: owner live tuple + separate context runs; R43 unchanged')
