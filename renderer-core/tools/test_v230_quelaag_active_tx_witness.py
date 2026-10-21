#!/usr/bin/env python3
"""Static regression: capture Quelaag P_Metal from the executed transaction, not the disabled persistent vtable hook."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
src=(root/"integrated/integrated_addon.cpp").read_text("utf8")
builder=(root/"tools/build_v230_envdiffuse_cinematic_trace.ps1").read_text("utf8")
assert "constexpr bool k_pmetal_native_draw_runtime_enabled = false;" in src
assert "constexpr bool k_pmetal_direct_current_native_dispatch = true;" in src
assert src.count("[DSRRL EDC ACTIVE TX]")==2
assert 'kind=draw' in src and 'kind=indexed' in src
assert src.count('w.route == 345u')==2
assert src.count('w.source && w.ready && w.selected')==2
assert src.count('dsrrl::runtime::draw_tx_issued(dispatch.transaction)')>=2
assert src.count('edc_tx_result::issued_restore_failed')==2
assert src.count('dsrrl::runtime::draw_tx_result::issued_restore_failed')==2  # inherited V315 code unchanged
assert src.count('dsrrl::core::operator_id::env_diffuse')>=2
assert src.count('prepared.batch.mutation.srv_count')>=2
assert src.count('prepared.batch.mutation.sampler_count')>=2
assert "DSRRL_V230_ENVDIFFUSE_CINEMATIC_TRACE" in builder
assert "build_v230_envdiffuse_rgba_fix.ps1" in builder
print("QUELAAG_ACTIVE_TX_SOURCE_CONTRACT_PASS: both actual dispatch sites, P_Metal-only, read-only, no renderer equation changes")
print("RUNTIME_ACTIVATION=OPEN; PTDE_PIXEL=OPEN")
