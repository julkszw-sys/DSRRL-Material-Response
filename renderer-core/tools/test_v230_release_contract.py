#!/usr/bin/env python3
"""Release invariants after V359 generation and V230 instrumentation removal."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
version=json.loads((root/"integrated/VERSION.json").read_text("utf8"))
assert version["version"]=="2.3.0", "Add-on version not v2.3.0"
assert version["artifact_stem"]=="DSRRL_Restored_Lighting_v2_3_0"
builder=(root/"tools/build_v230_production.ps1").read_text("utf8")
assert "/DDSRRL_V230_PRODUCTION" in builder
assert "/DDSRRL_V359_ATTESTED_GI_NAME_ALIAS" in builder
for token in ("/DDSRRL_V352_LONG_SOAK_PRESENT",
              "/DDSRRL_V353_CPU_MODULE_PROFILER",
              "/DDSRRL_V296_ROOTCAUSE_TELEMETRY"):
    assert token not in builder, f"Diagnostic feature still compiled: {token}"
assert "Once '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF' '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF'" in builder
assert "build_v350_gpu_resident_pointlight_off_rc2.ps1" in builder
assert "DSRRL_Restored_Lighting_v2.3.0.addon64" in builder
assert "DSRRL_V350_PTDE_LERP_RGB_COMPONENT_SWIZZLE" in (root/"tools/build_v350_gpu_resident_pointlight_off_rc2.ps1").read_text("utf8")
qpc=(root/"include/dsrrl/runtime/qpc_causality_bisect.hpp").read_text("utf8")
assert "DSRRL_QPC_SCOPE_SHELL_NO_CLOCK" in qpc
assert "class scope" in qpc and "mode_(g_mode.load(" in qpc
assert "g_mode.store(0u, std::memory_order_release);" in qpc
telemetry=(root/"include/dsrrl/runtime/runtime_hot_telemetry.hpp").read_text("utf8")
assert telemetry.count("#if defined(DSRRL_V230_PRODUCTION)")>=3
core=(root.parent/"build-v230-release/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp").read_text("utf8")
for fn in ("void log_state(","void log_effect_matrix(","void observe_pointlight_render_target("):
    assert fn in core
assert "qpc_bisect::report();" not in core
gpu=(root/"src/runtime/material_resource_draw_runtime.cpp").read_text("utf8")
for term in ("g_session_residency","g_sidecar_shared","on_init_device","QueryVideoMemoryInfo"):
    assert term in gpu, f"V350 GPU residency optimization missing: {term}"
source=(root/"src/runtime/pmetal_env_source_runtime.cpp").read_text("utf8")
assert "latest_hook_source" in source, "Protected producer fallback absent"
env=(root/"src/runtime/envspec_resource_runtime.cpp").read_text("utf8")
assert "v359_verified_native_name_probe_locked" in env
assert "g_v359_map_conflict" in env
assert "v359_match_original_gi_name" in env
print("V230_RELEASE_CONTRACT_PASS version=2.3.0 qpc_scope=RETAINED gpu_residency=RETAINED")
print("V230_TELEMETRY_DISABLED v352/v353/v296root=COMPILED_OUT hot_effect=FALSE")
print("V230_V359_EXACT_GI_SOURCE_OWNERSHIP_AND_FAILOPEN_RETAINED")
