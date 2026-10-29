from pathlib import Path
r=Path(__file__).resolve().parents[1]
core=(r.parent/'build-v419-rx34-native-predraw-diagnostic/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp').read_text()
source=(r/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
tele=(r/'include/dsrrl/runtime/runtime_hot_telemetry.hpp').read_text()
assert 'v296_partial_material' not in core
assert '[DSRRL V411 RX34 OWNER SEQ DRAW]' in (r/'include/dsrrl/runtime/v411_rx34_owner_draw_trace.hpp').read_text()
assert '[DSRRL V408 CONTEXT SELECTOR CENSUS]' in source
assert '[DSRRL V411 OWNER SEQ STAMP]' in source
assert 'if (latest_hook_source(out))' in source
assert '#if defined(DSRRL_V230_PRODUCTION)' not in tele
assert 'DSRRL_V415_FLVER_MODEL_ID_DIAG' in (r/'tools/build_v419_rx34_native_predraw_diagnostic.ps1').read_text()
assert 'v415::record_selector' in (r/'src/runtime/flver_engine_hooks.cpp').read_text()
assert 'v415::current_for' in source
assert '[DSRRL V415 MODEL SOURCE JOIN]' in source
assert 'DSRRL_V416_FGMODEL_PARENT_DIAG' in (r/'tools/build_v419_rx34_native_predraw_diagnostic.ps1').read_text()
assert '[DSRRL V416 FGMODEL PARENT]' in (r/'src/runtime/flver_engine_hooks.cpp').read_text()
assert '[DSRRL V416 FGMODEL SOURCE JOIN]' in source
assert '/DDSRRL_V417_FGMODEL_REGISTRY_DIAG' in (r/'tools/build_v419_rx34_native_predraw_diagnostic.ps1').read_text()
assert '[DSRRL V417 FG REGISTRY PROVENANCE]' in (r/'src/runtime/flver_engine_hooks.cpp').read_text()
assert '[DSRRL V417 FG REGISTRY SOURCE JOIN]' in source
assert '/DDSRRL_V418_RX34_GENERATIONAL_ISSUED_JOIN' in (r/'tools/build_v419_rx34_native_predraw_diagnostic.ps1').read_text()
assert '[DSRRL V418 RX34 MODEL ISSUED JOIN]' in (r/'include/dsrrl/runtime/v411_rx34_owner_draw_trace.hpp').read_text()
assert 'w.model_this=model_stamp_v418.model_this;' in source
assert '/DDSRRL_V419_RX34_PREDRAW_NATIVE_STATE' in (r/'tools/build_v419_rx34_native_predraw_diagnostic.ps1').read_text()
assert '[DSRRL V419 RX34 PRE-DRAW NATIVE STATE]' in (r/'include/dsrrl/runtime/v419_rx34_predraw_state.hpp').read_text()
assert 'v419::before_native_draw(ctx);' in (r/'src/runtime/draw_state_transaction.cpp').read_text()
assert 'v419::finish(' in (r/'integrated/integrated_addon.cpp').read_text()
print('V419 DIAGNOSTIC BUILD CONTRACT PASS')
