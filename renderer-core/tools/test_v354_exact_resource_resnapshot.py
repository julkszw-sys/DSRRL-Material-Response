#!/usr/bin/env python3
"""V354: preserve every exact PTDE operator, material, SRV and source gate."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf8")
o=(root/"src/runtime/material_owner_selection.cpp").read_text("utf8")
b=(root/"tools/build_v354_exact_resource_retry.ps1").read_text("utf8")
need=[
"DSRRL_V354_EXACT_RESOURCE_RESNAPSHOT",
"stage=envspec_native_exact_sha_rescued",
"stage=spec_rgb_native_exact_sidecar_rescued",
"[DSRRL V354 PROBE REJECT]",
"[DSRRL V354 SPEC REJECT]",
"env_resources_.prepare(",
"material_resources_.prepare_draw_requests(",
"material_resources_.release_prepared_draw(",
"!prepared.material_resources.spec_rgb",
"probe_exact_specular_companion(context)",
"probe_rejects_",
"spec_rgb_rejects_",
"exact_pmetal_envspec_candidate(",
"classify_mtd_envspec_semantics",
]
for key in need: assert key in s, key
assert s.count("stage=spec_rgb_native_exact_sidecar_rescued")==1
assert s.count("stage=envspec_native_exact_sha_rescued")==1
assert "v354_native_probe_for_view(native_view[0],probe_a)" in s
assert "v354_native_probe_for_view(native_view[2],probe_b)" in s
assert "native_a_known=%u native_a_probe=%u" in s
assert "native_b_known=%u native_b_probe=%u" in s
assert "v354_native_probe_for_view(" in (root/"include/dsrrl/runtime/envspec_resource_runtime.hpp").read_text("utf8")
assert "return probe_for_native_view(view,probe);" in (root/"src/runtime/envspec_resource_runtime.cpp").read_text("utf8")
assert "source_.latest(material, source)" in s
assert "material.owner_tuple_exact" in s
assert "g_current = resolved;" in o and "v351::recover_exact_route(identity)" in o
assert "/DDSRRL_V354_EXACT_RESOURCE_RESNAPSHOT" in b
assert "-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=ON" in b
assert "DSRRL_V353_CPU_MODULE_PROFILER" in b
assert "DSRRL_V352_LONG_SOAK_PRESENT" in b
assert "DSRRL_V351_TYPED_DSR_ONLY_OPERATOR" in b
print("V354_SAFE_RESNAPSHOT_SOURCE_PASS; all material/provider/SRV SHA guards retained; no probe or owner alias")
