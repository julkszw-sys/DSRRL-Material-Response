#!/usr/bin/env python3
"""Run only after V351, V352, V353 materializers."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
header=(r/"include/dsrrl/runtime/v353_sampled_cpu_module_profiler.hpp").read_text("utf-8")
integrated=(r/"integrated/integrated_addon.cpp").read_text("utf-8")
build=(r/"tools/build_v353_sampled_cpu_modules.ps1").read_text("utf-8")
assert "k_stride = 128u" in header
assert "(n & (k_stride - 1u)) != 0u" in header
assert "QueryPerformanceCounter(&v)" in header
assert "projected_cpu_ms_per_s=" in header
assert "caution=OVERLAPPING_STAGES_DO_NOT_SUM" in header
assert "gpu_time=NOT_MEASURED" in header
assert "DSRRL_V353_CPU_MODULE_PROFILER" in build
assert "-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=ON" in build
assert "DSRRL_QPC_SCOPE_SHELL_NO_CLOCK" not in header
assert "g_v352_soak.last_window_seconds()" in integrated
assert "g_v353_window_index" in integrated
assert integrated.count("v353::scoped_sample") == 3
paths={
"src/runtime/material_owner_selection.cpp":1,
"src/operators/material_response/material_response_island.cpp":1,
"src/runtime/material_response_draw_transaction.cpp":1,
"src/runtime/pmetal_env_source_runtime.cpp":1,
"src/runtime/pmetal_envspec_draw_runtime.cpp":1,
"src/runtime/envspec_resource_runtime.cpp":2,
"src/runtime/material_resource_draw_runtime.cpp":1,
"src/runtime/island_draw_adapter.cpp":2,
}
n=3
for path,count in paths.items():
 s=(r/path).read_text("utf-8")
 assert s.count("v353::scoped_sample")==count,(path,s.count("v353::scoped_sample"),count)
 assert "#if defined(DSRRL_V353_CPU_MODULE_PROFILER)" in s
 n+=count
assert n==13,n
assert "DSRRL_V351_EXACT_MATERIAL_ROUTE_RECOVERY" in build
assert "DSRRL_V351_TYPED_DSR_ONLY_OPERATOR" in build
assert "DSRRL_V352_LONG_SOAK_PRESENT" in build
assert "DSRRL_POINTLIGHT_DRAWTIME_BYPASS" not in header
print("V353_SOURCE_PROFILER_PASS scopes=13 stages=11 stride=128 30s_reporting=1 GPU_timing=OPEN")
