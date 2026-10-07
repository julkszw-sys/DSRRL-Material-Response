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
    pmetal_source=(root/"src/runtime/pmetal_env_source_runtime.cpp").read_text(encoding="utf-8")
    clustered=(root/"src/runtime/clustered_pnts_draw_runtime.cpp").read_text(encoding="utf-8")
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

    # Interleaved model selectors must remain on a bounded, set-associative
    # TLS fast path. Global epoch still invalidates cached verdicts on model
    # replacement/destroy, preserving fail-open pointer-reuse semantics.
    require(flver_registry,"k_lookup_tls_cache_sets = 128u","FLVER TLS cache sets")
    require(flver_registry,"k_lookup_tls_cache_ways = 4u","4-way interleaved FLVER TLS cache")
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
        "lookup_tls_cache_hit(",
        "std::lock_guard<std::mutex> lock(g_mutex)",
        "FLVER TLS must precede global map lock")
    require(lookup_body,
        "g_mutex_fallbacks",
        "FLVER global-lock fallback telemetry")

    # R43 PARAM-source Clustered architecture. The stock receiver/material
    # path remains DSR-owned; only the exact source producer is bridged.
    require(flver_cpp,
        "bool install(\n    bool enable_clustered_builder,\n    bool enable_upper_lower_selector,\n    bool enable_hemdir3_selector) noexcept",
        "optional clustered builder ABI retained")
    require(integrated,
        "flver_identity_transport::install(\n            false, // clustered PntS is source-only; no builder/receiver bridge",
        "clustered FLVER builder detour disabled")
    require(integrated,
        "constexpr bool k_clustered_pointlight_receiver_runtime_enabled = false;",
        "clustered receiver runtime disabled")
    require(integrated,
        "g_clustered_pointlight_selection_transport_active.store(\n        false,",
        "clustered selection transport disabled")
    require(clustered,
        "k_clustered_source_override_rva = 0xB7E02u;",
        "clustered source-only producer cut")
    require(clustered,
        "capture_clustered_live_drawparam_source(",
        "clustered live DrawParam source decoder")
    require(clustered,
        "g_source_dsr_only_fail_open",
        "clustered DSR-only row fail-open")
    callback_start=clustered.find(
        "float __fastcall clustered_source_override_callback(")
    callback_end=clustered.find(
        "bool build_clustered_source_override_stub(",
        callback_start)
    if callback_start<0 or callback_end<=callback_start:
        fail("clustered source callback boundaries missing")
    callback_body=clustered[callback_start:callback_end]
    if "pointlight_ptde_source::capture(" in callback_body:
        fail("clustered active callback regressed to embedded donor lookup")
    for forbidden in (
        "evaluate_direct_pointlight_material_identity(",
        "build_clustered_sidecar_v1(",
        "select_first_four_exact(",
    ):
        if forbidden in callback_body:
            fail("clustered source callback contains receiver/material work: "+forbidden)
    require(flver_cpp,
        "fixed_pointlight_selector_event_bridge(owner);",
        "fixed selector source association retained")

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
        "if (!exact_pmetal_envspec_candidate(",
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

    # Current generic operator set is owner-gated. PointLight has a dedicated
    # exact-route fast path with its own owner rejection, so inspect only the
    # generic observe_draw_identity body here. Preserve an escape hatch only
    # for explicitly enabled U/L/HemDir3/Subsurface if policy changes later.
    generic_observe=function_body(
        integrated,
        "bool observe_draw_identity(",
        "bool prepare_island_batch(")
    owner_reject=generic_observe.find("if (!owner_ok) {")
    owner_join=generic_observe.find("hot_count(g_draw_joins);",owner_reject)
    if owner_reject<0 or owner_join<0:
        fail("generic owner-reject draw boundary missing")
    owner_body=generic_observe[owner_reject:owner_join]
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
    begin_start=draw_tx.find(
        "bool draw_state_transaction_runtime::begin(")
    capture_cut=draw_tx.find(
        "bool draw_state_transaction_runtime::capture_only(",
        begin_start)
    if begin_start<0 or capture_cut<0:
        fail("normal draw transaction/native-context audit boundaries missing")
    normal_tx=draw_tx[begin_start:capture_cut]
    if normal_tx.count("cmd_list->get_native()") != 1:
        fail("normal draw transaction must resolve native D3D11 context once in begin and reuse it through replay/restore")
    require(normal_tx,
        "state.context = ctx;",
        "draw replay retained native context")
    if normal_tx.count("auto *ctx = state.context;") < 3:
        fail("draw replay/restore do not consistently reuse retained native context")

    # Owner/material caches use the already-computed 64-bit FLVER token only
    # as bucket entropy. Positive cache hits must still compare the full SHA-256.
    require(owner_selection,
        "k_owner_auth_cache_sets = 128u",
        "owner auth TLS cache sets")
    require(owner_selection,
        "k_owner_auth_cache_ways = 4u",
        "owner auth 4-way TLS working set")
    require(owner_selection,
        "owner_cache_entropy(identity)",
        "owner auth cheap cache entropy")
    require(owner_selection,
        "entry.flver_sha256 ==\n                identity.flver_sha256",
        "owner auth full SHA hit gate")
    require(owner_producer,
        "k_owner_material_cache_sets = 128u",
        "owner material TLS cache sets")
    require(owner_producer,
        "k_owner_material_cache_ways = 4u",
        "owner material 4-way TLS working set")
    require(owner_producer,
        "owner_material_cache_entropy(observation)",
        "owner material cheap cache entropy")
    require(owner_producer,
        "entry.flver_sha256 ==\n                observation.flver_sha256",
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
        "exact_pmetal_envspec_candidate(",
        "P_Metal exact prefilter before island prepare")
    require(integrated,
        "if (!g_pmetal_envspec.prepare(",
        "P_Metal atomic stock fail-open before generic MR")
    require(material_resource,
        "(k_companion_tls_slots - 1u)",
        "material resource power-of-two TLS index")

    # Full draw-time falsifier must preserve only true create-time islands.
    # It must not merely make draw handlers return early: the draw callbacks
    # themselves and all dynamic transports are absent from the profile.
    require(integrated,
        "constexpr bool k_drawtime_islands_runtime_enabled = false;",
        "draw-time bypass compile policy")
    create_start=integrated.find(
        "bool on_create_pipeline(\n    reshade::api::device *device")
    create_end=integrated.find(
        "void on_init_pipeline(\n    reshade::api::device *device",
        create_start)
    if create_start<0 or create_end<0:
        fail("integrated create-pipeline boundaries missing")
    create_body=integrated[create_start:create_end]
    require_before(
        create_body,
        "if (!k_drawtime_islands_runtime_enabled)",
        "const auto *pixel_shader =",
        "draw-time bypass before dynamic create-time census")
    require(create_body,
        "g_a1_bridge.on_create_pipeline(",
        "A1 preserved in draw-time bypass")
    require(create_body,
        "motion_blur_camera_fallback_disable::\n                on_create_pipeline(",
        "MotionBlur preserved in draw-time bypass")

    register_body=function_body(
        integrated,
        "void register_events()",
        "void unregister_events()")
    require(register_body,
        "if (k_drawtime_islands_runtime_enabled &&\n        k_draw_callbacks_runtime_enabled) {\n        reshade::register_event<reshade::addon_event::draw>(on_draw);",
        "draw callback omitted in full/transport-only bypass")
    require(register_body,
        "reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);",
        "indexed draw callback conditional registration")
    draw_gate=register_body.find(
        "if (k_drawtime_islands_runtime_enabled &&\n        k_draw_callbacks_runtime_enabled) {")
    draw_gate_end=register_body.find(
        "}\n}",
        draw_gate)
    if draw_gate<0 or draw_gate_end<0:
        fail("draw-time event gate boundary missing")
    gated_events=register_body[draw_gate:draw_gate_end]
    require(gated_events,
        "addon_event::present>(on_present)",
        "present callback omitted with draw-time runtime")

    init_marker=integrated.find(
        "if (!k_drawtime_islands_runtime_enabled) {\n        reshade::log::message(")
    dynamic_events=integrated.find(
        "g_material_resources.register_events()",
        init_marker)
    if init_marker<0 or dynamic_events<0 or not init_marker<dynamic_events:
        fail("AddonInit draw-time bypass does not precede dynamic resource transport")
    init_bypass=integrated[init_marker:dynamic_events]
    require(init_bypass,
        "return true;",
        "draw-time bypass exits before dynamic hook installation")

    # Bloom Q8 has no authorized production writer/consumer yet. Avoid even
    # the fixed 1024x720 texture/RTV/SRV allocation outside explicit telemetry.
    init_device_body=function_body(
        integrated,
        "void on_init_device(reshade::api::device *device)",
        "void on_destroy_device(reshade::api::device *device)")
    require_before(
        init_device_body,
        "if (g_hot_telemetry_enabled)",
        "g_bloom_scene_sidecar.on_init_device(device);",
        "Bloom Q8 diagnostic-only resource allocation")

    if integrated.count(
        "if (g_hot_telemetry_enabled &&\n        dsrrl::runtime::bloom_fx_draw_transport::\n            active_draw_scope())") != 2:
        fail("Bloom FX active-draw scope must be skipped on both production draw paths")

    guard_start=integrated.find("struct draw_semantic_selection_guard")
    guard_end=integrated.find("void observe_equipment_specrgb_carrier(",guard_start)
    if guard_start<0 or guard_end<0:
        fail("draw semantic selection guard boundary missing")
    guard_body=integrated[guard_start:guard_end]
    require_before(
        guard_body,
        "if (!g_any_draw_selection_transport_active.load(",
        "g_upper_lower_selection_transport_active.load(",
        "aggregate selection-transport fast reject")
    for token in (
        "g_upper_lower_selection_transport_active.load(",
        "g_hemdir3_selection_transport_active.load(",
        "g_fixed_pointlight_selection_transport_active.load(",
        "g_clustered_pointlight_selection_transport_active.load(",
    ):
        require(guard_body,token,"selection drain installed-transport gate")
    if "g_upper_lower.consume_draw_selection();\n        g_fixed_pointlight.consume_draw_selection();" in guard_body:
        fail("selection guard regressed to unconditional disabled-island drains")

    # Two runtime bisect profiles isolate transport outside draw from
    # per-draw preparation and from the final replay/restore transaction.
    require(integrated,
        "constexpr bool k_draw_callbacks_runtime_enabled = false;",
        "transport-only callback bypass policy")
    require(integrated,
        "constexpr bool k_draw_replay_runtime_enabled = false;",
        "no-replay policy")

    draw_start=integrated.find(
        "bool on_draw(\n    reshade::api::command_list *cmd_list")
    indexed_start=integrated.find(
        "bool on_draw_indexed(\n    reshade::api::command_list *cmd_list",
        draw_start)
    present_start=integrated.find(
        "void on_present(",
        indexed_start)
    if draw_start<0 or indexed_start<0 or present_start<0:
        fail("draw bisect function boundaries missing")
    draw_body=integrated[draw_start:indexed_start]
    indexed_body=integrated[indexed_start:present_start]
    for body,label,dispatch_token in (
        (draw_body,"draw","dispatch_island_draw_batch("),
        (indexed_body,"draw_indexed","dispatch_island_draw_indexed_batch("),
    ):
        gate=body.find("if (!k_draw_replay_runtime_enabled)")
        release=body.find("release_prepared_island_batch(prepared);",gate)
        dispatch=body.find(dispatch_token)
        if gate<0 or release<0 or dispatch<0 or not gate<release<dispatch:
            fail(f"{label} no-replay gate is not immediately before dispatch")
        if "return false;" not in body[release:dispatch]:
            fail(f"{label} no-replay gate does not stop before replay")

    init_log_start=integrated.find(
        "if (k_drawtime_islands_runtime_enabled &&\n        !k_draw_callbacks_runtime_enabled)")
    init_log_end=integrated.find(
        "} else if (k_drawtime_islands_runtime_enabled &&\n               !k_draw_replay_runtime_enabled)",
        init_log_start)
    dynamic_install=integrated.find(
        "g_material_resources.register_events()",
        init_log_end)
    if init_log_start<0 or init_log_end<0 or dynamic_install<0:
        fail("transport-only startup marker missing")
    if "return true;" in integrated[init_log_start:init_log_end]:
        fail("transport-only profile incorrectly exits before dynamic transport installation")

    require(integrated,
        "constexpr bool k_empty_draw_callback_bisect = true;",
        "empty draw callback bisect compile policy")
    for sig,label in (
        ("bool on_draw(\n    reshade::api::command_list *cmd_list","draw"),
        ("bool on_draw_indexed(\n    reshade::api::command_list *cmd_list","draw_indexed"),
    ):
        start=integrated.find(sig)
        if start<0:
            fail(f"{label} boundary missing")
        body=integrated[start:start+1400]
        require_before(
            body,
            "if (k_empty_draw_callback_bisect)\n        return false;",
            "g_hot_telemetry_enabled",
            f"{label} empty-callback bisect before all DSRRL draw work")

    require(integrated,
        "constexpr bool k_state_transaction_only_bisect = true;",
        "state-only replay bisect compile policy")
    require(integrated,
        "constexpr bool k_raw_draw_replay_bisect = true;",
        "raw-draw replay bisect compile policy")
    require(integrated,
        "g_draw_transactions.mutate_restore_only(",
        "state-only begin/restore bisect")
    require(integrated,
        "g_draw_transactions.raw_replay_draw(",
        "raw non-indexed draw bisect")
    require(integrated,
        "g_draw_transactions.raw_replay_draw_indexed(",
        "raw indexed draw bisect")
    require(integrated,
        "thread_local bool g_raw_draw_replay_recursing = false;",
        "raw replay recursion guard")
    require(draw_tx,
        "bool draw_state_transaction_runtime::mutate_restore_only(",
        "state-only helper")
    require(draw_tx,
        "bool draw_state_transaction_runtime::raw_replay_draw(",
        "raw draw helper")
    require(draw_tx,
        "bool draw_state_transaction_runtime::raw_replay_draw_indexed(",
        "raw indexed draw helper")

    # Final low-level split after owner runtime showed both state-only and
    # raw-draw replay independently lose FPS.
    for needle,label in (
        ("constexpr bool k_state_capture_only_bisect = true;","state capture-only compile policy"),
        ("constexpr bool k_native_state_mutate_restore_only_bisect = true;","native mutate/restore-only compile policy"),
        ("constexpr bool k_core_transaction_only_bisect = true;","core transaction-only compile policy"),
        ("constexpr bool k_raw_native_draw_reentry_min_bisect = true;","minimal raw native draw reentry compile policy"),
        ("g_draw_transactions.capture_only(","state capture-only dispatch"),
        ("g_draw_transactions.native_mutate_restore_only(","native mutate/restore-only dispatch"),
        ("g_draw_transactions.core_transaction_only(","core transaction-only dispatch"),
    ):
        require(integrated,needle,label)
    for needle,label in (
        ("bool draw_state_transaction_runtime::capture_only(","state capture-only helper"),
        ("bool draw_state_transaction_runtime::native_mutate_restore_only(","native mutate/restore-only helper"),
        ("bool draw_state_transaction_runtime::core_transaction_only(","core transaction-only helper"),
    ):
        require(draw_tx,needle,label)

    capture_start=draw_tx.find(
        "bool draw_state_transaction_runtime::capture_only(")
    native_start=draw_tx.find(
        "bool draw_state_transaction_runtime::native_mutate_restore_only(",
        capture_start)
    core_start=draw_tx.find(
        "bool draw_state_transaction_runtime::core_transaction_only(",
        native_start)
    raw_start=draw_tx.find(
        "bool draw_state_transaction_runtime::raw_replay_draw(",
        core_start)
    if min(capture_start,native_start,core_start,raw_start)<0:
        fail("low-level bisect helper boundaries missing")
    capture_body=draw_tx[capture_start:native_start]
    native_body=draw_tx[native_start:core_start]
    core_body=draw_tx[core_start:raw_start]
    if "PSSet" in capture_body or "core_.transactions().begin" in capture_body:
        fail("capture-only bisect performs mutation/core transaction")
    if "ctx->Draw" in native_body or "core_.transactions().begin" in native_body:
        fail("native mutate/restore-only bisect performs replay/core transaction")
    if "PSGet" in core_body or "PSSet" in core_body or "ctx->Draw" in core_body:
        fail("core transaction-only bisect performs native state/replay work")
    require(core_body,"core_.transactions().begin(","core-only begin")
    require(core_body,"core_.transactions().restore(command)","core-only restore")

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
    print("  drawtime_falsifier=no draw callbacks/no FLVER-texture-resource-PMetal-PointLight transports; A1+MotionBlur create-time retained")
    print("  transport_only=dynamic transports installed, draw/draw_indexed/present callbacks omitted")
    print("  no_replay=full draw routing/preparation active, release before dispatch/replay/restore")
    print("  empty_draw_callback=ReShade draw event dispatch/function call only; immediate return before DSRRL draw logic")
    print("  state_only=full prepare plus snapshot/mutate/restore; original stock draw proceeds")
    print("  raw_draw_only=manual stock Draw/DrawIndexed replay with recursion guard; no DSRRL mutation")
    print("  state_capture_only=native Get/capture only; no Set/core/replay/restore")
    print("  native_state_only=native capture+Set+restore; no core/replay")
    print("  core_transaction_only=renderer-core begin+restore only; no native PS state/replay")
    print("  raw_native_reentry_min=minimal callback native Draw/DrawIndexed reentry")
    print("  bloom_q8=no production resource allocation without telemetry authority")
    print("  bloom_fx=no production per-draw scope check outside telemetry")
    print("  selection_guard=only installed producer transports drain draw-scoped TLS")
    return 0

if __name__=="__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"Active-islands hot-path audit: FAIL: {exc}",file=sys.stderr)
        raise SystemExit(1)
