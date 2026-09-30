#!/usr/bin/env python3
import argparse
from pathlib import Path
import sys

def fail(msg):
    raise RuntimeError(msg)

def require(text, needle, label):
    if needle not in text:
        fail(f"{label}: missing {needle!r}")

def require_before(text, first, second, label):
    a=text.find(first)
    b=text.find(second)
    if a < 0 or b < 0 or a >= b:
        fail(f"{label}: expected {first!r} before {second!r}")

def function_body(text, signature, next_signature=None):
    start=text.find(signature)
    if start < 0:
        fail(f"missing function {signature!r}")
    if next_signature is None:
        return text[start:]
    end=text.find(next_signature,start+len(signature))
    if end < 0:
        fail(f"missing boundary {next_signature!r}")
    return text[start:end]

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--source-dir",required=True)
    ns=ap.parse_args()
    root=Path(ns.source_dir).resolve()

    integrated=(root/"integrated/integrated_addon.cpp").read_text(encoding="utf-8")
    feature_h=(root/"include/dsrrl/core/feature_registry.hpp").read_text(encoding="utf-8")
    feature_cpp=(root/"src/feature_registry.cpp").read_text(encoding="utf-8")
    flver_cpp=(root/"src/runtime/flver_engine_hooks.cpp").read_text(encoding="utf-8")
    flver_registry=(root/"src/runtime/flver_identity_registry.cpp").read_text(encoding="utf-8")
    pmetal=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
    resources=(root/"src/runtime/material_resource_draw_runtime.cpp").read_text(encoding="utf-8")
    hemdir3_mode=(root/"src/runtime/hemdir3_mode_transport.cpp").read_text(encoding="utf-8")

    # Feature flags are queried from render-hot paths. Keep reads lock-free,
    # while snapshot/set may retain coherent writer serialization.
    require(feature_h,"std::atomic<operator_mask> enabled_mask_","lock-free feature mask ABI")
    enabled_body=function_body(
        feature_cpp,
        "feature_registry::enabled(",
        "feature_registry::all_disabled(")
    require(enabled_body,"enabled_mask_.load(","lock-free feature read")
    if "lock_guard" in enabled_body or "mutex_" in enabled_body:
        fail("feature_registry::enabled regressed to a mutex-backed hot read")

    # One-entry FLVER identity caching is insufficient for interleaved model
    # selectors. Global epoch still invalidates the whole TLS table on parse/
    # destroy, preserving the old fail-open invalidation semantics.
    require(flver_registry,"k_lookup_tls_cache_size = 256u","interleaved FLVER TLS cache")
    require(flver_registry,"thread_local std::array<","FLVER TLS cache storage")
    lookup_body=function_body(
        flver_registry,
        "bool flver_identity_lookup(",
        "bool flver_identity_enrich_owner(")
    require_before(
        lookup_body,
        "if(cached.model==model",
        "std::lock_guard<std::mutex> lock(g_mutex)",
        "FLVER TLS must precede global map lock")

    # PointLight bypass must remove the actual clustered builder detour, not
    # merely make its observer a no-op.
    require(flver_cpp,"bool install(bool enable_clustered_builder) noexcept","optional clustered builder ABI")
    require(flver_cpp,"if(enable_clustered_builder){","clustered builder preparation gate")
    require(flver_cpp,"g_state.builder_armed=enable_clustered_builder;","clustered builder attestation")
    require(integrated,
        "flver_identity_transport::install(\n            k_pointlight_drawtime_runtime_enabled)",
        "integrated clustered-builder policy")
    require(integrated,
        "if (k_pointlight_drawtime_runtime_enabled) {\n            clustered_pnts =",
        "PointLight create-time analysis bypass")
    require(integrated,
        "if (k_pointlight_drawtime_runtime_enabled &&\n        clustered_pnts_candidate)",
        "PointLight post-A1 registration bypass")

    # Disabled islands must not keep receiver/materialization work alive.
    require(integrated,
        "g_core.features().enabled(\n                dsrrl::core::operator_id::subsurface) &&\n            dsrrl::runtime::\n                subsurface_receiver_observe_pipeline",
        "Subsurface init-route feature gate")
    require(integrated,
        "g_core.features().enabled(\n                dsrrl::core::operator_id::hemdir3) &&\n            dsrrl::runtime::\n                hemdir3_receiver_observe_pipeline",
        "HemDir3 init-route feature gate")
    require(integrated,
        "if (g_core.features().enabled(\n                dsrrl::core::operator_id::upper_lower)) {\n            std::vector<std::uint8_t> ul_payload;",
        "UpperLower create materializer gate")
    require(integrated,
        "if (g_core.features().enabled(\n                dsrrl::core::operator_id::hemdir3)) {\n            std::vector<std::uint8_t> h3_payload;",
        "HemDir3 create materializer gate")

    selector_body=function_body(
        hemdir3_mode,
        "void selector_begin(",
        "bool snapshot(")
    require_before(
        selector_body,
        "if (!g_state.lt5_hook_armed",
        "telemetry::hot_count(g_selector_begin)",
        "disabled HemDir3 selector no-op")

    # Create-time A1 replacement is already live in the host pipeline; if no
    # draw-specific route is present, do not enter semantic join/replay.
    require(integrated,"constexpr std::uint8_t k_dynamic_draw_route_mask","dynamic route mask")
    if integrated.count(
        "if ((route_mask & k_dynamic_draw_route_mask) == 0u)") < 2:
        fail("both draw and draw_indexed must fast-skip A1-only routes")

    # MotionBlur active bridge is compute-only under current policy. Ordinary
    # PS/VS binds must not serialize through its registry mutex.
    motion_start=integrated.find("namespace motion_blur_camera_fallback_disable")
    motion_bind=integrated.find("void on_bind_pipeline(",motion_start)
    motion_end=integrated.find("void on_destroy_pipeline(",motion_bind)
    if min(motion_start,motion_bind,motion_end)<0:
        fail("MotionBlur bind boundaries missing")
    motion_body=integrated[motion_bind:motion_end]
    require_before(
        motion_body,
        "if (!compute_bound",
        "std::lock_guard<std::mutex> lock(",
        "MotionBlur stage prefilter")

    # P_Metal is route 345 only. Exact-material rejection must precede any
    # feature-registry reads on ordinary MR draws.
    pmetal_prepare=function_body(
        pmetal,
        "pmetal_envspec_draw_runtime::prepare(",
        "pmetal_envspec_draw_runtime::release(")
    require_before(
        pmetal_prepare,
        "if (!exact_pmetal_material(",
        "core_.features().enabled(",
        "PMetal exact-route prefilter")

    # Generic resource bridges all require exact owner authority. Receiver-only
    # draws must not do PSGetShaderResources/COM retains or texture lookups.
    resource_prepare=function_body(
        resources,
        "prepare_draw_requests(",
        "prepare_fixed_pointlight_material_requests(")
    require_before(
        resource_prepare,
        "if (!exact_material)",
        "context->PSGetShaderResources(",
        "resource exact-owner prefilter")
    require_before(
        resource_prepare,
        "if (!spec_rgb_consumer_ready",
        "context->PSGetShaderResources(",
        "resource impossible-receiver prefilter")

    # Current active operator set is owner-gated. Preserve an escape hatch only
    # for explicitly enabled U/L/HemDir3/Subsurface if policy changes later.
    owner_reject=integrated.find("if (!owner_ok) {")
    owner_join=integrated.find("hot_count(g_draw_joins);",owner_reject)
    if owner_reject<0 or owner_join<0:
        fail("owner-reject draw boundary missing")
    owner_body=integrated[owner_reject:owner_join]
    require(owner_body,"const bool ownerless_island_enabled =","ownerless route policy")
    require(owner_body,"return ownerless_island_enabled;","ownerless draw early exit")

    print("Active-islands hot-path audit: PASS")
    print("  feature_reads=atomic")
    print("  flver_identity=256-entry TLS before global map lock")
    print("  pointlight_bypass=no producer hooks/no clustered builder detour/no create-time PL census")
    print("  disabled_islands=no HemDir3/Subsurface routes; no U/L/HemDir3 create materializers")
    print("  motion_blur=compute-stage prefilter before mutex")
    print("  pmetal=exact material/route prefilter before feature reads")
    print("  resources=exact-owner/impossible-receiver prefilter before PSGetShaderResources")
    print("  ownerless_draws=stop before batch preparation under current policy")
    return 0

if __name__=="__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"Active-islands hot-path audit: FAIL: {exc}",file=sys.stderr)
        raise SystemExit(1)
