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
    "src/runtime/pmetal_native_draw_bridge.cpp": "1369db9c4b35dccdf6b9422975d7fef198eac3e2",
    "include/dsrrl/runtime/pmetal_native_draw_bridge.hpp": "a03260f497beb7396fb778a92fe303efb3250d52",
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

# Native P_Metal bridge is byte-identical to R43; no R46/R47 owner-latch code is allowed.
forbid(native, "record_image_entry_owners(", "R48 must not carry R46/R47 native bridge telemetry mutations")
forbid(native_h, "envspec_draw_entered", "R48 native telemetry ABI must remain exact R43")

# Existing R43 profilers are compiled in; no new A/B is introduced.
require(cmake, "DSRRL_FLVER_SELECTOR_PROFILE", "R43 selector sampled profiler")
require(cmake, "DSRRL_POINTLIGHT_PROFILE", "R43 PointLight sampled profiler")

# DoF must really be default-on in both addon lifecycle and bridge internals.
require(integrated, "return !(size == 1u && value[0] == '0');", "addon DoF default-on gate")
require(dof_bridge, "return !(size == 1u && value[0] == '0');", "bridge DoF default-on gate")
forbid(dof_bridge, "value[0] == '1';", "stale opt-in-only DoF gate")
require(integrated, "register_ptde_draw_bridge_runtime(g_core)", "DoF lifecycle registration")
require(integrated, "unregister_dof_runtime();", "DoF lifecycle teardown")

# PERF summary and sampled integrated timings.
require(integrated, "[DSRRL PERF R48]", "R48 perf summary")
require(integrated, "draw_callback_us=", "draw callback timing")
require(integrated, "prepare_batch_us=", "prepare timing")
require(integrated, "dispatch_us=", "dispatch timing")
require(integrated, "selector_us=", "selector timing")
require(integrated, "selector_pmetal_source_us=", "selector P_Metal source timing")
require(cmake, "DSRRL_R48_PERF_PROFILE", "integrated R48 sampler")
require(cmake, "DSRRL_DOF_PROFILE", "DoF sampler")
require(dof_bridge, "[DSRRL PERF R48] DOF", "DoF sampled timing output")

print("R48 exact-R43 + DoF + telemetry audit: PASS")
