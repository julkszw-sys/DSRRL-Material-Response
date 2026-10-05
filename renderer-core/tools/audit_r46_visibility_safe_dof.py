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

pmetal = read("src/runtime/pmetal_env_source_runtime.cpp")
pointlight = read("src/runtime/clustered_pnts_draw_runtime.cpp")
integrated = read("integrated/integrated_addon.cpp")
cmake = read("integrated/CMakeLists.txt")
types = read("include/dsrrl/core/types.hpp")
catalog = read("src/operator_catalog.cpp")
policy = read("include/dsrrl/core/draw_transaction_policy.hpp")

# R43 visible-materialization semantics must remain present.
require(pmetal, "latest_hook_source(out)", "R43 P_Metal visible-source fallback")
require(pointlight, "pointlight_ptde_source::capture(", "R43 PointLight exact capture path")

# Only behavior-independent R46 hot-path reductions are allowed.
require(pmetal, "[DSRRL PMETAL R46] visibility_baseline=R43", "R46 P_Metal attestation")
require(pointlight, "[DSRRL POINTLIGHT R46] visibility_baseline=R43", "R46 PointLight attestation")

# Exact native Core DoF island from 302089dd.
required_dof = [
    "include/dsrrl/operators/dof/dof_island.hpp",
    "include/dsrrl/operators/dof/dof_resource_contract.hpp",
    "include/dsrrl/operators/dof/ptde_dofbank_embedded.hpp",
    "include/dsrrl/runtime/dof_preflight.hpp",
    "include/dsrrl/runtime/dof_authored_state_runtime.hpp",
    "include/dsrrl/runtime/dof_private_resource_runtime.hpp",
    "include/dsrrl/runtime/dof_ptde_scheduler_runtime.hpp",
    "include/dsrrl/runtime/dof_host_depth_route_runtime.hpp",
    "include/dsrrl/runtime/dof_tonemap_handoff_runtime.hpp",
    "include/dsrrl/runtime/dof_ptde_draw_bridge_runtime.hpp",
    "src/runtime/dof_preflight.cpp",
    "src/runtime/dof_authored_state_runtime.cpp",
    "src/runtime/dof_private_resource_runtime.cpp",
    "src/runtime/dof_ptde_scheduler_runtime.cpp",
    "src/runtime/dof_host_depth_route_runtime.cpp",
    "src/runtime/dof_tonemap_handoff_runtime.cpp",
    "src/runtime/dof_ptde_draw_bridge_runtime.cpp",
    "data/provenance/dof_plain_rate_exact_payload_v1.json",
    "tools/generate_dof_plain_rate_embedded.py",
]
for rel in required_dof:
    read(rel)

require(types, "post_dof_ptde", "DoF operator ABI")
require(catalog, '"post.dof_ptde"', "DoF operator catalog")
require(policy, "operator_id::post_dof_ptde, draw_transaction_mode::blocked", "DoF guarded draw policy")
require(integrated, "DSRRL_EXPERIMENTAL_PTDE_DOF", "DoF opt-in control plane")
require(integrated, "register_ptde_draw_bridge_runtime(g_core)", "DoF draw bridge lifecycle")
require(integrated, "unregister_dof_runtime();", "DoF teardown")
require(cmake, "dsrrl_core_dof_plain_rate_embedded", "DoF exact plain-rate generator wiring")
require(cmake, "src/runtime/dof_ptde_draw_bridge_runtime.cpp", "DoF native Core link wiring")

print("R46 visibility-safe + DoF 302089dd audit: PASS")
