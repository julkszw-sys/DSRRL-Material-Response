from pathlib import Path
p=Path(__file__).resolve().parents[1]
s=(p/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
begin=s.index('[DSRRL V408 SOURCE1 PACKER JOIN]')
seg=s[s.rfind('const bool same_producer =',0,begin):begin]
for wanted in ('published &&','next.native_producer_sequence != 0u',
 'committed.native_producer_sequence','committed.bank_signature_a',
 'committed.bank_signature_b','committed.row_id_a','committed.row_id_b',
 'std::memcmp(&committed.beta,&next.beta','committed.a == next.a',
 'committed.b == next.b','committed.envdiffuse_a == next.envdiffuse_a',
 'committed.envdiffuse_b == next.envdiffuse_b'):
    assert wanted in seg,wanted
assert 'pmetal_producer_state_publish(' not in seg
b=(p/'tools/build_v230_production.ps1').read_text()
assert '/DDSRRL_V408_CONTEXT_IDENTITY_DIAG' in b
assert '/DDSRRL_V409_RX34_TRANSITION_DIAG' in b
print('V410 READBACK CONTRACT PASS: both native endpoint payloads and beta bits; no source write')
