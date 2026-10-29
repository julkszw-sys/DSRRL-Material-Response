#!/usr/bin/env python3
"""V419 exact RX34 Source1 native state diagnostic SOURCE contract.

Readback point must be between begin() / native DrawIndexed and restore().
No data from a restored post-transaction state is promoted as pre-draw state.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
header=(r/'include/dsrrl/runtime/v419_rx34_predraw_state.hpp').read_text()
tx=(r/'src/runtime/draw_state_transaction.cpp').read_text()
integrated=(r/'integrated/integrated_addon.cpp').read_text()
build=(r/'tools/build_v419_rx34_native_predraw_diagnostic.ps1').read_text()
workflow=(r.parent/'.github/workflows/v419-rx34-native-predraw-state.yml').read_text()
assert 'DSRRL_V419_RX34_PREDRAW_NATIVE_STATE' in build
assert 'DSRRL_V418_RX34_GENERATIONAL_ISSUED_JOIN' in build
assert 'DSRRL_V419_RX34_Native_PreDraw_Diagnostic.addon64' in build
assert 'build_v419_rx34_native_predraw_diagnostic.ps1' in workflow
assert 'test_v419_native_predraw_source_contract.py' in workflow
assert 'DSRRL_V419_RX34_Native_PreDraw_Diagnostic.addon64' in workflow
start=tx.index('draw_state_transaction_runtime::replay_draw_indexed(')
body=tx[start:tx.index('bool draw_state_transaction_runtime::mutate_restore_only(',start)]
assert body.index('if (!begin(cmd_list, mutation, state))')<body.index('v419::before_native_draw(ctx);')
assert body.index('v419::before_native_draw(ctx);')<body.index('ctx->DrawIndexed(')
assert body.index('ctx->DrawIndexed(')<body.index('const bool restored = restore(cmd_list, state);')
integrated_start=integrated.index('dispatch_island_draw_indexed_batch(')
before=integrated[integrated_start-3900:integrated_start]
after=integrated[integrated_start:integrated_start+1500]
for guard in (
  'prepared.envspec_in_batch','!hemenvlerp_bound',
  'decision.receiver_id==34u','prepared.envspec.source_kind==1u',
  'joined_v419.candidates==1u','w.model_verified',
  'w.fg_reciprocal','w.registry_readable','w.registry_allocated',
  'w.row_a==prepared.envspec.stable_envdiffuse_row',
  '!joined_v419.superseded_by_newer',
  'v419::arm(req);'):
 assert guard in before,guard
assert 'v419::finish(' in after
for op in ('PSGetShader(','PSGetConstantBuffers(','OMGetRenderTargets(',
           'IAGetIndexBuffer(','RSGetViewports('):
 assert op in header,op
assert 'rtv0_format' in header and 'cb12_bytes' in header
assert 'PSGetConstantBuffers1(12u,1u,' in header
assert 'cb12_first_constant' in header and 'cb12_num_constants' in header
assert 'cb12_window_buffer_matches' in header
assert 'if(cb12_window)cb12_window->Release();' in header
assert 'ctx1->Release();' in header
assert 'reinterpret_cast<void *>' in integrated
assert 'native_draw_call=INSTRUMENTED gpu_execution=OPEN pixel=OPEN' in header
assert 'samples' in header or 'requests' in header
# Strictly read-only calls and bounded sampling.
for dangerous in ('->PSSet','->OMSet','->DrawIndexed','->Map(', '->UpdateSubresource('):
 assert dangerous not in header,dangerous
assert 'n>256u && n%8192u!=0u' in header
assert 'thread_local request pending' in header
assert 'thread_local snapshot observed' in header
assert 'if(!pending.armed || observed.captured)return;' in header
assert 'ctx!=pending.native_context' in header
assert 'if(ps)ps->Release();' in header
assert 'if(cb12)' in header and 'cb12->Release();' in header
assert 'if(dsv)' in header and 'dsv->Release();' in header
assert 'for(auto *rtv:rtvs)if(rtv)rtv->Release();' in header
print('V419 PRE-DRAW NATIVE D3D11 STATE SOURCE CONTRACT PASS; GPU execution/pixel OPEN')
