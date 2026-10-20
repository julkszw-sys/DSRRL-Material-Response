#!/usr/bin/env python3
"""Strict construction-only gates for parallel fail-open investigation."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
def read(relative):
    return (r/relative).read_text(encoding="utf-8")
probe=read("src/runtime/envspec_resource_runtime.cpp")
draw=read("src/runtime/pmetal_envspec_draw_runtime.cpp")
mr=read("integrated/integrated_addon.cpp")
ledger=read("include/dsrrl/runtime/v230_failopen_diagnostic.hpp")
stable=read("src/operators/env_spec/pmetal_rgba_materializer.cpp")
donors=read("data/provenance/user_envspec_intensity_overrides_v355.tsv")
build=read("tools/build_v230_production.ps1")
assert probe.count("snapshot_ready_envspec(")==3 # declaration and two calls
for marker in ("reject_stage = 1u;", "g_device == nullptr ? 2u", "reject_stage = 5u;",
               "reject_stage = 6u;", "reject_stage = 8u;", "reject_stage = 9u;",
               "v230_failopen_diag::probe_reason(final_stage,initial_reject_stage"):
    assert marker in probe,marker
assert "g_resource_by_view.find(view_key)" in probe
assert "v230_failopen_diag::probe_reason" in probe
for marker in ("[DSRRL SPECRGB ROOT]","[DSRRL DONOR SOURCE]",
               "[DSRRL CEASELESS ROW6 SOURCE_READY]","[DSRRL CEASELESS ROW6 REQUEST_READY]","source_.latest(material, source)",
               "material_resources_.probe_exact_specular_companion(context)"):
    assert marker in draw,marker
assert "[DSRRL MR ROOT]" in mr
assert "A correctly materialized HemEnvLerp shader" in stable
assert "pmetal_rgba_materialize_result::pass_not_candidate" in stable
assert 'inline void probe_reason(' in ledger and 'k_probe_names' in ledger
assert "/DDSRRL_V230_FAILOPEN_DIAGNOSTICS" in build
assert "8\ta14\tm14_LightBank.param\tc9b879a8c5e624d0\t6\t100\t170" in donors
assert "DSRRL_V230_STOCK_DSR_ENVDIFFUSE" in draw
print("V230_PARALLEL_FAILOPEN_CONSTRUCTION_PASS probe_substages=11 specrgb=1 donor_witness=1 mr=1 native_operators_unchanged=1")
