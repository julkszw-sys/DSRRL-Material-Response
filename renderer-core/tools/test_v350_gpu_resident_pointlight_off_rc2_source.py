#!/usr/bin/env python3
"""V350 GPU Resident RC2: additive source contract for exact addon PointLight OFF.

This checks construction/routing intent, not actual game runtime or pixels.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[2]
build=(root/"renderer-core/tools/build_v350_gpu_resident_pointlight_off_rc2.ps1").read_text()
runtime=(root/"renderer-core/integrated/integrated_addon.cpp").read_text()
cmake=(root/"renderer-core/integrated/CMakeLists.txt").read_text()

assert "'-DDSRRL_POINTLIGHT_DRAWTIME_BYPASS=ON'" in build
assert "'-DDSRRL_PHYSICAL_CUT_POINTLIGHT_ALL=ON'" in build  # only legacy V285 prerequisite
assert "'-DDSRRL_DRAWTIME_ISLANDS_BYPASS=ON'" not in build
assert "-DDSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG=ON" in build
assert "-DDSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE=ON" in build
assert "v350_rc2_gpu_resident_pointlight_off" in build
assert "DSRRL_V350_GPU_RESIDENT_POINTLIGHT_OFF_RC2.addon64" in build
assert "DSRRL_POINTLIGHT_DRAWTIME_BYPASS" in cmake
assert "target_compile_definitions" in cmake
assert ("#if defined(DSRRL_POINTLIGHT_DRAWTIME_BYPASS) || "
        "defined(DSRRL_DRAWTIME_ISLANDS_BYPASS)") in runtime
assert "constexpr bool k_pointlight_drawtime_runtime_enabled = false;" in runtime
assert "k_pointlight_drawtime_runtime_enabled &&\n        flver_hooks &&\n        g_fixed_pointlight.install();" in runtime
assert "k_pointlight_drawtime_runtime_enabled &&\n        flver_hooks &&\n        g_clustered_pnts.install();" in runtime
assert "if (k_pointlight_drawtime_runtime_enabled) {" in runtime
policy=runtime.split("bool integrated_operator_enabled_by_policy(",1)[1].split("bool enable_integrated_islands()",1)[0]
assert "#if defined(DSRRL_POINTLIGHT_DRAWTIME_BYPASS)" in policy
for operator in ("point_light", "local_specular_legacy", "pointlight_pnts_attenuation"):
    assert "case dsrrl::core::operator_id::"+operator+":" in policy
assert "case dsrrl::core::operator_id::hemdir3:" in policy
assert "[DSRRL GPU RESIDENT RC1] default=ON config_file=NONE" in (root/"renderer-core/src/runtime/material_resource_draw_runtime.cpp").read_text()
print("V350_RC2_POINTLIGHT_OFF_SOURCE_CONTRACT_PASS")
