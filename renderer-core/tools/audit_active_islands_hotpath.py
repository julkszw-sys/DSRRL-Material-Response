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
    draw_tx=(root/"src/runtime/draw_state_transaction.cpp").read_text(encoding="utf-8")
    owner_selection=(root/"src/runtime/material_owner_selection.cpp").read_text(encoding="utf-8")
    owner_producer=(root/"src/runtime/material_owner_producer.cpp").read_text(encoding="utf-8")
    mr_draw=(root/"src/runtime/material_response_draw_transaction.cpp").read_text(encoding="utf-8")
    envspec=(root/"src/runtime/envspec_resource_runtime.cpp").read_text(encoding="utf-8")
    material_resource=(root/"src/runtime/material_resource_draw_runtime.cpp").read_text(encoding="utf-8")

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
    parse_body=function_body(
        flver_registry,
        "flver_identity_observe_parse(",
        "flver_identity_observe_destroy(")
    require(parse_body,
        "if (found == g_by_model.end())",
        "FLVER unrelated-streaming insertion branch")
    insert_branch_start=parse_body.find(
        "if (found == g_by_model.end())")
    change_branch=parse_body.find(
        "} else if (found->second != sha)",
        insert_branch_start)
    if insert_branch_start<0 or change_branch<0:
        fail("FLVER parse insertion/change branches missing")
    if "g_epoch.fetch_add" in parse_body[insert_branch_start:change_branch]:
        fail("new unrelated FLVER still globally invalidates lookup TLS")

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
    require(flver_cpp,
        "if(g_state.builder_armed)\n  clustered_pnts_selector_event_bridge(",
        "clustered selector bridge follows builder arm state")
    require(flver_cpp,
        "r15,\n     g_state.builder_armed);",
        "fixed selector bridge follows PointLight runtime policy")

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

    # The diffuse-v1 classifier returns the receiver family itself. Do not
    # parse/hash the same shader twice for stable and Lerp branches, and do
    # not build MR+U/L composed variants while U/L is disabled.
    if integrated.count("materialize_ptde_diffuse_response_v1(") != 2:
        fail("diffuse-v1 materializer must run once in on_create_pipeline plus once in the subsurface helper")
    require(integrated,
        "const auto &lerp_mr = mr;\n        const auto &lerp_mr_payload = mr_payload;",
        "single-pass stable/Lerp MR classification")
    if integrated.count(
        "if (g_core.features().enabled(\n                    dsrrl::core::operator_id::upper_lower)) {") < 2:
        fail("stable and Lerp MR+U/L construction must both be feature-gated")

    selector_body=function_body(
        hemdir3_mode,
        "void selector_begin(",
        "bool snapshot(")
    require_before(
        selector_body,
        "if (!g_state.lt5_hook_armed",
        "telemetry::hot_count(g_selector_begin)",
        "disabled HemDir3 selector no-op")

    # Central bind routing must retain a realistic per-thread working set.
    # Adding an unrelated new pipeline must not globally invalidate already
    # cached route verdicts; destroy/change remains the invalidation boundary.
    require(integrated,
        "constexpr std::size_t k_integrated_route_cache_size = 256u;",
        "integrated route TLS capacity")
    remember_start=integrated.find("void remember_integrated_draw_route(")
    remember_end=integrated.find("void forget_integrated_draw_route(",remember_start)
    if remember_start<0 or remember_end<0:
        fail("integrated route remember boundary missing")
    remember_body=integrated[remember_start:remember_end]
    require(remember_body,
        "g_integrated_draw_routes.emplace(",
        "new route insertion without global invalidation")
    new_insert=remember_body.find(
        "found ==\n                g_integrated_draw_routes.end()")
    emplace=remember_body.find(
        "g_integrated_draw_routes.emplace(",
        new_insert)
    if new_insert<0 or emplace<0:
        fail("new integrated route insertion branch missing")
    insertion_branch=remember_body[new_insert:emplace]
    if "g_integrated_draw_route_epoch.fetch_add" in insertion_branch:
        fail("new unrelated pipeline still globally invalidates route TLS")

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

    # Constant-buffer window preservation still requires
    # ID3D11DeviceContext1, but QueryInterface must be cold-path only. The
    # replay begin path borrows a cached Context1 pointer and device teardown
    # invalidates the cache.
    draw_begin=function_body(
        draw_tx,
        "draw_state_transaction_runtime::begin(",
        "draw_state_transaction_runtime::restore(")
    if "QueryInterface(" in draw_begin:
        fail("draw replay regressed to per-draw ID3D11DeviceContext1 QueryInterface")
    require(draw_begin,
        "ctx1 = context1_for(ctx);",
        "draw replay Context1 cache use")
    context1_start=draw_tx.find(
        "draw_state_transaction_runtime::context1_for(")
    context1_end=draw_tx.find(
        "release_context1_cache() noexcept",
        context1_start)
    if context1_start<0 or context1_end<0:
        fail("Context1 cache function boundaries missing")
    context1_body=draw_tx[context1_start:context1_end]
    require(context1_body,
        "g_context1_tls_cache",
        "Context1 TLS fast path")
    require_before(
        context1_body,
        "if (tls.owner == this",
        "context->QueryInterface(",
        "Context1 TLS before QueryInterface")
    require(draw_tx,
        "draw_state_transaction_runtime::on_destroy_device(",
        "Context1 device-lifetime invalidation")
    require(integrated,
        "g_draw_transactions.on_destroy_device(device);",
        "integrated Context1 cache teardown")
    if draw_tx.count("cmd_list->get_native()") != 1:
        fail("draw replay must resolve native D3D11 context once in begin and reuse it through draw/restore")
    require(draw_tx,
        "state.context = ctx;",
        "draw replay retained native context")
    if draw_tx.count("auto *ctx = state.context;") < 3:
        fail("draw replay/restore do not consistently reuse retained native context")

    # Owner/material caches use the already-computed 64-bit FLVER token only
    # as bucket entropy. Positive cache hits must still compare the full SHA-256.
    require(owner_selection,
        "k_owner_auth_cache_slots = 256u",
        "owner auth TLS working set")
    require(owner_selection,
        "owner_cache_entropy(identity)",
        "owner auth cheap cache entropy")
    require(owner_selection,
        "cached.flver_sha256 ==\n            identity.flver_sha256",
        "owner auth full SHA hit gate")
    require(owner_producer,
        "k_owner_material_cache_slots = 256u",
        "owner material TLS working set")
    require(owner_producer,
        "owner_material_cache_entropy(observation)",
        "owner material cheap cache entropy")
    require(owner_producer,
        "cached.flver_sha256 ==\n            observation.flver_sha256",
        "owner material full SHA hit gate")

    # MR replacement registration is append-only for an existing
    # (bank,receiver) namespace. Streaming an unrelated shader must not
    # invalidate every TLS replacement hit; destructive teardown still bumps
    # the shared epoch.
    reg_start=mr_draw.find("bool material_response_draw_runtime::register_replacement_record(")
    rel_start=mr_draw.find("void material_response_draw_runtime::release_resources()",reg_start)
    if reg_start<0 or rel_start<0:
        fail("MR replacement registration boundaries missing")
    reg_body=mr_draw[reg_start:rel_start]
    require(reg_body,
        "bank.emplace(",
        "MR append-only replacement registration")
    if "resource_epoch_.fetch_add" in reg_body:
        fail("unrelated MR replacement streaming still globally invalidates draw TLS")
    release_body=function_body(
        mr_draw,
        "void material_response_draw_runtime::release_resources()",
        "void material_response_draw_runtime::on_init_device(")
    require(release_body,
        "resource_epoch_.fetch_add(",
        "MR destructive cache invalidation")

    # EnvSpec snapshot cache keys already include stock SRV identity and hold
    # strong PTDE resource references. Appending an unrelated stock SRV must
    # not invalidate every existing snapshot; destroy remains the epoch cut.
    init_view=function_body(
        envspec,
        "void on_init_resource_view(",
        "void on_destroy_resource_view(")
    if "g_snapshot_epoch.fetch_add" in init_view:
        fail("unrelated EnvSpec SRV creation still globally invalidates snapshot TLS")
    destroy_view=function_body(
        envspec,
        "void on_destroy_resource_view(",
        "bool probe_for_native_view(")
    require(destroy_view,
        "g_snapshot_epoch.fetch_add(",
        "EnvSpec view-destroy snapshot invalidation")

    require(material_resource,
        "k_companion_tls_slots = 256u",
        "material resource TLS working set")
    require(integrated,
        "decision.route_index == 345u &&\n        g_pmetal_envspec.prepare(",
        "P_Metal route prefilter before island prepare")
    require(material_resource,
        "(k_companion_tls_slots - 1u)",
        "material resource power-of-two TLS index")

    print("Active-islands hot-path audit: PASS")
    print("  feature_reads=atomic")
    print("  flver_identity=256-entry TLS before global map lock")
    print("  pointlight_bypass=no producer hooks/no clustered builder detour/no create-time PL census")
    print("  disabled_islands=no HemDir3/Subsurface routes; no U/L/HemDir3 create materializers")
    print("  motion_blur=compute-stage prefilter before mutex")
    print("  pmetal=exact material/route prefilter before feature reads")
    print("  resources=exact-owner/impossible-receiver prefilter before PSGetShaderResources")
    print("  ownerless_draws=stop before batch preparation under current policy")
    print("  draw_replay=Context1 TLS cache; QueryInterface cold-path only")
    print("  material_owner=256-entry TLS caches use optional legacy token or cheap SHA fold; full SHA authority retained")
    print("  mr_replacement=append-only streaming preserves unrelated TLS entries; teardown invalidates")
    print("  envspec=unrelated SRV creation preserves snapshot TLS; destroy invalidates")
    print("  material_resources=256-entry TLS companion working set; negative-cache invalidation retained")
    return 0

if __name__=="__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"Active-islands hot-path audit: FAIL: {exc}",file=sys.stderr)
        raise SystemExit(1)
