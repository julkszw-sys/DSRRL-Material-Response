#!/usr/bin/env python3
"""Non-perturbing V350-RC1 performance derivative: exact source invariants.

No QPC profiler or in-game FPS is used as performance proof. A PASS establishes
the source cut only; renderer compatibility and PTDE pixels remain separate.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
read = lambda path: (ROOT / path).read_text(encoding="utf-8")
registry = read("renderer-core/src/runtime/flver_identity_registry.cpp")
pmetal = read("renderer-core/src/runtime/pmetal_env_source_runtime.cpp")
selector = read("renderer-core/src/runtime/flver_engine_hooks.cpp")
cmake = read("renderer-core/integrated/CMakeLists.txt")
builder = read("renderer-core/tools/build_v350_perf_one_addon.ps1")
integrated = read("renderer-core/integrated/integrated_addon.cpp")


def require(condition, message):
    if not condition:
        raise SystemExit("V350 PERF SOURCE FAIL: " + message)


start = registry.index("bool flver_identity_lookup_direct_parser_model(")
end = registry.index("bool flver_identity_enrich_owner(", start)
direct = registry[start:end]
before_lock, after_lock = direct.split(
    "std::lock_guard<std::mutex> lock(g_mutex);", 1
)
for token in (
    "lookup_tls_cache_hit(candidate_model)",
    "cached != nullptr && cached->present",
    "cached_epoch_survives_for_model(",
    "cached->epoch = current_epoch",
    "sha256 = cached->sha",
):
    require(token in before_lock, "positive identity SHA TLS fast path: " + token)
for token in (
    "lookup_tls_cache_slot(candidate_model)",
    "g_by_model.find(candidate_model)",
    "cached = { candidate_model, epoch, insert_epoch, {}, false };",
    "cached = { candidate_model, epoch, insert_epoch, sha256, true };",
):
    require(token in after_lock, "authoritative parser map fallback: " + token)
for token in (
    "publish_invalidation_locked(model);",
    "g_insert_epoch.fetch_add(",
    "void flver_identity_observe_destroy(",
    "void flver_identity_reset()",
):
    require(token in registry, "FLVER lifetime invalidation: " + token)
for token in (
    "flver_identity_lookup_direct_parser_model(",
    "source_mtd == direct_runtime_mtd.semantic_name_hash",
    "direct_runtime_mtd.raw_mtd_sha256",
    "selector_identity_cache_publish(",
):
    require(token in selector, "selector source/full MTD gate: " + token)
require("if (!direct_parser_owner)" in selector,
        "parser alias must not use legacy selector material cache")

counters = (
    "g_hook_producer_cache_hit", "g_hook_producer_cache_miss",
    "g_hook_producer_cache_busy", "g_hook_producer_cache_publish",
    "g_region_cache_hit", "g_region_cache_miss",
    "g_endpoint_cache_hit", "g_endpoint_cache_miss",
    "g_endpoint_cache_fill",
)
for name in counters:
    require(pmetal.count("telemetry::hot_count(" + name + ");") == 1,
            "missing telemetry-OFF diagnostic cut " + name)
    require(name + ".fetch_add(" not in pmetal,
            "global cache RMW when telemetry OFF " + name)
for token in (
    "bool latest_hook_source(", "hook_producer_cache_publish_exact(",
    "cached_readable_window(", "endpoint_cache_lookup(",
):
    require(token in pmetal, "protected source/cache path lost " + token)

cmake_gate = cmake.split("if(DSRRL_V293_PARSER_EXACT_FLVER_ALIAS)", 1)[1].split(
    "endif()", 1
)[0]
require("NOT DSRRL_V285_ATOMIC_NONMETAL_DIAG" in cmake_gate,
        "V293 nonmetal semantic requirement missing")
require("NOT DSRRL_V291_MATERIAL_STAGE_CENSUS" not in cmake_gate,
        "V293 unnecessarily depends on diagnostic census")
for token in (
    "DSRRL_V350_PTDE_LERP_RGB_COMPONENT_SWIZZLE",
    "DSRRL_V349_PTDE_LERP_ENVSPEC_YZW_FIX",
    "DSRRL_V348_PTDE_LERP_SPECRGB_RGB_FIX",
    "DSRRL_V293_PARSER_EXACT_FLVER_ALIAS=ON",
    "DSRRL_V285_ATOMIC_NONMETAL_DIAG=ON",
    "DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB=ON",
    "DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE=ON",
    "DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL=ON",
    "DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE=ON",
    "DSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF",
    "DSRRL_FLVER_SELECTOR_PROFILE=OFF",
    "build-v350-perf",
    "DSRRL_V350_RC1_STREAMING_PERF.addon64",
):
    require(token in builder, "V350 RC1 build lineage missing " + token)
for token in (
    "-DDSRRL_V290_FULL_SIDECAR_DRAW_CENSUS=ON",
    "-DDSRRL_V291_MATERIAL_STAGE_CENSUS=ON",
    "-DDSRRL_V291_LERP_CUBEMAP_CENSUS=ON",
    "-DDSRRL_FLVER_SELECTOR_PROFILE=ON",
    "-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=ON",
):
    require(token not in builder, "CPU diagnostic inadvertently compiled in " + token)
require("#if defined(DSRRL_V290_FULL_SIDECAR_DRAW_CENSUS)" in integrated,
        "draw census conditional compilation guard removed")
require("#if defined(DSRRL_V291_MATERIAL_STAGE_CENSUS)" in integrated,
        "material census conditional compilation guard removed")
require("test_v350_perf_source_contract.py" in builder,
        "no-source-regression build gate")
print("V350-RC1 PERF SOURCE PASS: exact parser SHA lifetime cache; nine cache RMWs telemetry gated; V290/V291 draw census OFF; no QPC profiling; single addon")
