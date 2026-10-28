from pathlib import Path
r=Path(__file__).resolve().parents[1]
core=(r.parent/'build-v413-selector-diagnostic/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp').read_text()
source=(r/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
tele=(r/'include/dsrrl/runtime/runtime_hot_telemetry.hpp').read_text()
assert 'v296_partial_material' not in core
assert '[DSRRL V411 RX34 OWNER SEQ DRAW]' in core
assert '[DSRRL V408 CONTEXT SELECTOR CENSUS]' in source
assert '[DSRRL V411 OWNER SEQ STAMP]' in source
assert 'if (latest_hook_source(out))' in source
assert '#if defined(DSRRL_V230_PRODUCTION)' not in tele
print('V413 DIAGNOSTIC BUILD CONTRACT PASS')
