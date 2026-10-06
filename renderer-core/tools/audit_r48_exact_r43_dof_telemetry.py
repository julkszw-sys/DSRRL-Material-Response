from pathlib import Path
import hashlib
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else "renderer-core")

def read_bytes(rel: str) -> bytes:
    p = root / rel
    if not p.is_file():
        raise RuntimeError(f"missing required file: {rel}")
    return p.read_bytes()

def read(rel: str) -> str:
    return read_bytes(rel).decode("utf-8", errors="strict")

def git_blob_sha1(data: bytes) -> str:
    header = f"blob {len(data)}\0".encode("ascii")
    return hashlib.sha1(header + data).hexdigest()

def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise RuntimeError(f"{label}: missing {needle!r}")

def forbid(text: str, needle: str, label: str) -> None:
    if needle in text:
        raise RuntimeError(f"{label}: forbidden {needle!r}")

# These are authenticated R43 blobs and must remain byte-identical in R48.
expected_r43 = {
    "src/runtime/pmetal_env_source_runtime.cpp": "c88cb8f85fac64d7072605d7173f09f984a416cf",
    "src/runtime/clustered_pnts_draw_runtime.cpp": "deb0dcaeda7e608dbe46c2b282e27b7b5223d398",
    "src/runtime/pmetal_envspec_draw_runtime.cpp": "277ba7191182e5d19fbbd07c0874cfb6a72e8168",
    "src/runtime/material_response_draw_transaction.cpp": "44fa09b1777b16b2069d7395890c629e361469a2",
    "src/runtime/pmetal_producer_state.cpp": "d4d1e6fd064468ad87ff27f443370b29c3ef89be",
}
for rel, expected in expected_r43.items():
    actual = git_blob_sha1(read_bytes(rel))
    if actual != expected:
        raise RuntimeError(
            f"R43 byte-identity regression: {rel} actual={actual} expected={expected}"
        )

native = read("src/runtime/pmetal_native_draw_bridge.cpp")
native_h = read("include/dsrrl/runtime/pmetal_native_draw_bridge.hpp")
integrated = read("integrated/integrated_addon.cpp")
cmake = read("integrated/CMakeLists.txt")
dof_bridge = read("src/runtime/dof_ptde_draw_bridge_runtime.cpp")

# Native bridge deviations from R43 are telemetry-only owner latches.
require(native, "record_image_entry_owners(", "native image-entry telemetry")
require(native, "g_image_entry_envspec", "EnvSpec native image-entry latch")
require(native, "g_image_entry_pointlight", "PointLight native image-entry latch")
require(native, "g_image_entry_material_response", "MR native image-entry latch")
require(native, "g_image_entry_local_specular", "local-specular native image-entry latch")
require(native_h, "bool envspec_draw_entered = false;", "native telemetry ABI")
require(native, "capture_state(", "R43 native capture path preserved")
require(native, "apply_mutation(", "R43 native mutation path preserved")
require(native, "restore_state(", "R43 native restore path preserved")

# Existing R43 profilers are compiled in; no new A/B is introduced.
require(cmake, "DSRRL_FLVER_SELECTOR_PROFILE", "R43 selector sampled profiler")
require(cmake, "DSRRL_POINTLIGHT_PROFILE", "R43 PointLight sampled profiler")

# DoF must really be default-on in both addon lifecycle and bridge internals.
require(integrated, "return !(size == 1u && value[0] == '0');", "addon DoF default-on gate")
require(dof_bridge, "return !(size == 1u && value[0] == '0');", "bridge DoF default-on gate")
forbid(dof_bridge, "value[0] == '1';", "stale opt-in-only DoF gate")
require(integrated, "register_ptde_draw_bridge_runtime(g_core)", "DoF lifecycle registration")
require(integrated, "unregister_dof_runtime();", "DoF lifecycle teardown")

# IMAGE_ENTRY must distinguish actual draw/visible handoff from prepare/arm.
require(integrated, "[DSRRL IMAGE_ENTRY R48]", "image-entry summary")
require(integrated, "native.envspec_draw_entered", "EnvSpec actual native draw proof")
require(integrated, "native.pointlight_draw_entered", "PointLight actual native draw proof")
require(integrated, "dof_bridge.visible_handoffs != 0u", "DoF visible handoff proof")
require(integrated, "dof_tonemap.dof_source_hits != 0u", "DoF ToneMap consume proof")
require(integrated, "pixel=UNVERIFIED", "no pixel-equivalence overclaim")

# PERF summary and sampled integrated timings.
require(integrated, "[DSRRL PERF R48]", "R48 perf summary")
require(integrated, "prepare_batch_us=", "prepare timing")
require(integrated, "dispatch_us=", "dispatch timing")
require(integrated, "selector_us=", "selector timing")

print("R48 exact-R43 + DoF + telemetry audit: PASS")
