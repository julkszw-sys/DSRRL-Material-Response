#!/usr/bin/env python3
"""V329 static source/builder contract. No profiler or in-game FPS inference."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
cmake = (ROOT/"renderer-core/integrated/CMakeLists.txt").read_text(encoding="utf-8")
builder = (ROOT/"renderer-core/tools/build_v329_pmetal_cache_rmw_one_addon.ps1").read_text(encoding="utf-8")
registry = (ROOT/"renderer-core/src/runtime/flver_identity_registry.cpp").read_text(encoding="utf-8")
flver = (ROOT/"renderer-core/src/runtime/flver_engine_hooks.cpp").read_text(encoding="utf-8")
integrated = (ROOT/"renderer-core/integrated/integrated_addon.cpp").read_text(encoding="utf-8")

def require(cond, context):
    if not cond: raise SystemExit("V329 SOURCE FAIL: "+context)

section=cmake.split("if(DSRRL_V293_PARSER_EXACT_FLVER_ALIAS)",1)[1].split("endif()",1)[0]
require("NOT DSRRL_V285_ATOMIC_NONMETAL_DIAG" in section,"V293 nonmetal owner semantic gate lost")
require("NOT DSRRL_V291_MATERIAL_STAGE_CENSUS" not in section, "V293 still tied to diagnostic observer")
for flag in [
    "DSRRL_V285_ATOMIC_NONMETAL_DIAG=ON",
    "DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB=ON",
    "DSRRL_V293_PARSER_EXACT_FLVER_ALIAS=ON",
    "DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW=ON",
    "DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE=ON",
    "DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL=ON",
    "DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE=ON",
    "DSRRL_V315_DIRECT_749_RUNTIME_LEDGER",
    "DSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF",
    "test_v323_exact_resource_transport_contract.py",
]:
    require(flag in builder, "required single-addon feature missing "+flag)
for option in [
    "DSRRL_V290_FULL_SIDECAR_DRAW_CENSUS=ON",
    "DSRRL_V291_MATERIAL_STAGE_CENSUS=ON",
    "DSRRL_V291_LERP_CUBEMAP_CENSUS=ON",
    "DSRRL_FULL_TELEMETRY_DEFAULT_ON=ON",
]:
    require(option not in builder, "profiler/diagnostic enabled: "+option)
require("build-v329" in builder and "out-v329" in builder, "output artifact identity drift")
require("test_v329_pmetal_cache_hotcount_contract.py" in builder, "build guard absent")
require("#if defined(DSRRL_V290_FULL_SIDECAR_DRAW_CENSUS)" in integrated, "V290 probe guard absent")

start=registry.index("bool flver_identity_lookup_direct_parser_model(")
end=registry.index("bool flver_identity_enrich_owner(",start)
func=registry[start:end]
for token in [
    "lookup_tls_cache_hit(candidate_model)",
    "cached_epoch_survives_for_model(",
    "cached->epoch = current_epoch;",
    "sha256 = cached->sha;",
    "std::lock_guard<std::mutex> lock(g_mutex);",
    "lookup_tls_cache_slot(candidate_model)",
    "g_by_model.find(candidate_model)",
    "cached = { candidate_model, epoch, insert_epoch, {}, false };",
    "cached = { candidate_model, epoch, insert_epoch, sha256, true };",
]:
    require(token in func, "exact FLVER positive SHA registry contract lost "+token)
require("lookup_tls_cache_hit(candidate_model)" in func.split("std::lock_guard<std::mutex> lock(g_mutex);")[0],
        "alias success still waits for registry mutex")
require("g_by_model.find(candidate_model)" in func.split("std::lock_guard<std::mutex> lock(g_mutex);")[1],
        "fallback no longer verifies parser registration")
require("if (auto *cached = lookup_tls_cache_hit(candidate_model);" in func,
        "uncached SHA inference")
require("bool cached_epoch_survives_for_model(" in registry and
        "publish_invalidation_locked(model);" in registry and
        "flver_identity_observe_destroy(" in registry,
        "lifetime journal invalidation weakened")
require("flver_identity_lookup_direct_parser_model(" in flver and
        "source_mtd == direct_runtime_mtd.semantic_name_hash" in flver and
        "identity.raw_mtd_sha256 ==\n            direct_runtime_mtd.raw_mtd_sha256" in flver,
        "exact V293 MTD/material/slot constraints weakened")
pmetal=(ROOT/"renderer-core/src/runtime/pmetal_env_source_runtime.cpp").read_text(encoding="utf-8")
# These nine counters are diagnostic observations, not source/consumer keys.
# They previously executed global atomic RMW even when CPU telemetry was OFF.
diagnostics=[
    "g_hook_producer_cache_hit", "g_hook_producer_cache_miss",
    "g_hook_producer_cache_busy", "g_hook_producer_cache_publish",
    "g_region_cache_hit", "g_region_cache_miss",
    "g_endpoint_cache_hit", "g_endpoint_cache_miss",
    "g_endpoint_cache_fill",
]
for name in diagnostics:
    require(pmetal.count(f"telemetry::hot_count({name});")==1,
            f"one source-only cache diagnostic hot_count required: {name}")
    require(f"{name}.fetch_add(" not in pmetal,
            f"unconditional debug atomic on selector hot path: {name}")
    require(f"{name}.load(" in pmetal or name.startswith("g_hook_producer_cache_"),
            f"counter identity/telemetry ABI changed: {name}")
require("bool latest_hook_source(" in pmetal and
        "hook_producer_cache_publish_exact(" in pmetal and
        "cached_readable_window(" in pmetal and
        "endpoint_cache_lookup(" in pmetal,
        "protected producer/source and exact cache routines must remain")
print("V329 PASS: real V293 parser-source SHA hit lock-free by epoch journal; exact fallback and FLVER/MTD/slot gates intact; census/CPU telemetry OFF; one-addon provenance")
