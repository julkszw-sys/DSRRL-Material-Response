#!/usr/bin/env python3
"""V418 issued RX34 model/registry identity; strictly observational source contract."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sel=(root/'src/runtime/flver_engine_hooks.cpp').read_text('utf-8')
producer=(root/'src/runtime/pmetal_env_source_runtime.cpp').read_text('utf-8')
journal=(root/'include/dsrrl/runtime/v411_owner_sequence_journal.hpp').read_text('utf-8')
issued=(root/'include/dsrrl/runtime/v411_rx34_owner_draw_trace.hpp').read_text('utf-8')
stamp=(root/'include/dsrrl/runtime/v415_model_draw_identity.hpp').read_text('utf-8')
build=(root/'tools/build_v418_rx34_generational_diagnostic.ps1').read_text('utf-8')
workflow=(root.parent/'.github/workflows/v418-rx34-generational-issued-join.yml').read_text('utf-8')

assert 'DSRRL_V415_FLVER_MODEL_ID_DIAG' in build
assert 'DSRRL_V416_FGMODEL_PARENT_DIAG' in build
assert 'DSRRL_V417_FGMODEL_REGISTRY_DIAG' in build
assert 'DSRRL_V418_RX34_GENERATIONAL_ISSUED_JOIN' in build
assert 'v418_rx34_model_journal_tests.cpp' in workflow
assert 'test_v418_issued_model_journal_contract.py' in workflow
assert 'DSRRL_V418_RX34_Generational_Issued_Diagnostic.addon64' in workflow
assert 'fg_registry_readable=registry_readable' in stamp
assert 'model_addr,fg_registry_id,fg_registry_readable,' in sel
assert 'w.registry_readable=' in producer
assert 'w.registry_allocated=w.registry_readable &&' in producer
assert 'model_stamp_v418.fg_parent_reciprocal' in producer
assert 'w.model_this=model_stamp_v418.model_this' in producer
assert 'struct witness' in journal and 'fg_registry_id=0u;' in journal
assert 'same_model_tuple' in journal
assert '|| !same_model_tuple(*matching,w)' in journal
assert 'same_model_tuple(a,b)' in journal
assert 'joined.state==status::matched_snapshot' in issued
assert 'joined.candidates==1u' in issued
assert 'joined.prior.model_verified' in issued
assert 'joined.prior.fg_reciprocal' in issued
assert 'joined.prior.registry_readable' in issued
assert 'joined.prior.registry_allocated' in issued
assert 'cache_seq_matches' in issued
assert 'joined.prior.row_a==p.stable_envdiffuse_row' in issued
assert '!joined.superseded_by_newer' in issued
assert 'actor_id=OPEN gpu_cb=OPEN' in issued
assert 'rtv=OPEN visible_pixel=OPEN' in issued
assert '[DSRRL V418 RX34 MODEL ISSUED JOIN]' in issued
# Only the V411 diagnostic producer journal changes; no policy or R43 changes.
assert 'latest_hook_source_exact_selector(' in producer
assert 'if (latest_hook_source(out))' in producer
assert 'pmetal_producer_state_publish(' in producer
print('V418 exact model+parent+generation issued draw diagnostic SOURCE CONTRACT PASS')
