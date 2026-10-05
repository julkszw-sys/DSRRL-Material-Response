from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else "renderer-core")

def read(rel: str) -> str:
    p = root / rel
    if not p.is_file():
        raise RuntimeError(f"missing required file: {rel}")
    return p.read_text(encoding="utf-8", errors="strict")

def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise RuntimeError(f"{label}: missing {needle!r}")

def forbid(text: str, needle: str, label: str) -> None:
    if needle in text:
        raise RuntimeError(f"{label}: forbidden {needle!r}")

pmetal = read("src/runtime/pmetal_env_source_runtime.cpp")
envspec = read("src/runtime/pmetal_envspec_draw_runtime.cpp")
pointlight = read("src/runtime/clustered_pnts_draw_runtime.cpp")
native = read("src/runtime/pmetal_native_draw_bridge.cpp")
integrated = read("integrated/integrated_addon.cpp")
cmake = read("integrated/CMakeLists.txt")

# R47 P_Metal: R45 hot path retained, R43 visible compatibility fallback retained.
require(pmetal, "[DSRRL PMETAL R47] production_hot_counters=GATED", "R47 P_Metal hot-path attestation")
require(pmetal, "latest_r43_visible_fallback(out)", "R43 visible fallback")
require(pmetal, "decoded_endpoint_identity_reuse=ON", "decoded endpoint identity reuse")
require(pmetal, "selector_miss_identity_reuse=ON", "selector miss identity reuse")
require(pmetal, "material_sha_byte_compare=ON", "material SHA byte compare")
require(pmetal, "producer_key_handoff=ON", "producer key handoff")

# Historical zero-t11 diagnostic must not be active by default.
require(envspec, "#if defined(DSRRL_PMETAL_ZERO_T11_CONTRIBUTION_DIAG)", "explicit zero-t11 diagnostic gate")
forbid(cmake, "DSRRL_PMETAL_ZERO_T11_CONTRIBUTION_DIAG)", "zero-t11 diagnostic must not be wired into normal build")

# R47 PointLight: exact R45 frame-state acceleration plus the old R43 donor fallback.
require(pointlight, "capture_ptde_source_from_frame_state(", "PointLight frame-state fast path")
require(pointlight, "pointlight_ptde_source::capture(", "R43 PointLight donor fallback")
require(pointlight, "[DSRRL POINTLIGHT R47] frame_state_fastpath=ON R43_capture_fallback=ON", "PointLight hybrid attestation")

# Native steady-state diagnostics are gated.
require(native, "[DSRRL NATIVE R45] steady_state_diagnostic_counters=GATED", "native bridge diagnostic gating")

# DoF is part of this candidate and enabled by default; explicit 0 is kill switch.
require(integrated, "bool dof_runtime_enabled() noexcept", "DoF runtime gate")
require(integrated, "return !(size == 1u && value[0] == '0');", "DoF default-on policy")
require(integrated, "[DSRRL DoF] PTDE private island DEFAULT_ON READY", "DoF default-on runtime attestation")
require(integrated, "register_ptde_draw_bridge_runtime(g_core)", "DoF bridge lifecycle")
require(cmake, "dsrrl_core_dof_plain_rate_embedded", "DoF payload build wiring")

print("R47 performance + DoF audit: PASS")
