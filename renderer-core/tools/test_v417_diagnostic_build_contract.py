from pathlib import Path
r=Path(__file__).resolve().parents[1]
core=(r.parent/'build-v417-fgmodel-registry-diagnostic/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp').read_text()
source=(r/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
tele=(r/'include/dsrrl/runtime/runtime_hot_telemetry.hpp').read_text()
assert 'v296_partial_material' not in core
assert '[DSRRL V411 RX34 OWNER SEQ DRAW]' in (r/'include/dsrrl/runtime/v411_rx34_owner_draw_trace.hpp').read_text()
assert '[DSRRL V408 CONTEXT SELECTOR CENSUS]' in source
assert '[DSRRL V411 OWNER SEQ STAMP]' in source
assert 'if (latest_hook_source(out))' in source
assert '#if defined(DSRRL_V230_PRODUCTION)' not in tele
assert 'DSRRL_V415_FLVER_MODEL_ID_DIAG' in (r/'tools/build_v417_fgmodel_registry_diagnostic.ps1').read_text()
assert 'v415::record_selector' in (r/'src/runtime/flver_engine_hooks.cpp').read_text()
assert 'v415::current_for' in source
assert '[DSRRL V415 MODEL SOURCE JOIN]' in source
assert 'DSRRL_V416_FGMODEL_PARENT_DIAG' in (r/'tools/build_v417_fgmodel_registry_diagnostic.ps1').read_text()
assert '[DSRRL V416 FGMODEL PARENT]' in (r/'src/runtime/flver_engine_hooks.cpp').read_text()
assert '[DSRRL V416 FGMODEL SOURCE JOIN]' in source
assert '/DDSRRL_V417_FGMODEL_REGISTRY_DIAG' in (r/'tools/build_v417_fgmodel_registry_diagnostic.ps1').read_text()
assert '[DSRRL V417 FG REGISTRY PROVENANCE]' in (r/'src/runtime/flver_engine_hooks.cpp').read_text()
assert '[DSRRL V417 FG REGISTRY SOURCE JOIN]' in source
print('V417 DIAGNOSTIC BUILD CONTRACT PASS')
