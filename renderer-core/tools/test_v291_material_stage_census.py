#!/usr/bin/env python3
"""V291 read-only universal MTD stage contract; no renderer mutation."""
from pathlib import Path
import runpy
root=Path(__file__).resolve().parents[2]
runpy.run_path(str(root/"renderer-core/tools/test_v290_sidecar_draw_coverage.py"),run_name="__main__")
addon=(root/"renderer-core/integrated/integrated_addon.cpp").read_text()
header=(root/"renderer-core/include/dsrrl/runtime/v291_material_stage_trace.hpp").read_text()
cmake=(root/"renderer-core/integrated/CMakeLists.txt").read_text()
assert "16384u" in header and "recorded" in header
assert "material_identity" in header and "raw_mtd_sha256" in header and "flver_sha256" in header
assert "DSRRL V291 MATERIAL STAGE" in header
assert "DSRRL V291 UNROUTED MATERIAL" in header
assert "route_index==345u" not in header
assert "if(!first(" in header
assert "DSRRL_V291_MATERIAL_STAGE_CENSUS" in cmake
assert "NOT DSRRL_V290_FULL_SIDECAR_DRAW_CENSUS" in cmake
assert addon.count('v291_material_trace::log(')==14, addon.count('v291_material_trace::log(')
assert addon.count('v291_material_trace::log_unrouted(')==2
for stage in ("identity_ok","identity_failopen","prepare_failopen",
              "batch_prepared","pointlight_inactive"):
    assert addon.count('"'+stage+'"')==2,stage
for stage in ("native_draw_armed","indexed_native_armed",
              "indexed_issued","indexed_failopen",
              "draw_issued","draw_failopen"):
    assert stage in addon,stage
assert "draw_tx_issued(dispatch.transaction)" in addon
assert "if (!g_pmetal_envspec.prepare(" in addon
assert "DSRRL V291 LERP CUBEMAP" in header
assert "diagnostic_probe_a" in header and "diagnostic_diff_b" in header
assert addon.count("v291_material_trace::log_lerp_cubemaps(")==6
lerp_header=(root/"renderer-core/include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp").read_text()
lerp_runtime=(root/"renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
assert "DSRRL_V291_LERP_CUBEMAP_CENSUS" in lerp_header
assert "prepared.diagnostic_probe_a = prepared.env_resources.probe_a" in lerp_runtime
assert "prepared.diagnostic_beta = source.beta" in lerp_runtime
assert "prepared.diagnostic_diff_a = prepared.envdiffuse_resources.probe_a" in lerp_runtime
assert "DSRRL_V291_LERP_CUBEMAP_CENSUS" in cmake
print("V291_ALL_MATERIAL_STAGE_SOURCE_CONTRACT_PASS observer_only=1 pmetal_included=1")
print("OWNER_RUNTIME_ALL_ASSET_COVERAGE_AND_PTDE_PIXEL_OPEN")
