from pathlib import Path
p=Path(__file__).resolve().parents[1]
source=(p/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
gpu=(p/'integrated/integrated_addon.cpp').read_text()
journal=(p/'include/dsrrl/runtime/v411_owner_sequence_journal.hpp').read_text()
trace=(p/'include/dsrrl/runtime/v411_rx34_owner_draw_trace.hpp').read_text()
build=(p/'tools/build_v230_production.ps1').read_text()
s=source[source.index('void pmetal_env_source_selector_event('):source.index('bool pmetal_env_source_runtime::latest(')]
assert 'DSRRL_V411_OWNER_SEQUENCE_DIAG' in s
assert 'pmetal_producer_state_publish(\n            material,' in s
assert s.index('pmetal_producer_state_publish(\n            material,')<s.index('[DSRRL V411 OWNER SEQ STAMP]')
b=s[s.index('// V411: after actual actor-scoped publication'):s.index('#if defined(DSRRL_V408_CONTEXT_IDENTITY_DIAG)',s.index('// V411: after actual actor-scoped publication'))]
for x in ('committed_v411.bank_signature_a','committed_v411.bank_signature_b',
 'committed_v411.row_id_a','committed_v411.row_id_b',
 'std::memcmp(&committed_v411.beta,&next.beta',
 'committed_v411.envdiffuse_a','committed_v411.envdiffuse_b',
 'w.wrapper=','w.manager=','w.native_sequence=',
 'owner_bytes_v411+0x2320u','manager_field_v411==manager'):
 assert x in b,x
assert 'return;' not in b and 'latest_hook_source' not in b
assert 'if (latest_hook_source(out))' in source
r=gpu[gpu.index('void v388_envdiffuse_stable_issued('):gpu.index('bool prepare_island_batch(')]
assert r.index('v411::indexed_rx34(material,decision,prepared)')<r.index('const bool initial=nth<=32u')
assert 'if(issued && !hemenvlerp_bound)' in gpu
assert 'native_seq==0 || s.value.native_sequence!=native_seq' in journal or 'w.native_sequence!=seq' in journal
assert 'selector_epoch==0 || s.value.selector_epoch!=selector_epoch' in journal or 'w.selector_epoch!=epoch' in journal
assert 's.had_pointer_reassignment=true' in journal or 'e.had_pointer_reassignment=true' in journal
assert 'capacity_failures_.fetch_add' in journal
assert 'v409::source_transition_tracker' in trace
assert 'pmetal_producer_state_latest(' in trace
assert 'd.receiver_id!=34u' in trace
assert 'native_actor_call=OPEN command_order=OPEN pixel=OPEN' in trace
assert 'cache_seq_matches=%u' in trace
assert '/DDSRRL_V411_OWNER_SEQUENCE_DIAG' in build
assert '[DSRRL V411 OWNER SEQ STAMP]' in build
assert '[DSRRL V411 RX34 OWNER SEQ DRAW]' in build
print('V411 SOURCE CONTRACT PASS: gated read-only journal + indexed issued RX34, R43 intact')
