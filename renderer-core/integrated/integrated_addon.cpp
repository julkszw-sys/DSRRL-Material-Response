#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/a1_create_pipeline_bridge.hpp"
#include "dsrrl/runtime/flver_identity_transport.hpp"
#include "dsrrl/runtime/flver_identity_registry.hpp"
#include "dsrrl/runtime/material_owner_selection.hpp"
#include "dsrrl/runtime/material_response_draw_transaction.hpp"
#include "dsrrl/runtime/material_resource_draw_runtime.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include "dsrrl/runtime/envspec_resource_runtime.hpp"
#include "dsrrl/runtime/bloom_scene_sidecar_runtime.hpp"
#include "dsrrl/runtime/bloom_fx_draw_transport.hpp"
#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"
#include "dsrrl/runtime/pmetal_native_draw_bridge.hpp"
#include "dsrrl/runtime/pixel_srv_shadow.hpp"
#include "dsrrl/runtime/texture_identity_transport.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/runtime/stable_receiver_pipeline_registry.hpp"
#include "dsrrl/runtime/hemenvlerp_pipeline_registry.hpp"
#include "dsrrl/runtime/subsurface_pipeline_registry.hpp"
#include "dsrrl/runtime/subsurface_draw_runtime.hpp"
#include "dsrrl/runtime/upper_lower_draw_runtime.hpp"
#include "dsrrl/runtime/upper_lower_pipeline_registry.hpp"
#include "dsrrl/runtime/upper_lower_hemenv_draw_runtime.hpp"
#include "dsrrl/runtime/hemdir3_mode_transport.hpp"
#include "dsrrl/runtime/hemdir3_pipeline_registry.hpp"
#include "dsrrl/runtime/hemdir3_draw_runtime.hpp"
#include "dsrrl/runtime/fixed_pointlight_draw_runtime.hpp"
#include "dsrrl/runtime/fixed_pointlight_pipeline_runtime.hpp"
#include "dsrrl/runtime/clustered_pnts_draw_runtime.hpp"
#include "dsrrl/runtime/clustered_pnts_pipeline_runtime.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/material_response_seed.hpp"
#include "dsrrl/operators/material_response/material_response_diffuse_v1.hpp"
#include "dsrrl/operators/lightbank/hemdir3_b13_materializer.hpp"
#include "dsrrl/operators/lightbank/upper_lower_hemenv_materializer.hpp"
#include "dsrrl/operators/resource_bridges/spec_rgb_consumer_materializer.hpp"
#include "dsrrl/operators/resource_bridges/subsurface_plain_target_materializer.hpp"
#include "dsrrl/operators/resource_bridges/subsurface_route.hpp"
#include "dsrrl/operators/env_spec/pmetal_rgba_materializer.hpp"
#include "dsrrl/operators/env_spec/pmetal_rgba_lerp_materializer.hpp"
#include "dsrrl/operators/point_light/local_specular_receiver_registry.hpp"
#include "dsrrl/operators/point_light/local_specular_microfacet_windows.hpp"
#include "dsrrl/operators/point_light/fixed_local_specular_patch_plan.hpp"
#include "dsrrl/operators/point_light/fixed_local_specular_operand_contract.hpp"
#include "dsrrl/operators/point_light/fixed_local_specular_output_cut.hpp"
#include "dsrrl/operators/point_light/fixed_local_specular_island_plan.hpp"
#include "dsrrl/operators/point_light/fixed_local_specular_single_materializer.hpp"
#include "dsrrl/operators/point_light/clustered_pnts_direct_materializer.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include "dsrrl/operators/postprocess/motion_blur_velocity_authority.hpp"

#include <reshade.hpp>
#include <d3d11.h>

#if RESHADE_API_VERSION != 20
#error DSRRL Core+Islands requires ReShade Add-on API 20
#endif

#ifndef DSRRL_CORE_ISLANDS_VERSION
#error DSRRL_CORE_ISLANDS_VERSION must be supplied by integrated CMake
#endif

#ifndef DSRRL_CORE_ISLANDS_PRODUCT_LINE
#error DSRRL_CORE_ISLANDS_PRODUCT_LINE must be supplied by integrated CMake
#endif

#ifndef DSRRL_SOURCE_COMMIT
#define DSRRL_SOURCE_COMMIT "unknown"
#endif

#ifndef DSRRL_BUILD_FLAVOR
#define DSRRL_BUILD_FLAVOR "default"
#endif

#ifdef DSRRL_DRAWTIME_ISLANDS_BYPASS
constexpr bool k_drawtime_islands_runtime_enabled = false;
#else
constexpr bool k_drawtime_islands_runtime_enabled = true;
#endif

#ifdef DSRRL_DRAW_CALLBACKS_BYPASS
constexpr bool k_draw_callbacks_runtime_enabled = false;
#else
constexpr bool k_draw_callbacks_runtime_enabled = true;
#endif

#ifdef DSRRL_DRAW_REPLAY_BYPASS
constexpr bool k_draw_replay_runtime_enabled = false;
#else
constexpr bool k_draw_replay_runtime_enabled = true;
#endif

#ifdef DSRRL_EMPTY_DRAW_CALLBACK_BISECT
constexpr bool k_empty_draw_callback_bisect = true;
#else
constexpr bool k_empty_draw_callback_bisect = false;
#endif

#ifdef DSRRL_STATE_TRANSACTION_ONLY_BISECT
constexpr bool k_state_transaction_only_bisect = true;
#else
constexpr bool k_state_transaction_only_bisect = false;
#endif

#ifdef DSRRL_RAW_DRAW_REPLAY_BISECT
constexpr bool k_raw_draw_replay_bisect = true;
#else
constexpr bool k_raw_draw_replay_bisect = false;
#endif

#ifdef DSRRL_STATE_CAPTURE_ONLY_BISECT
constexpr bool k_state_capture_only_bisect = true;
#else
constexpr bool k_state_capture_only_bisect = false;
#endif

#ifdef DSRRL_NATIVE_STATE_MUTATE_RESTORE_ONLY_BISECT
constexpr bool k_native_state_mutate_restore_only_bisect = true;
#else
constexpr bool k_native_state_mutate_restore_only_bisect = false;
#endif

#ifdef DSRRL_CORE_TRANSACTION_ONLY_BISECT
constexpr bool k_core_transaction_only_bisect = true;
#else
constexpr bool k_core_transaction_only_bisect = false;
#endif

#ifdef DSRRL_RAW_NATIVE_DRAW_REENTRY_MIN_BISECT
constexpr bool k_raw_native_draw_reentry_min_bisect = true;
#else
constexpr bool k_raw_native_draw_reentry_min_bisect = false;
#endif

#ifdef DSRRL_ADDON_LOADED_ONLY_BISECT
constexpr bool k_addon_loaded_only_bisect = true;
#else
constexpr bool k_addon_loaded_only_bisect = false;
#endif

#ifdef DSRRL_DRAW_CALLBACK_ONLY_BISECT
constexpr bool k_draw_callback_only_bisect = true;
#else
constexpr bool k_draw_callback_only_bisect = false;
#endif

#ifdef DSRRL_PMETAL_SRV_SHADOW_EXPERIMENT
constexpr bool k_pmetal_srv_shadow_runtime_enabled = true;
#else
constexpr bool k_pmetal_srv_shadow_runtime_enabled = false;
#endif

#if defined(DSRRL_POINTLIGHT_DRAWTIME_BYPASS) || defined(DSRRL_DRAWTIME_ISLANDS_BYPASS)
constexpr bool k_pointlight_drawtime_runtime_enabled = false;
#else
constexpr bool k_pointlight_drawtime_runtime_enabled = true;
#endif

// R26: persistent Draw-family vtable ownership is runtime-falsified on the
// DSR/ReShade host. Use one current-native dispatch under the existing
// transaction + TLS recursion guard instead.
constexpr bool k_pmetal_native_draw_runtime_enabled = false;
constexpr bool k_pmetal_direct_current_native_dispatch = true;

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace {

dsrrl::core::renderer_core g_core;
dsrrl::runtime::a1_create_pipeline_bridge
    g_a1_bridge(g_core.features());
dsrrl::operators::material_response::material_response_island
    g_material_response;
dsrrl::runtime::draw_state_transaction_runtime
    g_draw_transactions(g_core);
dsrrl::runtime::material_response_draw_runtime
    g_mr_draw_runtime(g_draw_transactions);
dsrrl::runtime::material_resource_draw_runtime
    g_material_resources(g_core);
dsrrl::runtime::envspec_resource_runtime
    g_envspec_resources;
dsrrl::runtime::bloom_scene_sidecar_runtime
    g_bloom_scene_sidecar;
dsrrl::runtime::upper_lower_draw_runtime
    g_upper_lower(g_core);
dsrrl::runtime::pmetal_env_source_runtime
    g_pmetal_source;
dsrrl::runtime::subsurface_draw_runtime
    g_subsurface(
        g_core,
        g_mr_draw_runtime,
        g_material_resources);
dsrrl::runtime::upper_lower_hemenv_draw_runtime
    g_upper_lower_hemenv(g_core, g_upper_lower);
dsrrl::runtime::hemdir3_draw_runtime
    g_hemdir3(g_core, g_upper_lower);
dsrrl::runtime::fixed_pointlight_draw_runtime
    g_fixed_pointlight;
dsrrl::runtime::fixed_pointlight_pipeline_runtime
    g_fixed_pointlight_pipeline;
dsrrl::runtime::clustered_pnts_draw_runtime
    g_clustered_pnts;
dsrrl::runtime::clustered_pnts_pipeline_runtime
    g_clustered_pnts_pipeline;
dsrrl::runtime::pmetal_envspec_draw_runtime
    g_pmetal_envspec(
        g_core,
        g_pmetal_source,
        g_envspec_resources,
        g_material_resources);
dsrrl::runtime::pmetal_native_draw_bridge
    g_pmetal_native_draw;

thread_local bool g_raw_draw_replay_recursing = false;

std::atomic<std::uint64_t> g_present_count{0};
std::atomic<std::uint64_t> g_mr_draw_eval{0};
std::atomic<std::uint64_t> g_mr_would_activate{0};
std::atomic<std::uint64_t> g_mr_fail_open{0};
std::atomic_bool g_mr_ready{false};
std::atomic<std::uint64_t> g_mr_payload_materialize_ok{0};
std::atomic<std::uint64_t> g_mr_payload_materialize_fail{0};
std::atomic_bool g_mr_once_receiver_hit{false};
std::atomic_bool g_mr_once_owner_join{false};
std::atomic_bool g_mr_once_decision_active{false};
std::atomic_bool g_mr_once_identity{false};
std::atomic_bool g_mr_once_batch_ready{false};
std::atomic_bool g_mr_once_draw_issued{false};
std::atomic<std::uint32_t> g_pointlight_gate_log_mask{0u};
std::atomic<std::uint32_t> g_pointlight_prep_log_mask{0u};
std::atomic_bool g_pointlight_active_logged{false};
std::atomic_bool g_pointlight_once_shader_ready{false};
std::atomic_bool g_pointlight_once_sidecar_ready{false};
std::atomic_bool g_pointlight_once_batch_ready{false};
std::atomic_bool g_pointlight_once_applied{false};
std::atomic_bool g_pointlight_once_direct_native_applied{false};
// PR175/176 RT identity probing is diagnostic-only. Runtime proof already
// established the reflective-water 480x270 offscreen PointLight pass. Keep
// the small signature store for the one first-hit observation only.
std::array<std::atomic<std::uint64_t>,4>
    g_pointlight_rt_signatures{};
// Exact producer transports that may publish draw-scoped TLS. The selection
// guard only drains transports that are actually installed; disabled islands
// must not add function-call traffic to every host draw.
std::atomic_bool g_upper_lower_selection_transport_active{false};
std::atomic_bool g_hemdir3_selection_transport_active{false};
std::atomic_bool g_fixed_pointlight_selection_transport_active{false};
std::atomic_bool g_clustered_pointlight_selection_transport_active{false};
std::atomic_bool g_any_draw_selection_transport_active{false};
std::atomic<std::uint64_t> g_mr_ul_payload_materialize_ok{0};
std::atomic<std::uint64_t> g_mr_ul_payload_materialize_fail{0};
std::atomic<std::uint64_t> g_subsurface_spec_payload_materialize_ok{0};
std::atomic<std::uint64_t> g_subsurface_spec_payload_materialize_fail{0};
std::atomic<std::uint64_t> g_lerp_full_draw_ready{0};
std::atomic<std::uint64_t> g_lerp_full_draw_fallback{0};
std::atomic_bool g_lerp_once_receiver_hit{false};
std::atomic_bool g_lerp_once_mr_ul_ready{false};
std::atomic_bool g_lerp_once_mr_only_ready{false};
std::atomic_bool g_lerp_once_batch_ready{false};
std::atomic_bool g_lerp_once_draw_issued{false};
std::atomic<std::uint64_t> g_envspec_payload_materialize_ok{0};
std::atomic<std::uint64_t> g_envspec_payload_materialize_fail{0};
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
std::atomic_bool g_pmetal_full_ptde_create_ok_logged{false};
std::atomic_bool g_pmetal_full_ptde_create_fail_logged{false};
#endif
std::atomic<std::uint64_t> g_draw_events{0};
std::atomic<std::uint64_t> g_draw_receiver_hits{0};
std::atomic<std::uint64_t> g_draw_owner_hits{0};
std::atomic<std::uint64_t> g_draw_joins{0};
std::atomic<std::uint64_t> g_draw_owner_only{0};
std::atomic<std::uint64_t> g_draw_receiver_only{0};
std::atomic<std::uint64_t> g_draw_fast_skip{0};
std::atomic<std::uint64_t> g_local_specular_receiver_hits{0};
std::atomic<std::uint64_t> g_local_specular_clustered_hits{0};
std::atomic<std::uint64_t> g_local_specular_fixed2_hits{0};
std::atomic<std::uint64_t> g_local_specular_fixed4_hits{0};
std::atomic<std::uint64_t> g_local_specular_window_pass{0};
std::atomic<std::uint64_t> g_local_specular_window_fail{0};
std::atomic<std::uint64_t> g_local_specular_windows_total{0};
std::atomic<std::uint64_t> g_local_specular_fixed_plan_ready{0};
std::atomic<std::uint64_t> g_local_specular_clustered_deferred{0};
std::atomic<std::uint64_t> g_local_specular_fixed_plan_fail{0};
std::atomic<std::uint64_t> g_local_specular_operand_ready{0};
std::atomic<std::uint64_t> g_local_specular_operand_fail{0};
std::atomic<std::uint64_t> g_local_specular_output_cut_ready{0};
std::atomic<std::uint64_t> g_local_specular_output_cut_fail{0};
std::atomic<std::uint64_t> g_local_specular_island_plan_ready{0};
std::atomic<std::uint64_t> g_local_specular_island_plan_fail{0};
std::atomic<std::uint64_t> g_local_specular_single_materialize_ok{0};
std::atomic<std::uint64_t> g_local_specular_blended_defer{0};
std::atomic<std::uint64_t> g_local_specular_materialize_fail{0};
std::atomic<std::uint64_t> g_fixed_draw_candidates{0};
std::atomic<std::uint64_t> g_fixed_draw_material_ready{0};
std::atomic<std::uint64_t> g_fixed_draw_spec_ready{0};
std::atomic<std::uint64_t> g_fixed_draw_b12_ready{0};
std::atomic<std::uint64_t> g_fixed_draw_t19_ready{0};
std::atomic<std::uint64_t> g_fixed_draw_batch_ready{0};
std::atomic<std::uint64_t> g_fixed_draw_fail_open{0};
std::atomic<std::uint64_t> g_clustered_draw_candidates{0};
std::atomic<std::uint64_t> g_clustered_draw_pipeline_ready{0};
std::atomic<std::uint64_t> g_clustered_draw_material_ready{0};
std::atomic<std::uint64_t> g_clustered_draw_operator_gate_ready{0};
std::atomic<std::uint64_t> g_clustered_draw_sidecar_ready{0};
std::atomic<std::uint64_t> g_clustered_draw_batch_ready{0};
std::atomic<std::uint64_t> g_clustered_draw_applied{0};
std::atomic<std::uint64_t> g_clustered_draw_neutral_noop{0};
std::atomic<std::uint64_t> g_clustered_draw_fail_open{0};
std::atomic<std::uint64_t> g_clustered_draw_restore_fail{0};

bool g_hot_telemetry_enabled = false;
bool g_effect_telemetry_enabled = false;

bool runtime_hot_telemetry_requested() noexcept
{
    return dsrrl::runtime::telemetry::
        hot_enabled();
}

bool runtime_effect_telemetry_requested() noexcept
{
    return dsrrl::runtime::telemetry::
        effect_enabled();
}

void observe_pointlight_render_target(
    reshade::api::command_list *cmd_list) noexcept
{
    if (cmd_list == nullptr)
        return;

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
    if (context == nullptr)
        return;

    ID3D11RenderTargetView *rtv = nullptr;
    context->OMGetRenderTargets(
        1u,
        &rtv,
        nullptr);

    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    std::uint32_t format = 0u;

    if (rtv != nullptr) {
        ID3D11Resource *resource = nullptr;
        rtv->GetResource(&resource);
        if (resource != nullptr) {
            ID3D11Texture2D *texture = nullptr;
            if (SUCCEEDED(resource->QueryInterface(
                    __uuidof(ID3D11Texture2D),
                    reinterpret_cast<void **>(&texture))) &&
                texture != nullptr) {
                D3D11_TEXTURE2D_DESC desc{};
                texture->GetDesc(&desc);
                width = desc.Width;
                height = desc.Height;
                format =
                    static_cast<std::uint32_t>(
                        desc.Format);
                texture->Release();
            }
            resource->Release();
        }
        rtv->Release();
    }

    D3D11_VIEWPORT viewport{};
    UINT viewport_count = 1u;
    context->RSGetViewports(
        &viewport_count,
        &viewport);

    const auto vp_w =
        viewport_count != 0u
            ? static_cast<std::uint32_t>(
                  viewport.Width + 0.5f)
            : 0u;
    const auto vp_h =
        viewport_count != 0u
            ? static_cast<std::uint32_t>(
                  viewport.Height + 0.5f)
            : 0u;

    std::uint64_t signature =
        0xcbf29ce484222325ULL;
    const std::array<std::uint32_t,5> fields{{
        width,
        height,
        format,
        vp_w,
        vp_h
    }};
    for (const auto field : fields) {
        for (unsigned shift = 0u;
             shift < 32u;
             shift += 8u) {
            signature ^=
                static_cast<std::uint8_t>(
                    (field >> shift) &
                    0xffu);
            signature *=
                0x100000001b3ULL;
        }
    }
    if (signature == 0u)
        signature = 1u;

    bool should_log = false;
    for (auto &slot :
         g_pointlight_rt_signatures) {
        auto observed =
            slot.load(
                std::memory_order_relaxed);
        if (observed == signature)
            return;
        if (observed == 0u &&
            slot.compare_exchange_strong(
                observed,
                signature,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            should_log = true;
            break;
        }
    }

    if (!should_log)
        return;

    char line[320]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL POINTLIGHT RT] rtv=%ux%u fmt=%u viewport=%ux%u",
        static_cast<unsigned>(width),
        static_cast<unsigned>(height),
        static_cast<unsigned>(format),
        static_cast<unsigned>(vp_w),
        static_cast<unsigned>(vp_h));
    reshade::log::message(
        reshade::log::level::info,
        line);
}

void hot_count(
    std::atomic<std::uint64_t> &counter) noexcept
{
    if (g_hot_telemetry_enabled)
        counter.fetch_add(
            1u,
            std::memory_order_relaxed);
}

enum class effect_probe_id : std::uint8_t {
    material_response = 0,
    material_response_lerp,
    spec_rgb,
    diffuse,
    normal,
    upper_lower,
    subsurface,
    hemdir3,
    pmetal_envspec,
    pointlight,
    local_specular,
    pointlight_pnts_attenuation,
    diffuse_material_domain,
    terminal_sat_rgb,
    envspec_nospc_delete,
    fixed_postfog_identity,
    bloom_q8,
    count
};

enum class effect_probe_stage : std::uint8_t {
    candidate = 0,
    authority,
    prepared,
    applied,
    fail_open,
    restore_failed
};

struct effect_probe_state {
    std::atomic_bool candidate{false};
    std::atomic_bool authority{false};
    std::atomic_bool prepared{false};
    std::atomic_bool applied{false};
    std::atomic_bool fail_open{false};
    std::atomic_bool restore_failed{false};
};

constexpr std::size_t k_effect_probe_count =
    static_cast<std::size_t>(
        effect_probe_id::count);

std::array<
    effect_probe_state,
    k_effect_probe_count>
    g_effect_probe{};

// Effect telemetry is a diagnostic surface, not part of the renderer
// operator. Once a stage/effect bit has been observed, keep a compact stage
// mask so render-hot calls can return after one relaxed atomic load instead of
// scanning all 17 effect flags on every routed draw. Individual flags remain
// the source used by the matrix logger.
constexpr std::size_t k_effect_probe_stage_count = 6u;
std::array<
    std::atomic<std::uint32_t>,
    k_effect_probe_stage_count>
    g_effect_probe_stage_mask{};

const char *effect_probe_name(
    effect_probe_id id) noexcept
{
    switch (id) {
    case effect_probe_id::material_response:
        return "MaterialResponse";
    case effect_probe_id::material_response_lerp:
        return "MaterialResponseLerp";
    case effect_probe_id::spec_rgb:
        return "SpecRGB";
    case effect_probe_id::diffuse:
        return "Diffuse";
    case effect_probe_id::normal:
        return "Normal";
    case effect_probe_id::upper_lower:
        return "UpperLower";
    case effect_probe_id::subsurface:
        return "Subsurface";
    case effect_probe_id::hemdir3:
        return "HemDir3";
    case effect_probe_id::pmetal_envspec:
        return "PMetalEnvSpec";
    case effect_probe_id::pointlight:
        return "PointLight";
    case effect_probe_id::local_specular:
        return "LocalSpecular";
    case effect_probe_id::pointlight_pnts_attenuation:
        return "PointLightPntSAttenuation";
    case effect_probe_id::diffuse_material_domain:
        return "DiffuseMaterialDomain";
    case effect_probe_id::terminal_sat_rgb:
        return "TerminalSatRGB";
    case effect_probe_id::envspec_nospc_delete:
        return "EnvSpecNoSpcDelete";
    case effect_probe_id::fixed_postfog_identity:
        return "FixedPostFogIdentity";
    case effect_probe_id::bloom_q8:
        return "BloomQ8";
    default:
        return "Unknown";
    }
}

const char *effect_probe_stage_name(
    effect_probe_stage stage) noexcept
{
    switch (stage) {
    case effect_probe_stage::candidate:
        return "candidate";
    case effect_probe_stage::authority:
        return "authority";
    case effect_probe_stage::prepared:
        return "prepared";
    case effect_probe_stage::applied:
        return "applied";
    case effect_probe_stage::fail_open:
        return "fail_open";
    case effect_probe_stage::restore_failed:
        return "restore_failed";
    default:
        return "unknown";
    }
}

std::atomic_bool &effect_probe_flag(
    effect_probe_state &state,
    effect_probe_stage stage) noexcept
{
    switch (stage) {
    case effect_probe_stage::candidate:
        return state.candidate;
    case effect_probe_stage::authority:
        return state.authority;
    case effect_probe_stage::prepared:
        return state.prepared;
    case effect_probe_stage::applied:
        return state.applied;
    case effect_probe_stage::fail_open:
        return state.fail_open;
    case effect_probe_stage::restore_failed:
        return state.restore_failed;
    default:
        return state.fail_open;
    }
}

void mark_effect_probe(
    effect_probe_id id,
    effect_probe_stage stage,
    std::uint32_t receiver_id = 0xffffffffu,
    std::uint32_t route_index = 0xffffffffu) noexcept
{
    if (!g_effect_telemetry_enabled)
        return;

    auto &state =
        g_effect_probe[
            static_cast<std::size_t>(id)];
    auto &flag =
        effect_probe_flag(
            state,
            stage);

    if (flag.load(
            std::memory_order_relaxed))
        return;

    if (flag.exchange(
            true,
            std::memory_order_relaxed))
        return;

    const auto stage_index =
        static_cast<std::size_t>(stage);
    if (stage_index < k_effect_probe_stage_count) {
        const auto bit =
            std::uint32_t{1u} <<
            static_cast<std::uint8_t>(id);
        g_effect_probe_stage_mask[stage_index].
            fetch_or(
                bit,
                std::memory_order_relaxed);
    }

    char line[320]{};
    if (receiver_id != 0xffffffffu ||
        route_index != 0xffffffffu) {
        std::snprintf(
            line,
            sizeof(line),
            "[DSRRL EFFECT ACT] effect=%s stage=%s rx=%u route=%u",
            effect_probe_name(id),
            effect_probe_stage_name(stage),
            receiver_id == 0xffffffffu ? 0u : receiver_id,
            route_index == 0xffffffffu ? 0u : route_index);
    } else {
        std::snprintf(
            line,
            sizeof(line),
            "[DSRRL EFFECT ACT] effect=%s stage=%s",
            effect_probe_name(id),
            effect_probe_stage_name(stage));
    }
    reshade::log::message(
        reshade::log::level::info,
        line);
}

using effect_probe_mask = std::uint32_t;

constexpr effect_probe_mask effect_probe_bit(
    effect_probe_id id) noexcept
{
    return
        static_cast<effect_probe_mask>(1u) <<
        static_cast<std::uint8_t>(id);
}

effect_probe_mask static_shader_owner_effect_mask(
    dsrrl::core::operator_mask owners) noexcept
{
    effect_probe_mask mask = 0u;
    const auto add =
        [&mask,owners](
            dsrrl::core::operator_id owner,
            effect_probe_id effect) noexcept {
            if ((owners &
                 dsrrl::core::operator_bit(owner)) != 0u)
                mask |= effect_probe_bit(effect);
        };

    add(
        dsrrl::core::operator_id::
            pointlight_pnts_attenuation,
        effect_probe_id::
            pointlight_pnts_attenuation);
    add(
        dsrrl::core::operator_id::
            diffuse_material_domain,
        effect_probe_id::
            diffuse_material_domain);
    add(
        dsrrl::core::operator_id::
            terminal_sat_rgb,
        effect_probe_id::
            terminal_sat_rgb);
    add(
        dsrrl::core::operator_id::
            envspec_nospc_delete,
        effect_probe_id::
            envspec_nospc_delete);
    add(
        dsrrl::core::operator_id::
            fixed_postfog_identity,
        effect_probe_id::
            fixed_postfog_identity);
    return mask;
}

void mark_effect_probe_mask(
    effect_probe_mask mask,
    effect_probe_stage stage,
    std::uint32_t receiver_id = 0xffffffffu,
    std::uint32_t route_index = 0xffffffffu) noexcept
{
    if (!g_effect_telemetry_enabled ||
        mask == 0u)
        return;

    const auto stage_index =
        static_cast<std::size_t>(stage);
    if (stage_index >= k_effect_probe_stage_count)
        return;

    // Most calls hit an already-observed stage. The old implementation still
    // walked every effect on every routed draw, which made the LIVE telemetry
    // binary scale with scene draw density. Keep only unseen bits on the hot
    // path; mark_effect_probe remains the race-safe one-shot publisher.
    const auto seen =
        g_effect_probe_stage_mask[stage_index].
            load(std::memory_order_relaxed);
    const auto pending =
        mask & ~seen;
    if (pending == 0u)
        return;

    for (std::size_t i = 0u;
         i < k_effect_probe_count;
         ++i) {
        const auto id =
            static_cast<effect_probe_id>(i);
        if ((pending & effect_probe_bit(id)) != 0u)
            mark_effect_probe(
                id,
                stage,
                receiver_id,
                route_index);
    }
}

void reset_effect_probe() noexcept
{
    for (auto &stage_mask : g_effect_probe_stage_mask)
        stage_mask.store(
            0u,
            std::memory_order_relaxed);

    for (auto &state : g_effect_probe) {
        state.candidate.store(false);
        state.authority.store(false);
        state.prepared.store(false);
        state.applied.store(false);
        state.fail_open.store(false);
        state.restore_failed.store(false);
    }
}

const char *effect_probe_status(
    const effect_probe_state &state) noexcept
{
    if (state.restore_failed.load(
            std::memory_order_relaxed))
        return "RESTORE_FAILED";
    if (state.applied.load(
            std::memory_order_relaxed))
        return "APPLIED";
    if (state.prepared.load(
            std::memory_order_relaxed))
        return "PREPARED_NOT_APPLIED";
    if (state.authority.load(
            std::memory_order_relaxed))
        return "AUTHORIZED_NOT_PREPARED";
    if (state.fail_open.load(
            std::memory_order_relaxed))
        return "FAIL_OPEN";
    if (state.candidate.load(
            std::memory_order_relaxed))
        return "CANDIDATE_ONLY";
    return "NOT_SEEN";
}

enum integrated_draw_route_bit : std::uint8_t {
    k_route_stable = 1u << 0,
    k_route_hemenvlerp = 1u << 1,
    k_route_subsurface = 1u << 2,
    k_route_hemdir3 = 1u << 3,
    k_route_upper_lower = 1u << 4,
    k_route_a1 = 1u << 5,
    k_route_fixed_pointlight = 1u << 6,
    k_route_clustered_pointlight = 1u << 7
};

constexpr std::uint8_t k_dynamic_draw_route_mask =
    k_route_stable |
    k_route_hemenvlerp |
    k_route_subsurface |
    k_route_hemdir3 |
    k_route_upper_lower |
    k_route_fixed_pointlight |
    k_route_clustered_pointlight;

std::atomic<std::uint8_t> g_active_dynamic_draw_route_mask{
    k_route_stable |
    k_route_hemenvlerp};

std::uint8_t active_integrated_draw_route(
    std::uint8_t raw_mask) noexcept
{
    const auto dynamic =
        g_active_dynamic_draw_route_mask.load(
            std::memory_order_relaxed);
    return static_cast<std::uint8_t>(
        raw_mask &
        static_cast<std::uint8_t>(
            k_route_a1 | dynamic));
}

void publish_active_dynamic_draw_routes() noexcept
{
    std::uint8_t mask =
        k_route_stable |
        k_route_hemenvlerp;

    if (g_core.features().enabled(
            dsrrl::core::operator_id::subsurface))
        mask |= k_route_subsurface;

    if (g_hemdir3_selection_transport_active.load(
            std::memory_order_relaxed))
        mask |= k_route_hemdir3;

    if (g_upper_lower_selection_transport_active.load(
            std::memory_order_relaxed))
        mask |= k_route_upper_lower;

    if (g_fixed_pointlight_selection_transport_active.load(
            std::memory_order_relaxed))
        mask |= k_route_fixed_pointlight;

    if (g_clustered_pointlight_selection_transport_active.load(
            std::memory_order_relaxed))
        mask |= k_route_clustered_pointlight;

    g_active_dynamic_draw_route_mask.store(
        mask,
        std::memory_order_release);
}

struct integrated_draw_route_tls {
    const void *command_list_key = nullptr;
    std::uint8_t mask = 0u;
    dsrrl::core::operator_mask a1_owners = 0u;
};

std::shared_mutex g_integrated_draw_route_mutex;
std::unordered_map<std::uint64_t,std::uint8_t>
    g_integrated_draw_routes;
std::atomic<std::uint64_t>
    g_integrated_draw_route_epoch{1u};

struct integrated_pipeline_route_cache_entry {
    std::uint64_t pipeline_handle = 0u;
    std::uint64_t epoch = 0u;
    std::uint8_t mask = 0u;
};

// Complex areas bind far more than sixteen pixel pipelines in a frame.
// Keep a modest per-thread route working set so ordinary interleaving does
// not fall back to the shared route map on cache collisions.
constexpr std::size_t k_integrated_route_cache_size = 256u;
thread_local std::array<
    integrated_pipeline_route_cache_entry,
    k_integrated_route_cache_size>
    g_integrated_pipeline_route_cache{};
thread_local integrated_draw_route_tls
    g_integrated_draw_route_tls{};

std::size_t integrated_route_cache_index(
    std::uint64_t pipeline_handle) noexcept
{
    const auto mixed =
        (pipeline_handle >> 4u) ^
        (pipeline_handle >> 17u) ^
        (pipeline_handle >> 31u);
    return static_cast<std::size_t>(
        mixed &
        (k_integrated_route_cache_size - 1u));
}

void remember_integrated_draw_route(
    std::uint64_t pipeline_handle,
    std::uint8_t mask) noexcept
{
    if (pipeline_handle == 0u)
        return;

    try {
        std::unique_lock<std::shared_mutex> lock(
            g_integrated_draw_route_mutex);

        const auto found =
            g_integrated_draw_routes.find(
                pipeline_handle);

        if (mask == 0u) {
            // A zero route is normally a first observation and therefore has
            // no cached positive state to invalidate. If this handle already
            // had a route, however, invalidate all TLS verdicts before erase.
            if (found !=
                g_integrated_draw_routes.end()) {
                g_integrated_draw_route_epoch.fetch_add(
                    1u,
                    std::memory_order_acq_rel);
                g_integrated_draw_routes.erase(found);
            }
        } else if (
            found ==
                g_integrated_draw_routes.end()) {
            // Init of an unrelated new pipeline cannot invalidate cached
            // routes for existing handles. Destroy/reuse still advances the
            // epoch in forget_integrated_draw_route().
            g_integrated_draw_routes.emplace(
                pipeline_handle,
                mask);
        } else if (found->second != mask) {
            g_integrated_draw_route_epoch.fetch_add(
                1u,
                std::memory_order_acq_rel);
            found->second = mask;
        }
    } catch (...) {
        try {
            std::unique_lock<std::shared_mutex> lock(
                g_integrated_draw_route_mutex);
            g_integrated_draw_route_epoch.fetch_add(
                1u,
                std::memory_order_acq_rel);
            g_integrated_draw_routes.erase(
                pipeline_handle);
        } catch (...) {
        }
    }
}

void forget_integrated_draw_route(
    std::uint64_t pipeline_handle) noexcept
{
    if (pipeline_handle == 0u)
        return;

    std::unique_lock<std::shared_mutex> lock(
        g_integrated_draw_route_mutex);
    g_integrated_draw_route_epoch.fetch_add(
        1u,
        std::memory_order_acq_rel);
    g_integrated_draw_routes.erase(
        pipeline_handle);
}

std::uint8_t observe_integrated_draw_route_bind(
    const void *command_list_key,
    bool pixel_stage_bound,
    std::uint64_t pipeline_handle) noexcept
{
    if (!pixel_stage_bound ||
        command_list_key == nullptr)
        return g_integrated_draw_route_tls.command_list_key == command_list_key
            ? g_integrated_draw_route_tls.mask
            : 0u;

    std::uint8_t mask = 0u;
    const auto epoch =
        g_integrated_draw_route_epoch.load(
            std::memory_order_acquire);
    auto &cached =
        g_integrated_pipeline_route_cache[
            integrated_route_cache_index(
                pipeline_handle)];

    if (cached.pipeline_handle ==
            pipeline_handle &&
        cached.epoch == epoch &&
        g_integrated_draw_route_epoch.load(
            std::memory_order_acquire) == epoch) {
        mask = cached.mask;
    } else {
        try {
            std::shared_lock<std::shared_mutex> lock(
                g_integrated_draw_route_mutex);
            const auto found =
                g_integrated_draw_routes.find(
                    pipeline_handle);
            if (found !=
                g_integrated_draw_routes.end())
                mask = found->second;

            cached = {
                pipeline_handle,
                g_integrated_draw_route_epoch.load(
                    std::memory_order_relaxed),
                mask
            };
        } catch (...) {
            cached = {};
            mask = 0u;
        }
    }

    g_integrated_draw_route_tls = {
        command_list_key,
        mask,
        0u
    };
    return mask;
}

void observe_integrated_a1_owner_bind(
    const void *command_list_key,
    dsrrl::core::operator_mask owners) noexcept
{
    if (command_list_key == nullptr ||
        g_integrated_draw_route_tls.command_list_key !=
            command_list_key)
        return;

    g_integrated_draw_route_tls.a1_owners =
        owners;
}

dsrrl::core::operator_mask
integrated_a1_owners_bound(
    const void *command_list_key) noexcept
{
    if (command_list_key == nullptr ||
        g_integrated_draw_route_tls.command_list_key !=
            command_list_key)
        return 0u;

    return g_integrated_draw_route_tls.a1_owners;
}

std::uint8_t integrated_draw_route_bound(
    const void *command_list_key) noexcept
{
    if (command_list_key == nullptr ||
        g_integrated_draw_route_tls.command_list_key !=
            command_list_key)
        return 0u;

    return g_integrated_draw_route_tls.mask;
}

void reset_integrated_draw_routes() noexcept
{
    {
        std::unique_lock<std::shared_mutex> lock(
            g_integrated_draw_route_mutex);
        g_integrated_draw_route_epoch.fetch_add(
            1u,
            std::memory_order_acq_rel);
        g_integrated_draw_routes.clear();
    }
    g_integrated_pipeline_route_cache = {};
    g_integrated_draw_route_tls = {};
}

constexpr std::uint32_t k_material_receiver_first = 24u;
constexpr std::uint32_t k_material_receiver_last = 47u;
constexpr std::size_t k_material_receiver_count =
    k_material_receiver_last - k_material_receiver_first + 1u;

struct material_receiver_runtime_counters {
    std::atomic<std::uint64_t> seen{0};
    std::atomic<std::uint64_t> accepted{0};
    std::atomic<std::uint64_t> joined{0};
    std::atomic<std::uint64_t> mr_active{0};
    std::atomic<std::uint64_t> fail_open{0};
};

std::array<
    material_receiver_runtime_counters,
    k_material_receiver_count>
    g_material_receiver_runtime{};

material_receiver_runtime_counters *material_receiver_runtime_slot(
    std::uint32_t receiver_id) noexcept
{
    if (receiver_id < k_material_receiver_first ||
        receiver_id > k_material_receiver_last)
        return nullptr;

    return &g_material_receiver_runtime[
        receiver_id - k_material_receiver_first];
}

std::atomic<std::uint64_t> g_bloom_fx_draw_snapshots{0};
std::atomic<std::uint64_t> g_bloom_fx_draw_authorized{0};
std::atomic<std::uint64_t> g_bloom_fx_draw_rejected{0};

constexpr dsrrl::core::operator_id k_integrated_islands[] = {
    dsrrl::core::operator_id::material_response,
    dsrrl::core::operator_id::spec_rgb,
    dsrrl::core::operator_id::diffuse,
    dsrrl::core::operator_id::normal,
    dsrrl::core::operator_id::subsurface,
    dsrrl::core::operator_id::upper_lower,
    dsrrl::core::operator_id::hemdir3,
    dsrrl::core::operator_id::env_spec,
    dsrrl::core::operator_id::terminal_sat_rgb,
    dsrrl::core::operator_id::diffuse_material_domain,
    dsrrl::core::operator_id::point_light,
    dsrrl::core::operator_id::local_specular_legacy,
    dsrrl::core::operator_id::pointlight_pnts_attenuation,
    dsrrl::core::operator_id::envspec_nospc_delete,
    dsrrl::core::operator_id::fixed_postfog_identity
};

const reshade::api::shader_desc *find_pixel_shader(
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects) noexcept
{
    if (subobjects == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < subobject_count; ++i) {
        if (subobjects[i].type !=
                reshade::api::pipeline_subobject_type::pixel_shader ||
            subobjects[i].count != 1u ||
            subobjects[i].data == nullptr)
            continue;

        return static_cast<const reshade::api::shader_desc *>(
            subobjects[i].data);
    }

    return nullptr;
}

const reshade::api::shader_desc *find_vertex_shader(
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects) noexcept
{
    if (subobjects == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < subobject_count; ++i) {
        if (subobjects[i].type !=
                reshade::api::pipeline_subobject_type::vertex_shader ||
            subobjects[i].count != 1u ||
            subobjects[i].data == nullptr)
            continue;

        return static_cast<const reshade::api::shader_desc *>(
            subobjects[i].data);
    }

    return nullptr;
}

const reshade::api::shader_desc *find_compute_shader(
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects) noexcept
{
    if (subobjects == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < subobject_count; ++i) {
        if (subobjects[i].type !=
                reshade::api::pipeline_subobject_type::compute_shader ||
            subobjects[i].count != 1u ||
            subobjects[i].data == nullptr)
            continue;

        return static_cast<const reshade::api::shader_desc *>(
            subobjects[i].data);
    }

    return nullptr;
}

namespace motion_blur_camera_fallback_disable {

namespace hashing =
    dsrrl::operators::legacy_plan::hashing;
namespace dxbc =
    dsrrl::operators::legacy_plan::dxbc;
namespace velocity_authority =
    dsrrl::operators::postprocess::
        motion_blur_velocity_authority;

// Shared velocity writers are consumed by temporal systems outside MotionBlur.
// User runtime on a2391b9f reported TXAA corruption while the camera-neutral
// VPO replacement was active. Until an exact MotionBlur-only semantic cut is
// proven, preserve stock DSR velocity production and keep only the
// MotionBlurTiles consumer-local fallback patch active.
constexpr bool k_velocity_writer_patch_enabled = false;

// Exact vanilla DSR FRPG_Compute_MotionBlurTiles(.cpo/_CB.cpo).
// Both binder entries are byte-identical. The shader chooses between:
//   object velocity  = velocityBuffer.xy, when velocityBuffer.x < 1
//   camera fallback  = currentUV - previousUV reconstructed from depth,
//                      inverseViewClipMtx, cameraWorldPosition and
//                      prevWorldViewClipMtx.
// This bridge changes only the four repeated MOVC fallback operands so
// invalid/no-object-velocity pixels resolve to a known zero component instead
// of reconstructed camera velocity:
//
//   v_out = valid_object_velocity ? v_object : 0
//
// MotionBlurPre/Final, velocityBuffer production, depth, matrix CB ABI and
// DSR's 60 Hz temporal state remain stock.
constexpr std::size_t k_shader_size = 7944u;
constexpr const char *k_input_sha256 =
    "0792f0bd2fe5cb0dc67447cc01f268f30eeb2af9a8c723f634c76c25af39691a";
constexpr const char *k_output_sha256 =
    "2c22acfc55a9b641f56cf8c29a66eec1074c01cb31a71244f82d012d335f42fb";

struct patch_word {
    std::size_t offset;
    std::uint32_t expected;
    std::uint32_t replacement;
};

// Four camera-fallback MOVC sources, one for each velocity sample processed
// before the tile reduction. The selected .w components are proved zero by
// the immediately preceding block-local initialization and are not written
// before the corresponding MOVC.
constexpr std::array<patch_word,8> k_patch_words{{
    {0x06F4u,0x00100046u,0x00100FF6u}, // r1.xyxx -> r0.wwww
    {0x06F8u,0x00000001u,0x00000000u},
    {0x0ADCu,0x00100EA6u,0x00100FF6u}, // r0.zzzw -> r2.wwww
    {0x0AE0u,0x00000000u,0x00000002u},
    {0x0EA0u,0x00100EA6u,0x00100FF6u}, // r0.zzzw -> r1.wwww
    {0x0EA4u,0x00000000u,0x00000001u},
    {0x130Cu,0x00100EA6u,0x00100FF6u}, // r0.zzzw -> r1.wwww
    {0x1310u,0x00000000u,0x00000001u}
}};

struct velocity_payload_cache_entry {
    hashing::sha256_digest input_digest{};
    hashing::sha256_digest output_digest{};
    std::vector<std::uint8_t> bytes;
};

std::mutex g_mutex;
std::vector<std::uint8_t> g_payload;
std::vector<velocity_payload_cache_entry>
    g_velocity_payloads;
std::unordered_map<std::uint64_t,bool> g_pipelines;
std::unordered_map<std::uint64_t,bool>
    g_velocity_pipelines;
std::atomic<std::uint64_t> g_candidate_size{0};
std::atomic<std::uint64_t> g_exact_identity{0};
std::atomic<std::uint64_t> g_materialized{0};
std::atomic<std::uint64_t> g_fail_open{0};
std::atomic<std::uint64_t> g_init_attested{0};
std::atomic<std::uint64_t> g_compute_binds{0};
std::atomic<std::uint64_t> g_velocity_candidates{0};
std::atomic<std::uint64_t> g_velocity_exact{0};
std::atomic<std::uint64_t> g_velocity_materialized{0};
std::atomic<std::uint64_t> g_velocity_fail_open{0};
std::atomic<std::uint64_t> g_velocity_init_attested{0};
std::atomic<std::uint64_t> g_velocity_binds{0};
std::atomic_bool g_first_bind_logged{false};
std::atomic_bool g_velocity_first_materialize_logged{false};
std::atomic_bool g_velocity_first_bind_logged{false};

bool velocity_digest_authorized(
    const hashing::sha256_digest &digest) noexcept
{
    for (const auto expected :
         velocity_authority::
            k_dsr_velocity_vpo_sha256) {
        if (hashing::matches_hex(
                digest,
                expected))
            return true;
    }

    return false;
}

bool locate_velocity_previous_projection_sites(
    const std::uint8_t *bytes,
    std::size_t size,
    std::array<std::size_t,4> &sites) noexcept
{
    sites.fill(0u);

    if (!dxbc::checksum_container_valid(
            bytes,
            size) ||
        size < 32u)
        return false;

    const auto chunk_count =
        dxbc::read_u32(bytes + 28u);
    if (chunk_count == 0u ||
        chunk_count > 64u ||
        32u +
            static_cast<std::size_t>(
                chunk_count) * 4u >
            size)
        return false;

    constexpr std::uint32_t k_shex =
        0x58454853u; // "SHEX"
    constexpr std::uint32_t k_shdr =
        0x52444853u; // "SHDR"
    constexpr std::uint32_t k_opcode_dp4 = 17u;
    constexpr std::uint32_t k_opcode_customdata = 52u;

    const std::uint8_t *shader = nullptr;
    std::size_t shader_size = 0u;
    std::size_t shader_file_offset = 0u;

    for (std::uint32_t i = 0u;
         i < chunk_count;
         ++i) {
        const auto chunk_offset =
            static_cast<std::size_t>(
                dxbc::read_u32(
                    bytes + 32u + i * 4u));

        if (chunk_offset + 8u > size)
            return false;

        const auto fourcc =
            dxbc::read_u32(
                bytes + chunk_offset);
        if (fourcc != k_shex &&
            fourcc != k_shdr)
            continue;

        const auto payload_size =
            static_cast<std::size_t>(
                dxbc::read_u32(
                    bytes +
                    chunk_offset + 4u));

        if (payload_size < 8u ||
            (payload_size & 3u) != 0u ||
            chunk_offset + 8u +
                payload_size >
                size)
            return false;

        shader =
            bytes + chunk_offset + 8u;
        shader_size = payload_size;
        shader_file_offset =
            chunk_offset + 8u;
        break;
    }

    if (shader == nullptr)
        return false;

    const auto token_count =
        static_cast<std::size_t>(
            dxbc::read_u32(
                shader + 4u));

    if (token_count < 3u ||
        token_count * 4u !=
            shader_size)
        return false;

    std::array<std::uint32_t,8>
        observed{};
    std::size_t observed_count = 0u;
    std::size_t token = 2u;

    while (token < token_count) {
        const auto opcode_token =
            dxbc::read_u32(
                shader + token * 4u);
        const auto opcode =
            opcode_token & 0x7FFu;

        std::size_t instruction_length = 0u;
        if (opcode ==
            k_opcode_customdata) {
            if (token + 1u >=
                token_count)
                return false;
            instruction_length =
                static_cast<std::size_t>(
                    dxbc::read_u32(
                        shader +
                        (token + 1u) *
                            4u));
        } else {
            instruction_length =
                static_cast<std::size_t>(
                    (opcode_token >> 24u) &
                    0x7Fu);
        }

        if (instruction_length == 0u ||
            token +
                instruction_length >
                token_count)
            return false;

        if (opcode == k_opcode_dp4) {
            for (std::size_t j = 1u;
                 j + 2u <
                    instruction_length;
                 ++j) {
                const auto operand =
                    dxbc::read_u32(
                        shader +
                        (token + j) *
                            4u);

                const auto operand_type =
                    (operand >> 12u) &
                    0xFFu;
                const auto index_dimension =
                    (operand >> 20u) &
                    0x3u;
                const auto index_rep0 =
                    (operand >> 22u) &
                    0x7u;
                const auto index_rep1 =
                    (operand >> 25u) &
                    0x7u;

                if (operand_type != 8u ||
                    index_dimension != 2u ||
                    index_rep0 != 0u ||
                    index_rep1 != 0u)
                    continue;

                const auto cb_slot =
                    dxbc::read_u32(
                        shader +
                        (token + j + 1u) *
                            4u);
                const auto cb_register =
                    dxbc::read_u32(
                        shader +
                        (token + j + 2u) *
                            4u);

                if (cb_slot != 0u ||
                    cb_register < 8u ||
                    cb_register > 15u)
                    continue;

                if (observed_count >=
                    observed.size())
                    return false;

                observed[
                    observed_count] =
                    cb_register;

                if (observed_count >= 4u) {
                    sites[
                        observed_count - 4u] =
                        shader_file_offset +
                        (token + j + 2u) *
                            4u;
                }

                ++observed_count;
            }
        }

        token += instruction_length;
    }

    if (token != token_count ||
        observed_count != 8u)
        return false;

    constexpr std::array<
        std::uint32_t,8>
        k_expected{{
            12u,13u,14u,15u,
            8u,9u,10u,11u
        }};

    if (observed != k_expected)
        return false;

    for (std::size_t i = 0u;
         i < sites.size();
         ++i) {
        if (sites[i] == 0u ||
            sites[i] + 4u > size ||
            dxbc::read_u32(
                bytes + sites[i]) !=
                8u + i)
            return false;
    }

    return true;
}

bool velocity_patch_scope_valid(
    const std::uint8_t *source,
    const std::vector<std::uint8_t>
        &replacement,
    const std::array<std::size_t,4>
        &sites) noexcept
{
    if (source == nullptr)
        return false;

    for (std::size_t i = 0u;
         i < replacement.size();
         ++i) {
        if (source[i] ==
            replacement[i])
            continue;

        const bool checksum_byte =
            i >= 4u && i < 20u;

        bool site_byte = false;
        for (const auto site :
             sites) {
            if (i >= site &&
                i < site + 4u) {
                site_byte = true;
                break;
            }
        }

        if (!checksum_byte &&
            !site_byte)
            return false;
    }

    return true;
}

bool materialize_velocity_shader(
    const std::uint8_t *source,
    std::size_t size,
    const hashing::sha256_digest
        &input_digest,
    std::vector<std::uint8_t>
        &replacement,
    hashing::sha256_digest
        &output_digest) noexcept
{
    replacement.clear();
    output_digest = {};

    if (!velocity_digest_authorized(
            input_digest))
        return false;

    std::array<std::size_t,4>
        sites{};
    if (!locate_velocity_previous_projection_sites(
            source,
            size,
            sites))
        return false;

    try {
        replacement.assign(
            source,
            source + size);
    } catch (...) {
        replacement.clear();
        return false;
    }

    for (std::size_t i = 0u;
         i < sites.size();
         ++i)
        dxbc::write_u32(
            replacement.data() +
                sites[i],
            static_cast<std::uint32_t>(
                12u + i));

    if (!dxbc::fix_checksum(
            replacement.data(),
            replacement.size()) ||
        !velocity_patch_scope_valid(
            source,
            replacement,
            sites)) {
        replacement.clear();
        return false;
    }

    output_digest =
        hashing::sha256(
            replacement.data(),
            replacement.size());

    return output_digest !=
        input_digest;
}

bool on_create_velocity_pipeline(
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject
        *subobjects) noexcept
{
    auto *shader =
        const_cast<reshade::api::shader_desc *>(
            find_vertex_shader(
                subobject_count,
                subobjects));

    if (shader == nullptr ||
        shader->code == nullptr ||
        shader->code_size < 7000u ||
        shader->code_size > 11000u)
        return false;

    g_velocity_candidates.fetch_add(
        1u,
        std::memory_order_relaxed);

    const auto *source =
        static_cast<const std::uint8_t *>(
            shader->code);
    const auto input_digest =
        hashing::sha256(
            source,
            shader->code_size);

    if (!velocity_digest_authorized(
            input_digest))
        return false;

    g_velocity_exact.fetch_add(
        1u,
        std::memory_order_relaxed);

    try {
        std::vector<std::uint8_t>
            replacement;
        hashing::sha256_digest
            output_digest{};

        if (!materialize_velocity_shader(
                source,
                shader->code_size,
                input_digest,
                replacement,
                output_digest)) {
            g_velocity_fail_open.fetch_add(
                1u,
                std::memory_order_relaxed);
            return false;
        }

        std::lock_guard<std::mutex> lock(
            g_mutex);

        velocity_payload_cache_entry
            *cached = nullptr;

        for (auto &entry :
             g_velocity_payloads) {
            if (entry.input_digest ==
                input_digest) {
                cached = &entry;
                break;
            }
        }

        if (cached == nullptr) {
            g_velocity_payloads.push_back(
                {
                    input_digest,
                    output_digest,
                    std::move(
                        replacement)
                });
            cached =
                &g_velocity_payloads.back();
        } else if (
            cached->output_digest !=
                output_digest ||
            cached->bytes !=
                replacement) {
            g_velocity_fail_open.fetch_add(
                1u,
                std::memory_order_relaxed);
            return false;
        }

        shader->code =
            cached->bytes.data();
        shader->code_size =
            cached->bytes.size();

        g_velocity_materialized.fetch_add(
            1u,
            std::memory_order_relaxed);

        if (!g_velocity_first_materialize_logged.exchange(
                true,
                std::memory_order_relaxed)) {
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL MOTION BLUR] exact velocity VPO family matched; "
                "previous pose now uses current camera projection "
                "(CommonREG12), preserving pose/object motion while "
                "removing the CommonREG8 previous-camera contribution.");
        }

        return true;
    } catch (...) {
        g_velocity_fail_open.fetch_add(
            1u,
            std::memory_order_relaxed);
        return false;
    }
}

bool velocity_pipeline_attested(
    const reshade::api::shader_desc
        *shader) noexcept
{
    if (shader == nullptr ||
        shader->code == nullptr ||
        shader->code_size == 0u)
        return false;

    const auto digest =
        hashing::sha256(
            static_cast<
                const std::uint8_t *>(
                shader->code),
            shader->code_size);

    std::lock_guard<std::mutex> lock(
        g_mutex);

    for (const auto &entry :
         g_velocity_payloads) {
        if (entry.output_digest ==
            digest)
            return true;
    }

    return false;
}

bool materialize(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &replacement) noexcept
{
    replacement.clear();

    if (source == nullptr ||
        size != k_shader_size ||
        !dxbc::checksum_container_valid(
            source,
            size))
        return false;

    const auto input_digest =
        hashing::sha256(source, size);
    if (!hashing::matches_hex(
            input_digest,
            k_input_sha256))
        return false;

    try {
        replacement.assign(
            source,
            source + size);
    } catch (...) {
        replacement.clear();
        return false;
    }

    for (const auto &site : k_patch_words) {
        if (site.offset + sizeof(std::uint32_t) >
                replacement.size() ||
            dxbc::read_u32(
                replacement.data() +
                site.offset) != site.expected) {
            replacement.clear();
            return false;
        }
    }

    for (const auto &site : k_patch_words)
        dxbc::write_u32(
            replacement.data() +
            site.offset,
            site.replacement);

    if (!dxbc::fix_checksum(
            replacement.data(),
            replacement.size())) {
        replacement.clear();
        return false;
    }

    const auto output_digest =
        hashing::sha256(
            replacement.data(),
            replacement.size());
    if (!hashing::matches_hex(
            output_digest,
            k_output_sha256)) {
        replacement.clear();
        return false;
    }

    return true;
}

bool on_create_compute_pipeline(
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects) noexcept
{
    auto *shader =
        const_cast<reshade::api::shader_desc *>(
            find_compute_shader(
                subobject_count,
                subobjects));

    if (shader == nullptr ||
        shader->code == nullptr ||
        shader->code_size != k_shader_size)
        return false;

    g_candidate_size.fetch_add(
        1u,
        std::memory_order_relaxed);

    const auto *source =
        static_cast<const std::uint8_t *>(
            shader->code);
    const auto input_digest =
        hashing::sha256(
            source,
            shader->code_size);

    if (!hashing::matches_hex(
            input_digest,
            k_input_sha256))
        return false;

    g_exact_identity.fetch_add(
        1u,
        std::memory_order_relaxed);

    try {
        std::vector<std::uint8_t> replacement;
        if (!materialize(
                source,
                shader->code_size,
                replacement)) {
            g_fail_open.fetch_add(
                1u,
                std::memory_order_relaxed);
            return false;
        }

        std::lock_guard<std::mutex> lock(
            g_mutex);

        if (g_payload.empty()) {
            g_payload = std::move(replacement);
        } else if (g_payload != replacement) {
            g_fail_open.fetch_add(
                1u,
                std::memory_order_relaxed);
            return false;
        }

        shader->code =
            g_payload.data();
        shader->code_size =
            g_payload.size();

        g_materialized.fetch_add(
            1u,
            std::memory_order_relaxed);

        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL MOTION BLUR] exact MotionBlurTiles matched; "
            "camera-reprojection fallback velocity disabled; "
            "object velocity path preserved.");

        return true;
    } catch (...) {
        g_fail_open.fetch_add(
            1u,
            std::memory_order_relaxed);
        return false;
    }
}

bool on_create_pipeline(
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects) noexcept
{
    const bool compute_changed =
        on_create_compute_pipeline(
            subobject_count,
            subobjects);
    const bool velocity_changed =
        k_velocity_writer_patch_enabled &&
        on_create_velocity_pipeline(
            subobject_count,
            subobjects);

    return compute_changed ||
           velocity_changed;
}

void on_init_compute_pipeline(
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects,
    reshade::api::pipeline pipeline) noexcept
{
    if (pipeline.handle == 0u)
        return;

    const auto *shader =
        find_compute_shader(
            subobject_count,
            subobjects);
    if (shader == nullptr ||
        shader->code == nullptr ||
        shader->code_size != k_shader_size)
        return;

    const auto digest =
        hashing::sha256(
            static_cast<const std::uint8_t *>(
                shader->code),
            shader->code_size);
    if (!hashing::matches_hex(
            digest,
            k_output_sha256))
        return;

    try {
        std::lock_guard<std::mutex> lock(
            g_mutex);
        const auto inserted =
            g_pipelines.emplace(
                pipeline.handle,
                true).second;
        if (inserted)
            g_init_attested.fetch_add(
                1u,
                std::memory_order_relaxed);
    } catch (...) {
        g_fail_open.fetch_add(
            1u,
            std::memory_order_relaxed);
    }
}

void on_init_pipeline(
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects,
    reshade::api::pipeline pipeline) noexcept
{
    on_init_compute_pipeline(
        subobject_count,
        subobjects,
        pipeline);

    if (!k_velocity_writer_patch_enabled)
        return;

    const auto *vertex_shader =
        find_vertex_shader(
            subobject_count,
            subobjects);

    if (!velocity_pipeline_attested(
            vertex_shader) ||
        pipeline.handle == 0u)
        return;

    try {
        std::lock_guard<std::mutex> lock(
            g_mutex);
        const auto inserted =
            g_velocity_pipelines.emplace(
                pipeline.handle,
                true).second;
        if (inserted)
            g_velocity_init_attested.fetch_add(
                1u,
                std::memory_order_relaxed);
    } catch (...) {
        g_velocity_fail_open.fetch_add(
            1u,
            std::memory_order_relaxed);
    }
}

void on_destroy_pipeline(
    reshade::api::pipeline pipeline) noexcept
{
    if (pipeline.handle == 0u)
        return;

    std::lock_guard<std::mutex> lock(
        g_mutex);
    g_pipelines.erase(
        pipeline.handle);
    g_velocity_pipelines.erase(
        pipeline.handle);
}

void on_bind_pipeline(
    reshade::api::pipeline_stage stages,
    reshade::api::pipeline pipeline) noexcept
{
    if (pipeline.handle == 0u)
        return;

    const auto stage_bits =
        static_cast<std::uint32_t>(
            stages);

    const bool compute_bound =
        (stage_bits &
         static_cast<std::uint32_t>(
             reshade::api::pipeline_stage::
                compute_shader)) != 0u;

    const bool vertex_bound =
        (stage_bits &
         static_cast<std::uint32_t>(
             reshade::api::pipeline_stage::
                vertex_shader)) != 0u;

    // The consumer-local MotionBlur patch is compute-only in the active
    // policy (the shared velocity writer is disabled). Do not serialize every
    // unrelated PS/VS pipeline bind through the MotionBlur registry mutex.
    if (!compute_bound &&
        !(k_velocity_writer_patch_enabled &&
          vertex_bound))
        return;

    bool compute_target = false;
    bool velocity_target = false;

    {
        std::lock_guard<std::mutex> lock(
            g_mutex);
        if (compute_bound)
            compute_target =
                g_pipelines.find(
                    pipeline.handle) !=
                g_pipelines.end();
        if (k_velocity_writer_patch_enabled &&
            vertex_bound)
            velocity_target =
                g_velocity_pipelines.find(
                    pipeline.handle) !=
                g_velocity_pipelines.end();
    }

    if (compute_target) {
        g_compute_binds.fetch_add(
            1u,
            std::memory_order_relaxed);

        if (!g_first_bind_logged.exchange(
                true,
                std::memory_order_relaxed)) {
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL MOTION BLUR] FIRST_BIND exact camera-fallback-disabled "
                "MotionBlurTiles compute pipeline.");
        }
    }

    if (velocity_target) {
        g_velocity_binds.fetch_add(
            1u,
            std::memory_order_relaxed);

        if (!g_velocity_first_bind_logged.exchange(
                true,
                std::memory_order_relaxed)) {
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL MOTION BLUR] FIRST_BIND exact camera-neutral "
                "velocity vertex pipeline.");
        }
    }
}

void log_state(
    const char *tag) noexcept
{
    char line[768]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL MOTION BLUR] tag=%s "
        "tiles_candidate=%llu tiles_exact=%llu tiles_materialized=%llu "
        "tiles_failopen=%llu tiles_init=%llu compute_binds=%llu "
        "velocity_candidate=%llu velocity_exact=%llu velocity_materialized=%llu "
        "velocity_failopen=%llu velocity_init=%llu velocity_binds=%llu "
        "velocity_writer=%s pixel=UNVERIFIED",
        tag != nullptr ? tag : "UNKNOWN",
        static_cast<unsigned long long>(
            g_candidate_size.load(
                std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_exact_identity.load(
                std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_materialized.load(
                std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_fail_open.load(
                std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_init_attested.load(
                std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_compute_binds.load(
                std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_velocity_candidates.load(
                std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_velocity_exact.load(
                std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_velocity_materialized.load(
                std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_velocity_fail_open.load(
                std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_velocity_init_attested.load(
                std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_velocity_binds.load(
                std::memory_order_relaxed)),
        k_velocity_writer_patch_enabled
            ? "PATCHED"
            : "STOCK_TAA_GUARD");
    reshade::log::message(
        reshade::log::level::info,
        line);
}

void reset() noexcept
{
    {
        std::lock_guard<std::mutex> lock(
            g_mutex);
        g_pipelines.clear();
        g_velocity_pipelines.clear();
        g_payload.clear();
        g_velocity_payloads.clear();
    }

    g_candidate_size.store(0);
    g_exact_identity.store(0);
    g_materialized.store(0);
    g_fail_open.store(0);
    g_init_attested.store(0);
    g_compute_binds.store(0);
    g_velocity_candidates.store(0);
    g_velocity_exact.store(0);
    g_velocity_materialized.store(0);
    g_velocity_fail_open.store(0);
    g_velocity_init_attested.store(0);
    g_velocity_binds.store(0);
    g_first_bind_logged.store(false);
    g_velocity_first_materialize_logged.store(false);
    g_velocity_first_bind_logged.store(false);
}

} // namespace motion_blur_camera_fallback_disable

const char *pointlight_decision_reason_name(
    dsrrl::operators::material_response::decision_reason reason) noexcept
{
    using reason_t =
        dsrrl::operators::material_response::decision_reason;
    switch (reason) {
    case reason_t::active: return "active";
    case reason_t::unknown_receiver: return "unknown_receiver";
    case reason_t::material_required: return "material_required";
    case reason_t::owner_tuple_not_authenticated: return "owner_tuple_not_authenticated";
    case reason_t::ptde_companion_required: return "ptde_companion_required";
    case reason_t::unknown_material: return "unknown_material";
    case reason_t::receiver_material_mismatch: return "receiver_material_mismatch";
    case reason_t::no_certified_operator: return "no_certified_operator";
    default: return "unknown_reason";
    }
}

#if defined(DSRRL_MR_MATERIAL_TRACE)
// v2.0.2 location-local MR diagnostic: no shader, CB, resource or feature
// modification. Only diagnostic fingerprints are cached per-thread; all
// source material authority and stock fail-open policies remain untouched.
// Capped log lines avoid turning observation into a new draw-time hotspot.
thread_local std::array<std::uint64_t, 256u> g_mr_material_seen{};
std::atomic<std::uint32_t> g_mr_material_logged{0u};
constexpr std::uint32_t k_mr_material_log_limit = 512u;

void trace_mr_material(
    std::uint32_t receiver,
    const dsrrl::operators::material_response::material_identity &material,
    const dsrrl::operators::material_response::decision &decision,
    std::uint8_t stage) noexcept
{
    if (receiver < 24u || receiver > 47u)
        return;

    // Include stage, decision reason and material identity in the diagnostic
    // fingerprint. Hash entropy here is for log dedup only, NEVER authority.
    std::uint64_t fingerprint = material.semantic_name_hash ^
        (material.material_family_hash << 1u) ^
        (static_cast<std::uint64_t>(receiver) * 0x9e3779b185ebca87ull) ^
        (static_cast<std::uint64_t>(decision.route_index) << 24u) ^
        (static_cast<std::uint64_t>(material.material_slot) << 40u) ^
        (static_cast<std::uint64_t>(stage) << 56u) ^
        (static_cast<std::uint64_t>(decision.reason) << 48u);
    fingerprint ^= fingerprint >> 27u;
    fingerprint *= 0x94d049bb133111ebull;
    if (fingerprint == 0u)
        fingerprint = 1u;

    auto &seen = g_mr_material_seen[
        static_cast<std::size_t>(fingerprint) &
        (g_mr_material_seen.size() - 1u)];
    if (seen == fingerprint)
        return;
    seen = fingerprint;

    const auto ticket = g_mr_material_logged.fetch_add(
        1u, std::memory_order_relaxed);
    if (ticket >= k_mr_material_log_limit)
        return;

    const char *stage_name =
        stage == 1u ? "decision_reject" :
        stage == 2u ? "decision_active" :
        stage == 4u ? "draw_issued" : "owner_missing";
    const char *carrier = material.actual_material_exact
        ? "runtime_mtd"
        : material.owner_tuple_exact ? "flver_owner" : "unverified";

    char line[900]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL MR MATERIAL] stage=%s rx=%u route=%u "
        "reason=%s(%u) ops=%08x carrier=%s mat_valid=%u "
        "owner_exact=%u mtd_exact=%u slot=%u slot_valid=%u "
        "semantic=%016llx family=%016llx mtd_sha0=%02x%02x%02x%02x "
        "flver_sha0=%02x%02x%02x%02x "
        "c100=%.6f,%.6f,%.6f c101=%.6f ptde_spec_power=%.6f spec_verified=%u envspec=%u",
        stage_name, receiver, decision.route_index,
        pointlight_decision_reason_name(decision.reason),
        static_cast<unsigned>(decision.reason),
        decision.certified_operations, carrier,
        material.valid ? 1u : 0u,
        material.owner_tuple_exact ? 1u : 0u,
        material.actual_material_exact ? 1u : 0u,
        material.material_slot,
        material.material_slot_valid ? 1u : 0u,
        static_cast<unsigned long long>(material.semantic_name_hash),
        static_cast<unsigned long long>(material.material_family_hash),
        static_cast<unsigned>(material.raw_mtd_sha256[0]),
        static_cast<unsigned>(material.raw_mtd_sha256[1]),
        static_cast<unsigned>(material.raw_mtd_sha256[2]),
        static_cast<unsigned>(material.raw_mtd_sha256[3]),
        static_cast<unsigned>(material.flver_sha256[0]),
        static_cast<unsigned>(material.flver_sha256[1]),
        static_cast<unsigned>(material.flver_sha256[2]),
        static_cast<unsigned>(material.flver_sha256[3]),
        decision.c100[0], decision.c100[1], decision.c100[2],
        decision.c101, decision.ptde_specular_power,
        decision.ptde_specular_power_verified ? 1u : 0u,
        static_cast<unsigned>(decision.envspec));
    reshade::log::message(reshade::log::level::info,line);
}
#endif

void log_pointlight_gate_once(
    std::uint32_t bit,
    const char *stage,
    bool fixed_pointlight_receiver,
    bool clustered_pointlight_receiver,
    bool clustered_pointlight_spc,
    bool owner_ok,
    const dsrrl::operators::material_response::material_identity &material,
    const dsrrl::operators::material_response::decision &decision) noexcept
{
    if ((!fixed_pointlight_receiver &&
         !clustered_pointlight_receiver) ||
        stage == nullptr)
        return;

    const auto previous =
        g_pointlight_gate_log_mask.fetch_or(
            bit,
            std::memory_order_relaxed);
    if ((previous & bit) != 0u)
        return;

    char line[768]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL POINTLIGHT GATE] stage=%s fixed=%u clustered=%u spc=%u "
        "owner_ok=%u mat_valid=%u owner_exact=%u slot=%u slot_valid=%u "
        "actual_mtd=%u sem=%016llx family=%016llx decision=%s(%u) route=%u",
        stage,
        fixed_pointlight_receiver ? 1u : 0u,
        clustered_pointlight_receiver ? 1u : 0u,
        clustered_pointlight_spc ? 1u : 0u,
        owner_ok ? 1u : 0u,
        material.valid ? 1u : 0u,
        material.owner_tuple_exact ? 1u : 0u,
        material.material_slot,
        material.material_slot_valid ? 1u : 0u,
        material.actual_material_exact ? 1u : 0u,
        static_cast<unsigned long long>(material.semantic_name_hash),
        static_cast<unsigned long long>(material.material_family_hash),
        pointlight_decision_reason_name(decision.reason),
        static_cast<unsigned>(decision.reason),
        decision.route_index);
    reshade::log::message(
        reshade::log::level::info,
        line);
}

const char *clustered_prepare_failure_name(
    dsrrl::runtime::clustered_pnts_prepare_failure failure) noexcept
{
    using failure_t =
        dsrrl::runtime::clustered_pnts_prepare_failure;
    switch (failure) {
    case failure_t::none: return "none";
    case failure_t::precondition: return "precondition";
    case failure_t::selection: return "selection";
    case failure_t::empty_selection: return "empty_selection";
    case failure_t::source_capture: return "source_capture";
    case failure_t::sidecar_build: return "sidecar_build";
    case failure_t::gpu_prepare: return "gpu_prepare";
    case failure_t::gpu_resources: return "gpu_resources";
    case failure_t::upload: return "upload";
    default: return "unknown";
    }
}

void log_pointlight_prep_once(
    std::uint32_t bit,
    const char *stage,
    bool spc,
    bool blended,
    bool material_ready,
    bool operator_gate_ready,
    bool sidecar_ready,
    dsrrl::runtime::clustered_pnts_prepare_failure failure =
        dsrrl::runtime::clustered_pnts_prepare_failure::none,
    std::uint8_t sidecar_result_code = 0u) noexcept
{
    const auto previous =
        g_pointlight_prep_log_mask.fetch_or(
            bit,
            std::memory_order_relaxed);
    if ((previous & bit) != 0u)
        return;

    char line[384]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL POINTLIGHT PREP] stage=%s spc=%u blended=%u material=%u operator_gate=%u sidecar=%u "
        "failure=%s(%u) sidecar_result=%u",
        stage == nullptr ? "unknown" : stage,
        spc ? 1u : 0u,
        blended ? 1u : 0u,
        material_ready ? 1u : 0u,
        operator_gate_ready ? 1u : 0u,
        sidecar_ready ? 1u : 0u,
        clustered_prepare_failure_name(failure),
        static_cast<unsigned>(failure),
        static_cast<unsigned>(sidecar_result_code));
    reshade::log::message(
        reshade::log::level::info,
        line);
}

bool observe_pointlight_draw_identity(
    std::uint8_t route_mask,
    bool fixed_pointlight_receiver,
    bool clustered_pointlight_receiver,
    bool clustered_pointlight_spc,
    dsrrl::operators::material_response::material_identity &out_material,
    dsrrl::operators::material_response::decision &out_decision) noexcept
{
    out_material = {};
    out_decision = {};

    // Special-K-style fast dispatch: exact pipeline attestation selected the
    // PointLight namespace at init/bind time, so a PointLight draw must not
    // re-run the generic Stable/HemEnv/Lerp/Subsurface/HemDir3/U/L receiver
    // census. Any competing visible receiver route is ambiguous and fails open
    // instead of creating a hybrid operator.
    const unsigned pointlight_classes =
        (fixed_pointlight_receiver ? 1u : 0u) +
        (clustered_pointlight_receiver ? 1u : 0u);
    constexpr std::uint8_t k_conflicting_receiver_routes =
        k_route_stable |
        k_route_hemenvlerp |
        k_route_subsurface |
        k_route_hemdir3 |
        k_route_upper_lower;

    if (pointlight_classes != 1u ||
        (route_mask & k_conflicting_receiver_routes) != 0u) {
        hot_count(g_mr_fail_open);
        log_pointlight_gate_once(
            1u << 6,
            "pointlight_route_ambiguous",
            fixed_pointlight_receiver,
            clustered_pointlight_receiver,
            clustered_pointlight_spc,
            false,
            out_material,
            out_decision);
        return false;
    }

    hot_count(g_draw_receiver_hits);

    if (clustered_pointlight_receiver) {
        const bool cached_ready =
            g_clustered_pnts.current_draw_authority(
                clustered_pointlight_spc,
                out_material,
                out_decision);

        if (!cached_ready) {
            hot_count(g_draw_receiver_only);
            hot_count(g_mr_fail_open);
            log_pointlight_gate_once(
                1u << 1,
                "cached_authority_reject",
                fixed_pointlight_receiver,
                clustered_pointlight_receiver,
                clustered_pointlight_spc,
                false,
                out_material,
                out_decision);
            return true;
        }

        hot_count(g_draw_owner_hits);
        hot_count(g_draw_joins);
        hot_count(g_mr_draw_eval);
        hot_count(g_mr_would_activate);

        if (!g_pointlight_active_logged.exchange(
                true,
                std::memory_order_relaxed)) {
            log_pointlight_gate_once(
                1u << 4,
                "decision_active_cached",
                fixed_pointlight_receiver,
                clustered_pointlight_receiver,
                clustered_pointlight_spc,
                true,
                out_material,
                out_decision);
        }
        return true;
    }

    const bool owner_ok =
        dsrrl::runtime::material_owner_selection_consume(
            out_material);
    if (owner_ok)
        hot_count(g_draw_owner_hits);

    if (!owner_ok) {
        hot_count(g_draw_receiver_only);
        hot_count(g_mr_fail_open);
        log_pointlight_gate_once(
            1u << 1,
            "owner_reject",
            fixed_pointlight_receiver,
            clustered_pointlight_receiver,
            clustered_pointlight_spc,
            false,
            out_material,
            out_decision);
        return true;
    }

    hot_count(g_draw_joins);

    if (!g_mr_ready.load()) {
        hot_count(g_mr_fail_open);
        log_pointlight_gate_once(
            1u << 2,
            "material_registry_not_ready",
            fixed_pointlight_receiver,
            clustered_pointlight_receiver,
            clustered_pointlight_spc,
            true,
            out_material,
            out_decision);
        return true;
    }

    hot_count(g_mr_draw_eval);
    const bool requires_specular =
        fixed_pointlight_receiver ||
        (clustered_pointlight_receiver &&
         clustered_pointlight_spc);

    out_decision =
        g_material_response.
            evaluate_direct_pointlight_material(
                out_material,
                requires_specular);

    if (out_decision.active) {
        hot_count(g_mr_would_activate);
        if (!g_pointlight_active_logged.exchange(
                true,
                std::memory_order_relaxed)) {
            log_pointlight_gate_once(
                1u << 4,
                "decision_active",
                fixed_pointlight_receiver,
                clustered_pointlight_receiver,
                clustered_pointlight_spc,
                true,
                out_material,
                out_decision);
        }
    } else {
        hot_count(g_mr_fail_open);
        log_pointlight_gate_once(
            1u << 3,
            "decision_reject",
            fixed_pointlight_receiver,
            clustered_pointlight_receiver,
            clustered_pointlight_spc,
            true,
            out_material,
            out_decision);
    }

    return true;
}

bool observe_draw_identity(
    reshade::api::command_list *cmd_list,
    std::uint8_t route_mask,
    bool fixed_pointlight_receiver,
    bool clustered_pointlight_receiver,
    bool clustered_pointlight_spc,
    std::uint32_t &receiver_id,
    bool &hemenvlerp_bound,
    dsrrl::runtime::hemenvlerp_receiver_identity &hemenvlerp_identity,
    bool &subsurface_bound,
    bool &hemdir3_bound,
    dsrrl::runtime::hemdir3_receiver_identity &hemdir3_identity,
    bool &upper_lower_bound,
    dsrrl::runtime::upper_lower_receiver_identity &upper_lower_identity,
    dsrrl::operators::material_response::material_identity &out_material,
    dsrrl::operators::material_response::decision &out_decision) noexcept
{
    receiver_id = 0u;
    hemenvlerp_bound = false;
    hemenvlerp_identity = {};
    subsurface_bound = false;
    hemdir3_bound = false;
    hemdir3_identity = {};
    upper_lower_bound = false;
    upper_lower_identity = {};

    const bool stable_receiver =
        (route_mask & k_route_stable) != 0u &&
        dsrrl::runtime::stable_receiver_bound(
            cmd_list,
            receiver_id);

    const bool hemenvlerp_receiver =
        (route_mask & k_route_hemenvlerp) != 0u &&
        dsrrl::runtime::hemenvlerp_receiver_bound(
            cmd_list,
            hemenvlerp_identity);

    std::uint32_t subsurface_target = 0u;
    const bool subsurface_receiver =
        (route_mask & k_route_subsurface) != 0u &&
        dsrrl::runtime::subsurface_receiver_bound(
            cmd_list,
            subsurface_target);

    const bool hemdir3_receiver =
        (route_mask & k_route_hemdir3) != 0u &&
        dsrrl::runtime::hemdir3_receiver_bound(
            cmd_list,
            hemdir3_identity);

    const bool upper_lower_receiver =
        (route_mask & k_route_upper_lower) != 0u &&
        dsrrl::runtime::upper_lower_receiver_bound(
            cmd_list,
            upper_lower_identity);

    const bool upper_lower_spc =
        upper_lower_receiver &&
        upper_lower_identity.stratum ==
            dsrrl::operators::lightbank::
                upper_lower_hemenv_stratum::spc;

    const bool upper_lower_nospc =
        upper_lower_receiver &&
        upper_lower_identity.stratum ==
            dsrrl::operators::lightbank::
                upper_lower_hemenv_stratum::nospc;

    const bool upper_lower_isolated =
        upper_lower_receiver &&
        (upper_lower_identity.family ==
             dsrrl::operators::lightbank::
                 upper_lower_hemenv_family::hemenv_parallax ||
         upper_lower_identity.family ==
             dsrrl::operators::lightbank::
                 upper_lower_hemenv_family::hemenvlerp_parallax ||
         upper_lower_identity.family ==
             dsrrl::operators::lightbank::
                 upper_lower_hemenv_family::phn_pnts ||
         upper_lower_identity.family ==
             dsrrl::operators::lightbank::
                 upper_lower_hemenv_family::phn_faceeye ||
         upper_lower_identity.family ==
             dsrrl::operators::lightbank::
                 upper_lower_hemenv_family::phn_subsurf ||
         upper_lower_identity.family ==
             dsrrl::operators::lightbank::
                 upper_lower_hemenv_family::gst ||
         upper_lower_identity.family ==
             dsrrl::operators::lightbank::
                 upper_lower_hemenv_family::gst_faceeye ||
         upper_lower_identity.family ==
             dsrrl::operators::lightbank::
                 upper_lower_hemenv_family::sfx ||
         upper_lower_identity.family ==
             dsrrl::operators::lightbank::
                 upper_lower_hemenv_family::snow ||
         upper_lower_identity.family ==
             dsrrl::operators::lightbank::
                 upper_lower_hemenv_family::ntoa);

    const bool upper_lower_standalone =
        upper_lower_isolated &&
        !subsurface_receiver;

    const bool clustered_pnts_upper_lower_overlap =
        clustered_pointlight_receiver &&
        upper_lower_standalone &&
        upper_lower_identity.family ==
            dsrrl::operators::lightbank::
                upper_lower_hemenv_family::phn_pnts;

    const bool upper_lower_subsurf_combined =
        upper_lower_receiver &&
        upper_lower_identity.family ==
            dsrrl::operators::lightbank::
                upper_lower_hemenv_family::phn_subsurf &&
        subsurface_receiver;

    const bool upper_lower_spc_matches_stable =
        !upper_lower_spc ||
        upper_lower_standalone ||
        upper_lower_subsurf_combined ||
        (upper_lower_identity.family ==
                 dsrrl::operators::lightbank::
                     upper_lower_hemenv_family::hemenv
             ? (stable_receiver &&
                receiver_id ==
                    upper_lower_identity.stable_receiver_id)
             : (hemenvlerp_receiver &&
                hemenvlerp_identity.semantic_receiver_id ==
                    upper_lower_identity.stable_receiver_id));

    const bool upper_lower_unpaired_nospc =
        upper_lower_nospc &&
        !upper_lower_standalone;

    const unsigned receiver_classes =
        (stable_receiver ? 1u : 0u) +
        (hemenvlerp_receiver ? 1u : 0u) +
        (subsurface_receiver ? 1u : 0u) +
        (hemdir3_receiver ? 1u : 0u) +
        (upper_lower_unpaired_nospc ? 1u : 0u) +
        ((upper_lower_standalone && !clustered_pnts_upper_lower_overlap) ? 1u : 0u) +
        (fixed_pointlight_receiver ? 1u : 0u) +
        (clustered_pointlight_receiver ? 1u : 0u);

    const bool receiver_ok =
        receiver_classes == 1u &&
        upper_lower_spc_matches_stable;

    if (fixed_pointlight_receiver ||
        clustered_pointlight_receiver) {
        // Direct PointLight families are independent receiver namespaces.
        // Never borrow Material Response receiver IDs 24..47 for these paths.
        receiver_id = 0u;
    } else if (subsurface_receiver) {
        // DSBT body Subsurf exact receivers 33..35 own the route. Their
        // target plain-HemEnv replacement now includes fresh U/L+b13.
        receiver_id = subsurface_target;
        subsurface_bound = true;
    } else if (upper_lower_standalone) {
        // Exact Parallax, PntS, FaceEye and non-body Subsurf executable
        // identities are U/L-only here. Do not borrow stable HemEnv/HemEnvLerp
        // receiver IDs into MR/resources/EnvSpec. PntS may already carry
        // exact A1 PointLight suboperators composed by stock SHA.
        receiver_id = 0u;
        upper_lower_bound = !clustered_pnts_upper_lower_overlap;
    } else if (hemenvlerp_receiver) {
        receiver_id =
            hemenvlerp_identity.semantic_receiver_id;
        hemenvlerp_bound = true;
        if (upper_lower_spc &&
            upper_lower_identity.family ==
                dsrrl::operators::lightbank::
                    upper_lower_hemenv_family::hemenvlerp)
            upper_lower_bound = true;
    } else if (hemdir3_receiver) {
        // HemDir3 has its own exact receiver namespace. no-Spc deliberately
        // has no paired stable HemEnv receiver, and Spc pairing is metadata
        // only; never leak either into generic HemEnv routing.
        hemdir3_bound = true;
    } else if (upper_lower_nospc) {
        // U/L no-Spc has an exact HemEnv consumer identity but no stable
        // Material Response receiver. Keep generic receiver_id unset.
        receiver_id = 0u;
        upper_lower_bound = true;
    } else if (upper_lower_spc) {
        upper_lower_bound = true;
    }

    out_material = {};
    out_decision = {};

    const bool owner_ok =
        dsrrl::runtime::material_owner_selection_consume(
            out_material);

    // Receiver telemetry is intentionally limited to the confirmed Material
    // Response HemEnv/HemEnvLerp namespace (24..47). Other islands may reuse
    // numeric IDs internally and must not contaminate this census.
    auto *const rx =
        (stable_receiver || hemenvlerp_receiver)
            ? material_receiver_runtime_slot(receiver_id)
            : nullptr;
    if (rx != nullptr)
        hot_count(rx->seen);

    if (receiver_ok) {
        hot_count(g_draw_receiver_hits);
        if (rx != nullptr)
            hot_count(rx->accepted);
        if (!fixed_pointlight_receiver &&
            !clustered_pointlight_receiver &&
            !g_mr_once_receiver_hit.exchange(true)) {
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL MR ACT] stage=receiver_hit");
        }
    }
    if (owner_ok)
        hot_count(g_draw_owner_hits);

    if (!receiver_ok) {
        if (owner_ok)
            hot_count(g_draw_owner_only);
        hot_count(g_mr_fail_open);
        if (rx != nullptr)
            hot_count(rx->fail_open);
        log_pointlight_gate_once(
            1u << 0, "receiver_reject",
            fixed_pointlight_receiver,
            clustered_pointlight_receiver,
            clustered_pointlight_spc,
            owner_ok, out_material, out_decision);
        return false;
    }

    if (!owner_ok) {
#if defined(DSRRL_MR_MATERIAL_TRACE)
        if (rx != nullptr)
            trace_mr_material(receiver_id, out_material, out_decision, 8u);
#endif
        hot_count(g_draw_receiver_only);
        hot_count(g_mr_fail_open);
        if (rx != nullptr)
            hot_count(rx->fail_open);
        log_pointlight_gate_once(
            1u << 1, "owner_reject",
            fixed_pointlight_receiver,
            clustered_pointlight_receiver,
            clustered_pointlight_spc,
            false, out_material, out_decision);

        // Generic MR, resource bridges, P_Metal and direct PointLight all
        // require a material owner. Preserve the previous ownerless
        // continuation only for explicitly enabled standalone islands that
        // may carry their own authority. Under the current policy U/L,
        // HemDir3 and Subsurface are disabled, so receiver-only draws stop
        // here instead of entering batch/resource preparation.
        const bool ownerless_island_enabled =
            (upper_lower_bound &&
             g_core.features().enabled(
                 dsrrl::core::operator_id::upper_lower)) ||
            (hemdir3_bound &&
             g_core.features().enabled(
                 dsrrl::core::operator_id::hemdir3)) ||
            (subsurface_bound &&
             g_core.features().enabled(
                 dsrrl::core::operator_id::subsurface));
        return ownerless_island_enabled;
    }

    hot_count(g_draw_joins);
    if (rx != nullptr)
        hot_count(rx->joined);
    if (!fixed_pointlight_receiver &&
        !clustered_pointlight_receiver &&
        !g_mr_once_owner_join.exchange(true)) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL MR ACT] stage=owner_join");
    }

    if (subsurface_bound ||
        hemdir3_bound) {
        hot_count(g_mr_fail_open);
        return true;
    }

    if (!g_mr_ready.load()) {
        hot_count(g_mr_fail_open);
        if (rx != nullptr)
            hot_count(rx->fail_open);
        log_pointlight_gate_once(
            1u << 2, "material_registry_not_ready",
            fixed_pointlight_receiver,
            clustered_pointlight_receiver,
            clustered_pointlight_spc,
            true, out_material, out_decision);
        return true;
    }

    hot_count(g_mr_draw_eval);
    const bool direct_pointlight_receiver =
        fixed_pointlight_receiver ||
        clustered_pointlight_receiver;
    const bool direct_pointlight_requires_specular =
        fixed_pointlight_receiver ||
        (clustered_pointlight_receiver &&
         clustered_pointlight_spc);

    out_decision =
        direct_pointlight_receiver
            ? g_material_response.
                evaluate_direct_pointlight_material(
                    out_material,
                    direct_pointlight_requires_specular)
            : g_material_response.evaluate(
                receiver_id,
                out_material);

    // Shared base-MTD extension rows are intentionally not authorized by
    // owner identity alone. Their canonical gate requires the currently
    // bound stock t1 logical resource to resolve to an actual PTDE SpecRGB
    // companion. Probe that draw-local carrier only when the profile asks
    // for it, then re-evaluate with the positive companion authority.
    if (!direct_pointlight_receiver &&
        !out_decision.active &&
        out_decision.reason ==
            dsrrl::operators::material_response::
                decision_reason::ptde_companion_required) {
        auto *context =
            cmd_list != nullptr
                ? reinterpret_cast<ID3D11DeviceContext *>(
                      cmd_list->get_native())
                : nullptr;
        if (g_material_resources.
                exact_specular_companion_ready(
                    context))
            out_decision =
                g_material_response.evaluate(
                    receiver_id,
                    out_material,
                    true);
    }

#if defined(DSRRL_MR_MATERIAL_TRACE)
    if (!direct_pointlight_receiver)
        trace_mr_material(
            receiver_id, out_material, out_decision,
            out_decision.active ? 2u : 1u);
#endif

    if (out_decision.active) {
        hot_count(g_mr_would_activate);
        if (rx != nullptr)
            hot_count(rx->mr_active);
        if ((fixed_pointlight_receiver ||
             clustered_pointlight_receiver) &&
            !g_pointlight_active_logged.exchange(
                true,
                std::memory_order_relaxed)) {
            log_pointlight_gate_once(
                1u << 4, "decision_active",
                fixed_pointlight_receiver,
                clustered_pointlight_receiver,
                clustered_pointlight_spc,
                true, out_material, out_decision);
        }
        if (!fixed_pointlight_receiver &&
            !clustered_pointlight_receiver &&
            !g_mr_once_decision_active.exchange(true)) {
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL MR ACT] stage=decision_active");
        }

        if (!fixed_pointlight_receiver &&
            !clustered_pointlight_receiver &&
            !g_mr_once_identity.exchange(true)) {
            char mr_identity_line[512]{};
            std::snprintf(
                mr_identity_line,
                sizeof(mr_identity_line),
                "[DSRRL MR ACTIVE] rx=%u route=%u carrier=%s "
                "c100=%.6f,%.6f,%.6f raw_c101=%.6f",
                receiver_id,
                out_decision.route_index,
                out_material.actual_material_exact
                    ? "runtime_mtd"
                    : (out_material.owner_tuple_exact
                        ? "flver_owner"
                        : "unknown"),
                out_decision.c100[0],
                out_decision.c100[1],
                out_decision.c100[2],
                out_decision.c101);
            reshade::log::message(
                reshade::log::level::info,
                mr_identity_line);
        }
    } else {
        hot_count(g_mr_fail_open);
        if (rx != nullptr)
            hot_count(rx->fail_open);
        log_pointlight_gate_once(
            1u << 3, "decision_reject",
            fixed_pointlight_receiver,
            clustered_pointlight_receiver,
            clustered_pointlight_spc,
            true, out_material, out_decision);
    }

    return true;
}

bool integrated_operator_enabled_by_policy(
    dsrrl::core::operator_id op) noexcept
{
    // Active runtime policy 2026-09-29:
    //   * Subsurface stays stock DSR after the oily/wet + Lerp pixel falsifier.
    //   * HemDir3 stays stock DSR; no visible bridge is shipped.
    //   * Upper/Lower stays stock DSR after the world/FaceEye pixel falsifiers.
    //
    // P_Metal EnvSpec reads PTDE donors at the exact material selector cut
    // and must not depend on the visible U/L runtime or its reference transport.
    switch (op) {
    case dsrrl::core::operator_id::subsurface:
    case dsrrl::core::operator_id::upper_lower:
    case dsrrl::core::operator_id::hemdir3:
        return false;
    default:
        return true;
    }
}

bool enable_integrated_islands() noexcept
{
    for (const auto op : k_integrated_islands) {
        if (!g_core.features().set(
                op,
                integrated_operator_enabled_by_policy(op)))
            return false;
    }

    return true;
}

void disable_integrated_islands() noexcept
{
    for (const auto op : k_integrated_islands)
        (void)g_core.features().set(op, false);
}

void log_effect_matrix(
    const char *tag) noexcept
{
    if (!g_effect_telemetry_enabled)
        return;

    const auto ul =
        g_upper_lower.telemetry();

    if (ul.direct_ul_producer_active) {
        mark_effect_probe(
            effect_probe_id::upper_lower,
            effect_probe_stage::candidate);
        mark_effect_probe(
            effect_probe_id::upper_lower,
            effect_probe_stage::authority);
    }

    if (ul.direct_ul_operator_changed) {
        mark_effect_probe(
            effect_probe_id::upper_lower,
            effect_probe_stage::prepared);
        mark_effect_probe(
            effect_probe_id::upper_lower,
            effect_probe_stage::applied);
    }

    for (std::size_t i = 0u;
         i < k_effect_probe_count;
         ++i) {
        const auto id =
            static_cast<effect_probe_id>(i);
        const auto &state =
            g_effect_probe[i];

        char line[512]{};
        std::snprintf(
            line,
            sizeof(line),
            "[DSRRL EFFECT MATRIX] tag=%s effect=%s status=%s "
            "candidate=%u authority=%u prepared=%u applied=%u "
            "fail_open=%u restore_fail=%u pixel=UNVERIFIED",
            tag != nullptr ? tag : "UNKNOWN",
            effect_probe_name(id),
            effect_probe_status(state),
            state.candidate.load(
                std::memory_order_relaxed) ? 1u : 0u,
            state.authority.load(
                std::memory_order_relaxed) ? 1u : 0u,
            state.prepared.load(
                std::memory_order_relaxed) ? 1u : 0u,
            state.applied.load(
                std::memory_order_relaxed) ? 1u : 0u,
            state.fail_open.load(
                std::memory_order_relaxed) ? 1u : 0u,
            state.restore_failed.load(
                std::memory_order_relaxed) ? 1u : 0u);
        reshade::log::message(
            reshade::log::level::info,
            line);
    }

    const auto fx =
        dsrrl::runtime::
            bloom_fx_draw_transport::status();
    const auto bloom =
        g_bloom_scene_sidecar.telemetry();
    const auto flver =
        dsrrl::runtime::
            flver_identity_transport::status();
    const auto pmetal =
        g_pmetal_envspec.telemetry();
    const auto pmetal_source =
        g_pmetal_source.telemetry();
    const auto subsurface =
        g_subsurface.telemetry();

    char detail[2048]{};
    std::snprintf(
        detail,
        sizeof(detail),
        "[DSRRL EFFECT DETAIL] "
        "MR mtd_classified=%u cache_hit=%u selection_published=%u selector_gates_ul=%u h3=%u "
        "PMetal entry=%u feature=%u material=%u semantic=%u source=%u "
        "receiver_source=%u repl=%u probe=%u spec=%u b12=%u request=%u fail=0x%08X "
        "PMSRC steady=%llu blend=%llu publish=%llu bank_unknown=%llu decode_fail=%llu "
        "busy_drop=%llu consume=%llu/%llu ep_cache=%llu/%llu/%llu region=%llu/%llu gen=%llu "
        "carrier=%u/%u selector=%u q=%u restore_fail=%u "
        "SUB cand=%llu matrej=%llu piperej=%llu surfrej=%llu prep=%llu "
        "UL producer=%u changed=%u quarantine=%u restore_fail=%u "
        "Bloom diag_hooks=%u/%u model_hook=%u proof=%u contents=%u "
        "fx_authorized=%llu fx_rejected=%llu",
        flver.runtime_mtd_classified ? 1u : 0u,
        flver.runtime_mtd_cache_hit ? 1u : 0u,
        flver.runtime_mtd_selection_published ? 1u : 0u,
        flver.upper_lower_selector_enabled ? 1u : 0u,
        flver.hemdir3_selector_enabled ? 1u : 0u,
        pmetal.effect_entry_seen ? 1u : 0u,
        pmetal.effect_feature_ready ? 1u : 0u,
        pmetal.effect_material_ready ? 1u : 0u,
        pmetal.effect_semantic_ready ? 1u : 0u,
        pmetal.effect_source_ready ? 1u : 0u,
        pmetal.effect_receiver_source_ready ? 1u : 0u,
        pmetal.effect_replacement_ready ? 1u : 0u,
        pmetal.effect_probe_ready ? 1u : 0u,
        pmetal.effect_spec_rgb_ready ? 1u : 0u,
        pmetal.effect_b12_ready ? 1u : 0u,
        pmetal.effect_request_ready ? 1u : 0u,
        static_cast<unsigned>(pmetal.effect_fail_mask),
        static_cast<unsigned long long>(pmetal_source.steady_seen),
        static_cast<unsigned long long>(pmetal_source.blend_seen),
        static_cast<unsigned long long>(pmetal_source.exact_publish),
        static_cast<unsigned long long>(pmetal_source.bank_unknown),
        static_cast<unsigned long long>(pmetal_source.decode_fail),
        static_cast<unsigned long long>(pmetal_source.publish_busy_drop),
        static_cast<unsigned long long>(pmetal_source.consumer_ok),
        static_cast<unsigned long long>(pmetal_source.consumer_fail),
        static_cast<unsigned long long>(pmetal_source.endpoint_cache_hit),
        static_cast<unsigned long long>(pmetal_source.endpoint_cache_miss),
        static_cast<unsigned long long>(pmetal_source.endpoint_cache_fill),
        static_cast<unsigned long long>(pmetal_source.region_cache_hit),
        static_cast<unsigned long long>(pmetal_source.region_cache_miss),
        static_cast<unsigned long long>(pmetal_source.cache_generation),
        pmetal_source.steady_carrier_active ? 1u : 0u,
        pmetal_source.blend_carrier_active ? 1u : 0u,
        pmetal_source.selector_carrier_active ? 1u : 0u,
        pmetal_source.quarantined ? 1u : 0u,
        pmetal_source.restore_failed ? 1u : 0u,
        static_cast<unsigned long long>(subsurface.candidates),
        static_cast<unsigned long long>(subsurface.material_rejects),
        static_cast<unsigned long long>(subsurface.pipeline_rejects),
        static_cast<unsigned long long>(subsurface.surface_rejects),
        static_cast<unsigned long long>(subsurface.prepared),
        ul.direct_ul_producer_active ? 1u : 0u,
        ul.direct_ul_operator_changed ? 1u : 0u,
        ul.quarantined ? 1u : 0u,
        ul.restore_failed ? 1u : 0u,
        fx.particle_hook_armed ? 1u : 0u,
        fx.cluster_hook_armed ? 1u : 0u,
        fx.particle_model_ctor_hook_armed ? 1u : 0u,
        bloom.proof_authorized ? 1u : 0u,
        bloom.contents_valid ? 1u : 0u,
        static_cast<unsigned long long>(
            g_bloom_fx_draw_authorized.load(
                std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_bloom_fx_draw_rejected.load(
                std::memory_order_relaxed)));
    reshade::log::message(
        reshade::log::level::info,
        detail);

    char pmetal_frontier[2048]{};
    std::snprintf(
        pmetal_frontier,
        sizeof(pmetal_frontier),
        "[DSRRL PMETAL ENVSPEC FRONTIER] "
        "candidate=%llu lerp_candidate=%llu request=%llu lerp_request=%llu "
        "material_reject=%llu semantic_reject=%llu source_reject=%llu blend_hold=%llu "
        "probe_reject=%llu spec_reject=%llu "
        "b12_upload=%llu b12_reuse=%llu q=%u fail=0x%08X "
        "fail_feature=%u fail_lerp_feature=%u fail_material=%u fail_semantic=%u "
        "fail_source=%u fail_blend=%u fail_context=%u "
        "fail_replacement=%u fail_probe=%u fail_spec_rgb=%u "
        "fail_device=%u fail_b12=%u fail_mutation=%u",
        static_cast<unsigned long long>(pmetal.candidates),
        static_cast<unsigned long long>(pmetal.lerp_candidates),
        static_cast<unsigned long long>(pmetal.requests),
        static_cast<unsigned long long>(pmetal.lerp_requests),
        static_cast<unsigned long long>(pmetal.material_rejects),
        static_cast<unsigned long long>(pmetal.semantic_rejects),
        static_cast<unsigned long long>(pmetal.source_rejects),
        static_cast<unsigned long long>(pmetal.blended_receiver_hold),
        static_cast<unsigned long long>(pmetal.probe_rejects),
        static_cast<unsigned long long>(pmetal.spec_rgb_rejects),
        static_cast<unsigned long long>(pmetal.b12_uploads),
        static_cast<unsigned long long>(pmetal.b12_reuses),
        pmetal.quarantined ? 1u : 0u,
        static_cast<unsigned>(pmetal.effect_fail_mask),
        (pmetal.effect_fail_mask & (1u << 0u)) ? 1u : 0u,
        (pmetal.effect_fail_mask & (1u << 1u)) ? 1u : 0u,
        (pmetal.effect_fail_mask & (1u << 2u)) ? 1u : 0u,
        (pmetal.effect_fail_mask & (1u << 3u)) ? 1u : 0u,
        (pmetal.effect_fail_mask & (1u << 4u)) ? 1u : 0u,
        (pmetal.effect_fail_mask & (1u << 5u)) ? 1u : 0u,
        (pmetal.effect_fail_mask & (1u << 7u)) ? 1u : 0u,
        (pmetal.effect_fail_mask & (1u << 8u)) ? 1u : 0u,
        (pmetal.effect_fail_mask & (1u << 10u)) ? 1u : 0u,
        (pmetal.effect_fail_mask & (1u << 11u)) ? 1u : 0u,
        (pmetal.effect_fail_mask & (1u << 12u)) ? 1u : 0u,
        (pmetal.effect_fail_mask & (1u << 13u)) ? 1u : 0u,
        (pmetal.effect_fail_mask & (1u << 14u)) ? 1u : 0u);
    reshade::log::message(
        reshade::log::level::info,
        pmetal_frontier);

    char subsurface_frontier[1536]{};
    std::snprintf(
        subsurface_frontier,
        sizeof(subsurface_frontier),
        "[DSRRL SUBSURFACE FRONTIER] "
        "candidate=%llu pipeline_reject=%llu material_reject=%llu "
        "surface_reject=%llu mr_reject=%llu resource_reject=%llu "
        "spec_reject=%llu route_reject=%llu adapter_reject=%llu "
        "ul_reject=%llu prepared=%llu",
        static_cast<unsigned long long>(subsurface.candidates),
        static_cast<unsigned long long>(subsurface.pipeline_rejects),
        static_cast<unsigned long long>(subsurface.material_rejects),
        static_cast<unsigned long long>(subsurface.surface_rejects),
        static_cast<unsigned long long>(subsurface.mr_route_rejects),
        static_cast<unsigned long long>(subsurface.resource_rejects),
        static_cast<unsigned long long>(subsurface.spec_rgb_rejects),
        static_cast<unsigned long long>(subsurface.route_rejects),
        static_cast<unsigned long long>(subsurface.adapter_rejects),
        static_cast<unsigned long long>(subsurface.upper_lower_rejects),
        static_cast<unsigned long long>(subsurface.prepared));
    reshade::log::message(
        reshade::log::level::info,
        subsurface_frontier);
}

void log_state(const char *tag) noexcept
{
    const auto t = g_a1_bridge.telemetry();
    const auto f = dsrrl::runtime::flver_identity_stats();
    const auto h = dsrrl::runtime::flver_identity_transport::status();
    const auto selector =
        dsrrl::runtime::flver_identity_transport::
            selector_owner_stats();
    const auto m = dsrrl::runtime::material_owner_selection_stats();
    const auto mr_tx = g_mr_draw_runtime.telemetry();
    const auto tx = g_draw_transactions.telemetry();
    const auto resources = g_material_resources.telemetry();
    const auto tex = dsrrl::runtime::texture_identity_transport::status();
    const auto ul = g_upper_lower.telemetry();
    const auto subs = g_subsurface.telemetry();
    const auto mode =
        dsrrl::runtime::hemdir3_mode_transport::status();

    char line[4096]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s "
        "create=%llu candidate=%llu exact=%llu materialized=%llu "
        "unknown=%llu no_owner=%llu failopen=%llu init_ok=%llu "
        "init_bad=%llu binds=%llu quarantine=%u "
        "flver_hook=%u/%u/%u mtd_hook=%u prov=%u owner_enrich=%u actual_carrier=%u restore_fail=%u "
        "inserts=%llu lookups=%llu hits=%llu misses=%llu tls_hit=%llu mutex_fb=%llu erases=%llu invalid=%llu "
        "owner_sel=%llu owner_enriched=%llu owner_auth=%llu owner_actual=%llu owner_fo=%llu "
        "mr_ready=%u mr_eval=%llu mr_would_activate=%llu mr_fo=%llu "
        "mr_payload_ok=%llu mr_payload_fail=%llu "
        "mr_ul_payload=%llu/%llu sub_spec_payload=%llu/%llu "
        "mr_ul_reg=%llu/%llu mr_ul_prepare=%llu mr_ul_miss=%llu "
        "mr_reg=%llu/%llu lerp_full=%llu/%llu "
        "mr_b12_create=%llu mr_b12_hit=%llu mr_b12_bind_fail=%llu "
        "mr_tx_eligible=%llu mr_tx_miss=%llu mr_replay=%llu mr_restore_fail=%llu mr_quarantine=%u "
        "tx_begin_ok=%llu tx_begin_fail=%llu tx_bind_fail=%llu tx_issued=%llu "
        "tx_restore_ok=%llu tx_restore_fail=%llu tx_readback_skip=%llu tx_quarantine=%u "
        "tex_hook=%u/%u tex_restore_fail=%u res_named=%llu res_ready=%llu res_missing=%llu "
        "res_unsupported=%llu spec_req=%llu fixed_spec=%llu fixed_diff=%llu diff_req=%llu norm_req=%llu res_fo=%llu "
        "ul_hook=%u ul_q=%u ul_restore_fail=%u ul_pub=%llu ul_sel=%llu/%llu/%llu ul_tuple_miss=%llu "
        "ul_pub_tuple_miss=%llu ul_steady=%llu/%llu ul_blend=%llu/%llu/%llu ul_b13=%llu/%llu ul_req=%llu "
        "sub_candidate=%llu sub_prepared=%llu sub_pipe_reject=%llu sub_mat_reject=%llu sub_surface_reject=%llu "
        "sub_ul_reject=%llu sub_mr_reject=%llu sub_res_reject=%llu sub_spec_reject=%llu sub_route_reject=%llu sub_adapter_reject=%llu "
        "mode_hook=%u/%u mode_q=%u mode_restore_fail=%u mode_begin=%llu mode_in2=%llu mode_obs=%llu mode2=%llu mode_snap=%llu/%llu "
        "draw=%llu draw_skip=%llu draw_rx=%llu draw_owner=%llu draw_join=%llu owner_only=%llu rx_only=%llu",
        tag,
        static_cast<unsigned long long>(t.create_events),
        static_cast<unsigned long long>(t.candidate_size_hits),
        static_cast<unsigned long long>(t.exact_identity_hits),
        static_cast<unsigned long long>(t.materialized),
        static_cast<unsigned long long>(t.pass_unknown_identity),
        static_cast<unsigned long long>(t.pass_no_enabled_owner),
        static_cast<unsigned long long>(t.fail_open),
        static_cast<unsigned long long>(t.init_attested),
        static_cast<unsigned long long>(t.init_mismatch),
        static_cast<unsigned long long>(t.target_binds),
        t.quarantined ? 1u : 0u,
        h.parser_armed ? 1u : 0u,
        h.selector_armed ? 1u : 0u,
        h.destructor_armed ? 1u : 0u,
        h.mtd_armed ? 1u : 0u,
        h.provenance_ok ? 1u : 0u,
        h.selector_owner_enrichment ? 1u : 0u,
        h.exact_runtime_material_carrier ? 1u : 0u,
        h.restore_failed ? 1u : 0u,
        static_cast<unsigned long long>(f.inserts),
        static_cast<unsigned long long>(f.lookups),
        static_cast<unsigned long long>(f.hits),
        static_cast<unsigned long long>(f.misses),
        static_cast<unsigned long long>(f.tls_hits),
        static_cast<unsigned long long>(f.mutex_fallbacks),
        static_cast<unsigned long long>(f.erases),
        static_cast<unsigned long long>(f.invalid_raw),
        static_cast<unsigned long long>(m.selector_events),
        static_cast<unsigned long long>(m.owner_enriched),
        static_cast<unsigned long long>(m.owner_authenticated),
        static_cast<unsigned long long>(
            m.actual_material_authenticated),
        static_cast<unsigned long long>(m.fail_open),
        g_mr_ready.load() ? 1u : 0u,
        static_cast<unsigned long long>(g_mr_draw_eval.load()),
        static_cast<unsigned long long>(g_mr_would_activate.load()),
        static_cast<unsigned long long>(g_mr_fail_open.load()),
        static_cast<unsigned long long>(g_mr_payload_materialize_ok.load()),
        static_cast<unsigned long long>(g_mr_payload_materialize_fail.load()),
        static_cast<unsigned long long>(g_mr_ul_payload_materialize_ok.load()),
        static_cast<unsigned long long>(g_mr_ul_payload_materialize_fail.load()),
        static_cast<unsigned long long>(g_subsurface_spec_payload_materialize_ok.load()),
        static_cast<unsigned long long>(g_subsurface_spec_payload_materialize_fail.load()),
        static_cast<unsigned long long>(mr_tx.combined_ul_register_ok),
        static_cast<unsigned long long>(mr_tx.combined_ul_register_fail),
        static_cast<unsigned long long>(mr_tx.combined_ul_prepare),
        static_cast<unsigned long long>(mr_tx.combined_ul_miss),
        static_cast<unsigned long long>(mr_tx.replacement_register_ok),
        static_cast<unsigned long long>(mr_tx.replacement_register_fail),
        static_cast<unsigned long long>(g_lerp_full_draw_ready.load()),
        static_cast<unsigned long long>(g_lerp_full_draw_fallback.load()),
        static_cast<unsigned long long>(mr_tx.b12_create),
        static_cast<unsigned long long>(mr_tx.b12_hit),
        static_cast<unsigned long long>(mr_tx.b12_bind_fail),
        static_cast<unsigned long long>(mr_tx.eligible_draws),
        static_cast<unsigned long long>(mr_tx.replacement_miss),
        static_cast<unsigned long long>(mr_tx.replay_ok),
        static_cast<unsigned long long>(mr_tx.replay_restore_fail),
        mr_tx.quarantined ? 1u : 0u,
        static_cast<unsigned long long>(tx.begin_ok),
        static_cast<unsigned long long>(tx.begin_fail),
        static_cast<unsigned long long>(tx.bind_fail),
        static_cast<unsigned long long>(tx.draws_issued),
        static_cast<unsigned long long>(tx.restore_ok),
        static_cast<unsigned long long>(tx.restore_fail),
        static_cast<unsigned long long>(tx.native_readback_skipped),
        tx.quarantined ? 1u : 0u,
        tex.name_hook_armed ? 1u : 0u,
        tex.clear_hook_armed ? 1u : 0u,
        tex.restore_failed ? 1u : 0u,
        static_cast<unsigned long long>(resources.named_views),
        static_cast<unsigned long long>(resources.sidecar_ready),
        static_cast<unsigned long long>(resources.sidecar_missing),
        static_cast<unsigned long long>(resources.sidecar_unsupported),
        static_cast<unsigned long long>(resources.spec_requests),
        static_cast<unsigned long long>(resources.fixed_pointlight_spec_requests),
        static_cast<unsigned long long>(resources.fixed_pointlight_diffuse_requests),
        static_cast<unsigned long long>(resources.diffuse_requests),
        static_cast<unsigned long long>(resources.normal_requests),
        static_cast<unsigned long long>(resources.fail_open),
        ul.producer_hooks_armed ? 1u : 0u,
        ul.quarantined ? 1u : 0u,
        ul.restore_failed ? 1u : 0u,
        static_cast<unsigned long long>(ul.snapshot_publish),
        static_cast<unsigned long long>(ul.selector_seen),
        static_cast<unsigned long long>(ul.selector_match),
        static_cast<unsigned long long>(ul.selector_miss),
        static_cast<unsigned long long>(ul.tuple_mismatch),
        static_cast<unsigned long long>(
            ul.reference_publish_tuple_mismatch),
        static_cast<unsigned long long>(ul.steady_seen),
        static_cast<unsigned long long>(ul.steady_pass),
        static_cast<unsigned long long>(ul.blend_seen),
        static_cast<unsigned long long>(ul.blend_upper),
        static_cast<unsigned long long>(ul.blend_lower),
        static_cast<unsigned long long>(ul.b13_create),
        static_cast<unsigned long long>(ul.b13_hit),
        static_cast<unsigned long long>(ul.requests),
        static_cast<unsigned long long>(subs.candidates),
        static_cast<unsigned long long>(subs.prepared),
        static_cast<unsigned long long>(subs.pipeline_rejects),
        static_cast<unsigned long long>(subs.material_rejects),
        static_cast<unsigned long long>(subs.surface_rejects),
        static_cast<unsigned long long>(subs.upper_lower_rejects),
        static_cast<unsigned long long>(subs.mr_route_rejects),
        static_cast<unsigned long long>(subs.resource_rejects),
        static_cast<unsigned long long>(subs.spec_rgb_rejects),
        static_cast<unsigned long long>(subs.route_rejects),
        static_cast<unsigned long long>(subs.adapter_rejects),
        mode.lt5_hook_armed ? 1u : 0u,
        mode.selector_end_hook_armed ? 1u : 0u,
        mode.quarantined ? 1u : 0u,
        mode.restore_failed ? 1u : 0u,
        static_cast<unsigned long long>(mode.selector_begin),
        static_cast<unsigned long long>(mode.incoming_mode2),
        static_cast<unsigned long long>(mode.effective_observed),
        static_cast<unsigned long long>(mode.mode2_observed),
        static_cast<unsigned long long>(mode.snapshot_hits),
        static_cast<unsigned long long>(mode.snapshot_misses),
        static_cast<unsigned long long>(g_draw_events.load()),
        static_cast<unsigned long long>(g_draw_fast_skip.load()),
        static_cast<unsigned long long>(g_draw_receiver_hits.load()),
        static_cast<unsigned long long>(g_draw_owner_hits.load()),
        static_cast<unsigned long long>(g_draw_joins.load()),
        static_cast<unsigned long long>(g_draw_owner_only.load()),
        static_cast<unsigned long long>(g_draw_receiver_only.load()));

    reshade::log::message(reshade::log::level::info, line);

    char selector_perf_line[512]{};
    std::snprintf(
        selector_perf_line,
        sizeof(selector_perf_line),
        "[DSRRL RUNTIME V2] %s SELECTOR_PERF events=%llu final_cache=%llu/%llu owner_sha=%llu owner_mtd=%llu exact_ready=%llu runtime_mtd=%llu/%llu flver_tls=%llu mutex_fb=%llu",
        tag,
        static_cast<unsigned long long>(
            selector.selector_events),
        static_cast<unsigned long long>(
            selector.selector_identity_cache_hits),
        static_cast<unsigned long long>(
            selector.selector_identity_cache_misses),
        static_cast<unsigned long long>(
            selector.owner_sha_hits),
        static_cast<unsigned long long>(
            selector.owner_mtd_hits),
        static_cast<unsigned long long>(
            selector.exact_owner_ready),
        static_cast<unsigned long long>(
            selector.runtime_material_hits),
        static_cast<unsigned long long>(
            selector.runtime_material_ready),
        static_cast<unsigned long long>(
            f.tls_hits),
        static_cast<unsigned long long>(
            f.mutex_fallbacks));
    reshade::log::message(
        reshade::log::level::info,
        selector_perf_line);

    char ul_direct_line[320]{};
    std::snprintf(
        ul_direct_line,
        sizeof(ul_direct_line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s "
        "UL_DIRECT active=%u steady=%llu blend=%llu fail=%llu "
        "draw_ready=%llu draw_fallback=%llu",
        tag,
        ul.direct_ul_producer_active ? 1u : 0u,
        static_cast<unsigned long long>(
            ul.direct_ul_steady_inject),
        static_cast<unsigned long long>(
            ul.direct_ul_blend_inject),
        static_cast<unsigned long long>(
            ul.direct_ul_inject_fail),
        static_cast<unsigned long long>(
            ul.direct_ul_draw_ready),
        static_cast<unsigned long long>(
            ul.direct_ul_draw_fallback));
    reshade::log::message(
        reshade::log::level::info,
        ul_direct_line);

    // Observe-only exact-hash census for the still-unarmed PTDE local
    // PointLight specular island. These counters are not bridge activation.
    char local_spec_line[1024]{};
    std::snprintf(
        local_spec_line,
        sizeof(local_spec_line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s "
        "LOCAL_SPEC_RX exact=%llu clustered=%llu fixed2=%llu fixed4=%llu "
        "window_pass=%llu window_fail=%llu windows=%llu "
        "fixed_ready=%llu clustered_defer=%llu plan_fail=%llu "
        "operand_ready=%llu operand_fail=%llu output_cut=%llu/%llu "
        "island_plan=%llu/%llu materialize=%llu blend_defer=%llu mat_fail=%llu armed=0",
        tag,
        static_cast<unsigned long long>(g_local_specular_receiver_hits.load()),
        static_cast<unsigned long long>(g_local_specular_clustered_hits.load()),
        static_cast<unsigned long long>(g_local_specular_fixed2_hits.load()),
        static_cast<unsigned long long>(g_local_specular_fixed4_hits.load()),
        static_cast<unsigned long long>(g_local_specular_window_pass.load()),
        static_cast<unsigned long long>(g_local_specular_window_fail.load()),
        static_cast<unsigned long long>(g_local_specular_windows_total.load()),
        static_cast<unsigned long long>(g_local_specular_fixed_plan_ready.load()),
        static_cast<unsigned long long>(g_local_specular_clustered_deferred.load()),
        static_cast<unsigned long long>(g_local_specular_fixed_plan_fail.load()),
        static_cast<unsigned long long>(g_local_specular_operand_ready.load()),
        static_cast<unsigned long long>(g_local_specular_operand_fail.load()),
        static_cast<unsigned long long>(g_local_specular_output_cut_ready.load()),
        static_cast<unsigned long long>(g_local_specular_output_cut_fail.load()),
        static_cast<unsigned long long>(g_local_specular_island_plan_ready.load()),
        static_cast<unsigned long long>(g_local_specular_island_plan_fail.load()),
        static_cast<unsigned long long>(g_local_specular_single_materialize_ok.load()),
        static_cast<unsigned long long>(g_local_specular_blended_defer.load()),
        static_cast<unsigned long long>(g_local_specular_materialize_fail.load()));
    reshade::log::message(
        reshade::log::level::info,
        local_spec_line);

    const auto fixed_pl = g_fixed_pointlight.telemetry();
    const auto fixed_pipe =
        g_fixed_pointlight_pipeline.telemetry();
    char fixed_pl_line[1024]{};
    std::snprintf(
        fixed_pl_line,
        sizeof(fixed_pl_line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s "
        "FIXED_PL_TX captures=%llu restarts=%llu rejects=%llu "
        "selector=%llu/%llu stale=%llu t19=%llu/%llu requests=%llu "
        "pipe=%llu/%llu init=%llu bind=%llu/%llu "
        "draw=%llu mat=%llu spec=%llu b12=%llu q=%llu ready=%llu failopen=%llu "
        "hook=%u quarantine=%u/%u restore_fail=%u",
        tag,
        static_cast<unsigned long long>(fixed_pl.producer_captures),
        static_cast<unsigned long long>(fixed_pl.producer_restarts),
        static_cast<unsigned long long>(fixed_pl.producer_rejects),
        static_cast<unsigned long long>(fixed_pl.selector_seen),
        static_cast<unsigned long long>(fixed_pl.selector_match),
        static_cast<unsigned long long>(fixed_pl.selector_stale),
        static_cast<unsigned long long>(fixed_pl.t19_create),
        static_cast<unsigned long long>(fixed_pl.t19_hit),
        static_cast<unsigned long long>(fixed_pl.requests),
        static_cast<unsigned long long>(fixed_pipe.candidate_create_ok),
        static_cast<unsigned long long>(fixed_pipe.candidate_create_fail),
        static_cast<unsigned long long>(fixed_pipe.init_attested),
        static_cast<unsigned long long>(fixed_pipe.bind_hits),
        static_cast<unsigned long long>(fixed_pipe.bind_misses),
        static_cast<unsigned long long>(g_fixed_draw_candidates.load()),
        static_cast<unsigned long long>(g_fixed_draw_material_ready.load()),
        static_cast<unsigned long long>(g_fixed_draw_spec_ready.load()),
        static_cast<unsigned long long>(g_fixed_draw_b12_ready.load()),
        static_cast<unsigned long long>(g_fixed_draw_t19_ready.load()),
        static_cast<unsigned long long>(g_fixed_draw_batch_ready.load()),
        static_cast<unsigned long long>(g_fixed_draw_fail_open.load()),
        fixed_pl.capture_hook_armed ? 1u : 0u,
        fixed_pl.quarantined ? 1u : 0u,
        fixed_pipe.quarantined ? 1u : 0u,
        fixed_pl.restore_failed ? 1u : 0u);
    reshade::log::message(
        reshade::log::level::info,
        fixed_pl_line);

    const auto clustered_pl =
        g_clustered_pnts.telemetry();
    const auto clustered_pipe =
        g_clustered_pnts_pipeline.telemetry();
    char clustered_pl_line[2048]{};
    std::snprintf(
        clustered_pl_line,
        sizeof(clustered_pl_line),
        "[DSRRL CLPNTS ACT] tag=%s "
        "builder=%llu collection=%llu/%llu selector=%llu mirror=%llu/%llu "
        "source=%llu/%llu publish=%llu owner=%llu/%llu max=%llu/%llu "
        "sidecar=%llu/%llu gpu18=%llu/%llu gpu19=%llu/%llu b12=%llu/%llu prepare=%llu/%llu neutral=%llu "
        "failstage=pre:%llu sel:%llu empty:%llu build:%llu ctx:%llu/%llu/%llu gpu:%llu upload:%llu "
        "pipe_reg=%llu/%llu pipe_init=%llu pipe_bind=%llu/%llu "
        "draw_candidate=%llu pipeline_ready=%llu material_ready=%llu operator_gate_ready=%llu "
        "sidecar_ready=%llu batch_ready=%llu applied=%llu neutral_noop=%llu target_failopen=%llu restore_fail=%llu "
        "enabled=%u quarantine=%u/%u",
        tag,
        static_cast<unsigned long long>(clustered_pl.builder_seen),
        static_cast<unsigned long long>(clustered_pl.collection_ok),
        static_cast<unsigned long long>(clustered_pl.collection_fail),
        static_cast<unsigned long long>(clustered_pl.selector_calls),
        static_cast<unsigned long long>(clustered_pl.mirror_equal),
        static_cast<unsigned long long>(clustered_pl.mirror_diff),
        static_cast<unsigned long long>(clustered_pl.source_capture_ok),
        static_cast<unsigned long long>(clustered_pl.source_capture_fail),
        static_cast<unsigned long long>(clustered_pl.snapshot_publish),
        static_cast<unsigned long long>(clustered_pl.owner_join_hit),
        static_cast<unsigned long long>(clustered_pl.owner_join_miss),
        static_cast<unsigned long long>(clustered_pl.material_limit_ok),
        static_cast<unsigned long long>(clustered_pl.material_limit_fail),
        static_cast<unsigned long long>(clustered_pl.sidecar_ready),
        static_cast<unsigned long long>(clustered_pl.sidecar_fail),
        static_cast<unsigned long long>(clustered_pl.t18_create),
        static_cast<unsigned long long>(clustered_pl.t18_hit),
        static_cast<unsigned long long>(clustered_pl.t19_create),
        static_cast<unsigned long long>(clustered_pl.t19_hit),
        static_cast<unsigned long long>(clustered_pl.b12_create),
        static_cast<unsigned long long>(clustered_pl.b12_hit),
        static_cast<unsigned long long>(clustered_pl.prepare_ok),
        static_cast<unsigned long long>(clustered_pl.prepare_fail),
        static_cast<unsigned long long>(clustered_pl.prepare_neutral_empty),
        static_cast<unsigned long long>(clustered_pl.prepare_precondition_fail),
        static_cast<unsigned long long>(clustered_pl.selection_fail),
        static_cast<unsigned long long>(clustered_pl.selection_empty),
        static_cast<unsigned long long>(clustered_pl.sidecar_build_fail),
        static_cast<unsigned long long>(clustered_pl.context_immediate),
        static_cast<unsigned long long>(clustered_pl.context_deferred),
        static_cast<unsigned long long>(clustered_pl.context_other),
        static_cast<unsigned long long>(clustered_pl.gpu_prepare_fail),
        static_cast<unsigned long long>(clustered_pl.upload_fail),
        static_cast<unsigned long long>(clustered_pipe.candidate_create_ok),
        static_cast<unsigned long long>(clustered_pipe.candidate_create_fail),
        static_cast<unsigned long long>(clustered_pipe.init_attested),
        static_cast<unsigned long long>(clustered_pipe.bind_hits),
        static_cast<unsigned long long>(clustered_pipe.bind_misses),
        static_cast<unsigned long long>(g_clustered_draw_candidates.load()),
        static_cast<unsigned long long>(g_clustered_draw_pipeline_ready.load()),
        static_cast<unsigned long long>(g_clustered_draw_material_ready.load()),
        static_cast<unsigned long long>(g_clustered_draw_operator_gate_ready.load()),
        static_cast<unsigned long long>(g_clustered_draw_sidecar_ready.load()),
        static_cast<unsigned long long>(g_clustered_draw_batch_ready.load()),
        static_cast<unsigned long long>(g_clustered_draw_applied.load()),
        static_cast<unsigned long long>(g_clustered_draw_neutral_noop.load()),
        static_cast<unsigned long long>(g_clustered_draw_fail_open.load()),
        static_cast<unsigned long long>(g_clustered_draw_restore_fail.load()),
        clustered_pl.enabled ? 1u : 0u,
        clustered_pl.quarantined ? 1u : 0u,
        clustered_pipe.quarantined ? 1u : 0u);
    reshade::log::message(
        reshade::log::level::info,
        clustered_pl_line);

    // Compact machine-parseable receiver census. Emit one line rather than
    // 24 lines per checkpoint so long runtime captures remain practical.
    char rx_line[4096]{};
    int rx_used = std::snprintf(
        rx_line,
        sizeof(rx_line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s_RX",
        tag);
    bool rx_any = false;
    for (std::uint32_t receiver_id = k_material_receiver_first;
         receiver_id <= k_material_receiver_last &&
         rx_used > 0 &&
         static_cast<std::size_t>(rx_used) < sizeof(rx_line);
         ++receiver_id) {
        const auto &rx = g_material_receiver_runtime[
            receiver_id - k_material_receiver_first];
        const auto seen = rx.seen.load();
        if (seen == 0u)
            continue;

        rx_any = true;
        const int added = std::snprintf(
            rx_line + rx_used,
            sizeof(rx_line) - static_cast<std::size_t>(rx_used),
            " rx%u=%llu,%llu,%llu,%llu,%llu",
            receiver_id,
            static_cast<unsigned long long>(seen),
            static_cast<unsigned long long>(rx.accepted.load()),
            static_cast<unsigned long long>(rx.joined.load()),
            static_cast<unsigned long long>(rx.mr_active.load()),
            static_cast<unsigned long long>(rx.fail_open.load()));
        if (added <= 0)
            break;
        rx_used += added;
    }
    if (!rx_any &&
        rx_used > 0 &&
        static_cast<std::size_t>(rx_used) < sizeof(rx_line))
        std::snprintf(
            rx_line + rx_used,
            sizeof(rx_line) - static_cast<std::size_t>(rx_used),
            " none=1");

    reshade::log::message(
        reshade::log::level::info,
        rx_line);

    const auto ul_pipe =
        dsrrl::runtime::upper_lower_receiver_pipeline_stats();
    const auto ul_draw =
        g_upper_lower_hemenv.telemetry();

    char ul_line[1280]{};
    std::snprintf(
        ul_line,
        sizeof(ul_line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s_UL "
        "pipe_attest=%llu pipe_conflict=%llu pipe_init=%llu exact_nospc=%llu exact_spc=%llu "
        "init_bad=%llu binds=%llu/%llu/%llu unknown=%llu lookup=%llu/%llu/%llu q=%u "
        "repl=%llu/%llu candidate=%llu carrier=%llu/%llu identity_reject=%llu "
        "nospc_ready=%llu spc_ready=%llu spc_mr_hold=%llu "
        "family_seen=%llu/%llu/%llu/%llu/%llu family_ready=%llu/%llu/%llu/%llu/%llu "
        "req=%llu ul_q=%u",
        tag,
        static_cast<unsigned long long>(ul_pipe.created_code_attested),
        static_cast<unsigned long long>(ul_pipe.created_code_conflict),
        static_cast<unsigned long long>(ul_pipe.pipeline_inits),
        static_cast<unsigned long long>(ul_pipe.exact_nospc_hits),
        static_cast<unsigned long long>(ul_pipe.exact_spc_hits),
        static_cast<unsigned long long>(ul_pipe.init_mismatch),
        static_cast<unsigned long long>(ul_pipe.pixel_binds),
        static_cast<unsigned long long>(ul_pipe.nospc_binds),
        static_cast<unsigned long long>(ul_pipe.spc_binds),
        static_cast<unsigned long long>(ul_pipe.unknown_binds),
        static_cast<unsigned long long>(ul_pipe.lookups),
        static_cast<unsigned long long>(ul_pipe.lookup_hits),
        static_cast<unsigned long long>(ul_pipe.lookup_misses),
        ul_pipe.quarantined ? 1u : 0u,
        static_cast<unsigned long long>(ul_draw.replacement_register_ok),
        static_cast<unsigned long long>(ul_draw.replacement_register_fail),
        static_cast<unsigned long long>(ul_draw.candidates),
        static_cast<unsigned long long>(ul_draw.carrier_ready),
        static_cast<unsigned long long>(ul_draw.carrier_rejects),
        static_cast<unsigned long long>(ul_draw.identity_rejects),
        static_cast<unsigned long long>(ul_draw.nospc_ready),
        static_cast<unsigned long long>(ul_draw.spc_ready),
        static_cast<unsigned long long>(ul_draw.spc_mr_hold),
        static_cast<unsigned long long>(ul_draw.phn_candidates),
        static_cast<unsigned long long>(ul_draw.gst_candidates),
        static_cast<unsigned long long>(ul_draw.sfx_candidates),
        static_cast<unsigned long long>(ul_draw.snow_candidates),
        static_cast<unsigned long long>(ul_draw.ntoa_candidates),
        static_cast<unsigned long long>(ul_draw.phn_ready),
        static_cast<unsigned long long>(ul_draw.gst_ready),
        static_cast<unsigned long long>(ul_draw.sfx_ready),
        static_cast<unsigned long long>(ul_draw.snow_ready),
        static_cast<unsigned long long>(ul_draw.ntoa_ready),
        static_cast<unsigned long long>(ul_draw.requests),
        ul_draw.quarantined ? 1u : 0u);

    reshade::log::message(
        reshade::log::level::info,
        ul_line);

    const auto h3_pipe =
        dsrrl::runtime::hemdir3_receiver_pipeline_stats();
    const auto h3_draw =
        g_hemdir3.telemetry();

    char h3_line[1536]{};
    std::snprintf(
        h3_line,
        sizeof(h3_line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s_H3 "
        "pipe_attest=%llu pipe_conflict=%llu pipe_init=%llu exact_nospc=%llu exact_spc=%llu "
        "init_bad=%llu binds=%llu/%llu/%llu unknown=%llu lookup=%llu/%llu/%llu q=%u "
        "repl=%llu/%llu candidate=%llu mode2=%llu mode_reject=%llu carrier=%llu/%llu "
        "nospc_ready=%llu spc_ready=%llu donor=%llu/%llu b12=%llu/%llu spc_hold=%llu "
        "readiness_reject=%llu req=%llu h3_q=%u "
        "d123_steady=%llu d123_blend_dir=%llu d123_blend_col=%llu d123_publish=%llu "
        "h3_b13=%llu/%llu h3_carrier_req=%llu",
        tag,
        static_cast<unsigned long long>(h3_pipe.created_code_attested),
        static_cast<unsigned long long>(h3_pipe.created_code_conflict),
        static_cast<unsigned long long>(h3_pipe.pipeline_inits),
        static_cast<unsigned long long>(h3_pipe.exact_nospc_hits),
        static_cast<unsigned long long>(h3_pipe.exact_spc_hits),
        static_cast<unsigned long long>(h3_pipe.init_mismatch),
        static_cast<unsigned long long>(h3_pipe.pixel_binds),
        static_cast<unsigned long long>(h3_pipe.nospc_binds),
        static_cast<unsigned long long>(h3_pipe.spc_binds),
        static_cast<unsigned long long>(h3_pipe.unknown_binds),
        static_cast<unsigned long long>(h3_pipe.lookups),
        static_cast<unsigned long long>(h3_pipe.lookup_hits),
        static_cast<unsigned long long>(h3_pipe.lookup_misses),
        h3_pipe.quarantined ? 1u : 0u,
        static_cast<unsigned long long>(h3_draw.replacement_register_ok),
        static_cast<unsigned long long>(h3_draw.replacement_register_fail),
        static_cast<unsigned long long>(h3_draw.candidates),
        static_cast<unsigned long long>(h3_draw.mode2_hits),
        static_cast<unsigned long long>(h3_draw.mode_rejects),
        static_cast<unsigned long long>(h3_draw.carrier_ready),
        static_cast<unsigned long long>(h3_draw.carrier_rejects),
        static_cast<unsigned long long>(h3_draw.nospc_ready),
        static_cast<unsigned long long>(h3_draw.spc_ready),
        static_cast<unsigned long long>(h3_draw.spc_donor_hit),
        static_cast<unsigned long long>(h3_draw.spc_donor_miss),
        static_cast<unsigned long long>(h3_draw.spc_b12_create),
        static_cast<unsigned long long>(h3_draw.spc_b12_hit),
        static_cast<unsigned long long>(h3_draw.spc_b12_hold),
        static_cast<unsigned long long>(h3_draw.readiness_rejects),
        static_cast<unsigned long long>(h3_draw.requests),
        h3_draw.quarantined ? 1u : 0u,
        static_cast<unsigned long long>(ul.d123_steady),
        static_cast<unsigned long long>(ul.d123_blend_direction),
        static_cast<unsigned long long>(ul.d123_blend_color),
        static_cast<unsigned long long>(ul.d123_snapshot_publish),
        static_cast<unsigned long long>(ul.hemdir3_b13_create),
        static_cast<unsigned long long>(ul.hemdir3_b13_hit),
        static_cast<unsigned long long>(ul.hemdir3_carrier_requests));

    reshade::log::message(
        reshade::log::level::info,
        h3_line);

    const auto env_res =
        g_envspec_resources.telemetry();
    const auto env_draw =
        g_pmetal_envspec.telemetry();
    const auto env_source =
        g_pmetal_source.telemetry();
    const auto env_lerp =
        dsrrl::runtime::
            hemenvlerp_receiver_pipeline_stats();
    const auto pmetal_native =
        g_pmetal_native_draw.telemetry();

    char env_line[1792]{};
    std::snprintf(
        env_line,
        sizeof(env_line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s_ENVSPEC "
        "ps_mat=%llu/%llu src_steady=%llu src_blend=%llu src_miss=%llu src_hook=%u/%u "
        "native=%llu/%llu hash_miss=%llu views=%llu pack=%llu/%llu pack_ready=%u sampler=%u "
        "cube=%llu/%llu prepare=%llu/%llu candidate=%llu material_reject=%llu semantic_reject=%llu "
        "source_reject=%llu blend_hold=%llu probe_reject=%llu spec_reject=%llu req=%llu b12_map=%llu/%llu srv_shadow=%llu/%llu q=%u "
        "native_draw=%llu/%llu/%llu reject=%llu restore_fail=%llu hook=%u nq=%u "
        "lerp_reg=%llu/%llu lerp_candidate=%llu lerp_req=%llu lerp_pipe=%llu/%llu bind=%llu/%llu miss=%llu conflict=%llu",
        tag,
        static_cast<unsigned long long>(
            g_envspec_payload_materialize_ok.load()),
        static_cast<unsigned long long>(
            g_envspec_payload_materialize_fail.load()),
        static_cast<unsigned long long>(
            env_source.steady_seen),
        static_cast<unsigned long long>(
            env_source.blend_seen),
        static_cast<unsigned long long>(
            env_source.decode_fail),
        env_source.steady_carrier_active ? 1u : 0u,
        env_source.blend_carrier_active ? 1u : 0u,
        static_cast<unsigned long long>(
            env_res.native_candidates),
        static_cast<unsigned long long>(
            env_res.native_matches),
        static_cast<unsigned long long>(
            env_res.native_hash_miss),
        static_cast<unsigned long long>(
            env_res.view_matches),
        static_cast<unsigned long long>(
            env_res.pack_admit_ok),
        static_cast<unsigned long long>(
            env_res.pack_admit_fail),
        env_res.pack_ready ? 1u : 0u,
        env_res.sampler_ready ? 1u : 0u,
        static_cast<unsigned long long>(
            env_res.cube_created),
        static_cast<unsigned long long>(
            env_res.cube_fail),
        static_cast<unsigned long long>(
            env_res.prepare_ok),
        static_cast<unsigned long long>(
            env_res.prepare_fail),
        static_cast<unsigned long long>(
            env_draw.candidates),
        static_cast<unsigned long long>(
            env_draw.material_rejects),
        static_cast<unsigned long long>(
            env_draw.semantic_rejects),
        static_cast<unsigned long long>(
            env_draw.source_rejects),
        static_cast<unsigned long long>(
            env_draw.blended_receiver_hold),
        static_cast<unsigned long long>(
            env_draw.probe_rejects),
        static_cast<unsigned long long>(
            env_draw.spec_rgb_rejects),
        static_cast<unsigned long long>(
            env_draw.requests),
        static_cast<unsigned long long>(
            env_draw.b12_uploads),
        static_cast<unsigned long long>(
            env_draw.b12_reuses),
        static_cast<unsigned long long>(
            env_draw.srv_shadow_hits),
        static_cast<unsigned long long>(
            env_draw.srv_shadow_fallbacks),
        env_draw.quarantined ? 1u : 0u,
        static_cast<unsigned long long>(
            pmetal_native.armed),
        static_cast<unsigned long long>(
            pmetal_native.draw_applied),
        static_cast<unsigned long long>(
            pmetal_native.draw_indexed_applied),
        static_cast<unsigned long long>(
            pmetal_native.arm_reject),
        static_cast<unsigned long long>(
            pmetal_native.restore_fail),
        pmetal_native.hook_active ? 1u : 0u,
        pmetal_native.quarantined ? 1u : 0u,
        static_cast<unsigned long long>(
            env_draw.lerp_replacement_register_ok),
        static_cast<unsigned long long>(
            env_draw.lerp_replacement_register_fail),
        static_cast<unsigned long long>(
            env_draw.lerp_candidates),
        static_cast<unsigned long long>(
            env_draw.lerp_requests),
        static_cast<unsigned long long>(
            env_lerp.exact_hits),
        static_cast<unsigned long long>(
            env_lerp.hash_misses),
        static_cast<unsigned long long>(
            env_lerp.exact_binds),
        static_cast<unsigned long long>(
            env_lerp.unknown_binds),
        static_cast<unsigned long long>(
            env_lerp.lookup_misses),
        static_cast<unsigned long long>(
            env_lerp.handle_conflicts));

    reshade::log::message(
        reshade::log::level::info,
        env_line);

    const auto bloom_q8 =
        g_bloom_scene_sidecar.telemetry();

    char bloom_line[640]{};
    std::snprintf(
        bloom_line,
        sizeof(bloom_line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s_BLOOM_Q8 "
        "init=%llu create=%llu/%llu ready=%u auth=%llu/%llu armed=%u "
        "begin=%llu/%llu commit=%llu/%llu rtv=%llu srv=%llu acquire_fail=%llu "
        "valid=%u frame=%llu destroy=%llu",
        tag,
        static_cast<unsigned long long>(bloom_q8.init_calls),
        static_cast<unsigned long long>(bloom_q8.create_ok),
        static_cast<unsigned long long>(bloom_q8.create_fail),
        bloom_q8.resource_ready ? 1u : 0u,
        static_cast<unsigned long long>(bloom_q8.authorize_ok),
        static_cast<unsigned long long>(bloom_q8.authorize_fail),
        bloom_q8.proof_authorized ? 1u : 0u,
        static_cast<unsigned long long>(bloom_q8.begin_ok),
        static_cast<unsigned long long>(bloom_q8.begin_fail),
        static_cast<unsigned long long>(bloom_q8.commit_ok),
        static_cast<unsigned long long>(bloom_q8.commit_fail),
        static_cast<unsigned long long>(bloom_q8.rtv_acquire_ok),
        static_cast<unsigned long long>(bloom_q8.srv_acquire_ok),
        static_cast<unsigned long long>(bloom_q8.acquire_fail),
        bloom_q8.contents_valid ? 1u : 0u,
        static_cast<unsigned long long>(bloom_q8.frame_serial),
        static_cast<unsigned long long>(bloom_q8.destroy_calls));

    reshade::log::message(
        reshade::log::level::info,
        bloom_line);

    const auto bloom_fx =
        dsrrl::runtime::bloom_fx_draw_transport::status();

    char bloom_fx_line[1536]{};
    std::snprintf(
        bloom_fx_line,
        sizeof(bloom_fx_line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s_BLOOM_FX "
        "prov=%u hooks=%u/%u model=%u/%u state_upd=%u sem_get=%u q=%u restore_fail=%u "
        "events=%llu/%llu exact=%llu reject=%llu state=%llu/%llu "
        "state_exact=%llu/%llu links=%llu/%llu model_evt=%llu/%llu "
        "model_src=%llu/%llu model_join=%llu/%llu join_ch=%llu/%llu/%llu "
        "backend=%llu key=%llu/%llu ww_idx=%llu key_eq=%llu ww_diag_join=%llu sem_snap=%llu/%llu "
        "ww_publish=%llu/%llu ww_same=%llu registry=%llu "
        "snap=%llu/%llu api_snap=%llu ww_auth=%llu/%llu",
        tag,
        bloom_fx.provenance_ok ? 1u : 0u,
        bloom_fx.particle_hook_armed ? 1u : 0u,
        bloom_fx.cluster_hook_armed ? 1u : 0u,
        bloom_fx.particle_model_ctor_hook_armed ? 1u : 0u,
        bloom_fx.particle_model_dtor_hook_armed ? 1u : 0u,
        bloom_fx.particle_state_update_hook_armed ? 1u : 0u,
        bloom_fx.semantic_index_getter_attested ? 1u : 0u,
        bloom_fx.quarantined ? 1u : 0u,
        bloom_fx.restore_failed ? 1u : 0u,
        static_cast<unsigned long long>(bloom_fx.particle_events),
        static_cast<unsigned long long>(bloom_fx.cluster_events),
        static_cast<unsigned long long>(bloom_fx.exact_entity_hits),
        static_cast<unsigned long long>(bloom_fx.entity_rejects),
        static_cast<unsigned long long>(bloom_fx.state_ready_hits),
        static_cast<unsigned long long>(bloom_fx.state_missing),
        static_cast<unsigned long long>(bloom_fx.exact_state_hits),
        static_cast<unsigned long long>(bloom_fx.state_vtable_rejects),
        static_cast<unsigned long long>(bloom_fx.source_links_ready),
        static_cast<unsigned long long>(bloom_fx.source_links_missing),
        static_cast<unsigned long long>(bloom_fx.particle_model_ctor_events),
        static_cast<unsigned long long>(bloom_fx.particle_model_dtor_events),
        static_cast<unsigned long long>(bloom_fx.particle_model_source_attest_hits),
        static_cast<unsigned long long>(bloom_fx.particle_model_source_attest_misses),
        static_cast<unsigned long long>(bloom_fx.particle_model_join_hits),
        static_cast<unsigned long long>(bloom_fx.particle_model_join_misses),
        static_cast<unsigned long long>(bloom_fx.particle_model_owner_join_hits),
        static_cast<unsigned long long>(bloom_fx.particle_model_source_primary_join_hits),
        static_cast<unsigned long long>(bloom_fx.particle_model_source_secondary_join_hits),
        static_cast<unsigned long long>(bloom_fx.particle_state_update_events),
        static_cast<unsigned long long>(bloom_fx.backend_key_reads),
        static_cast<unsigned long long>(bloom_fx.backend_key_read_failures),
        static_cast<unsigned long long>(bloom_fx.waterwave_runtime_index_reads),
        static_cast<unsigned long long>(bloom_fx.backend_key_waterwave_matches),
        static_cast<unsigned long long>(bloom_fx.waterwave_semantic_model_candidate_hits),
        static_cast<unsigned long long>(bloom_fx.backend_semantic_snapshot_hits),
        static_cast<unsigned long long>(bloom_fx.backend_semantic_snapshot_misses),
        static_cast<unsigned long long>(bloom_fx.waterwave_publish_ok),
        static_cast<unsigned long long>(bloom_fx.waterwave_publish_fail),
        static_cast<unsigned long long>(bloom_fx.waterwave_same_instance_hits),
        static_cast<unsigned long long>(bloom_fx.particle_model_registry_size),
        static_cast<unsigned long long>(bloom_fx.snapshot_hits),
        static_cast<unsigned long long>(bloom_fx.snapshot_misses),
        static_cast<unsigned long long>(g_bloom_fx_draw_snapshots.load()),
        static_cast<unsigned long long>(g_bloom_fx_draw_authorized.load()),
        static_cast<unsigned long long>(g_bloom_fx_draw_rejected.load()));

    reshade::log::message(
        reshade::log::level::info,
        bloom_fx_line);
}

void on_init_device(reshade::api::device *device)
{
    g_a1_bridge.on_init_device(device);
    if (!k_drawtime_islands_runtime_enabled)
        return;

    // Q8 Bloom scene sidecar has no authorized production writer/consumer.
    // Keep its resource allocation diagnostic-only until that graph closes.
    if (g_hot_telemetry_enabled)
        g_bloom_scene_sidecar.on_init_device(device);
    g_mr_draw_runtime.on_init_device(device);
    g_pmetal_envspec.on_init_device(device);
    if (k_pmetal_direct_current_native_dispatch && k_pointlight_drawtime_runtime_enabled) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL POINTLIGHT R26] direct_current_native_dispatch=ACTIVE persistent_vtable_hook=OFF recursion_guard=ON");
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL POINTLIGHT R30] synchronous_core_transaction=ACTIVE exact_clustered_shape_guard=ON global_core_tx_mutex=OFF_FOR_CLUSTERED_DIRECT_NATIVE tls_reentry_guard=ON t18_t19_batch=ON sync_draw_serial_atomic=OFF fallback_general_tx=UNCHANGED");
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL POINTLIGHT R31] direct_source_class=DirectPointLightEntity dsr_vfunc_rva=0x55C570 ptde_homologue=0x00D34D50 carrier=position_invRange_RGB_End authorization=EXACT_ONLY");
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL POINTLIGHT R33] clustered_spc_protected_failopen=SUPERSEDED_BY_R35_OWNER_AUTH");
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL POINTLIGHT R34] source_material_split=ACTIVE source_stage=selector_source_event material_stage=selector_identity_event");
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL POINTLIGHT R35] owner_authorized_spc_hybrid=ACTIVE clustered_nospc_consumes_ptde_source=ON clustered_spc_consumes_ptde_source=ON spc_material_tail=STOCK_DSR_GGX_SCHLICK local_specular_equivalence=OPEN");
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL POINTLIGHT R36] bank_lerp_native_double_pack=OFF frame_source_cache=TLS_EXACT_STATE_PER_PRESENT donor_miss_stock_fallback=ON");
    } else if (k_pmetal_native_draw_runtime_enabled) {
        if (!g_pmetal_native_draw.install(device)) {
            reshade::log::message(
                reshade::log::level::warning,
                "[DSRRL RUNTIME V2] P_Metal native original-draw bridge unavailable; P_Metal falls back to the legacy replay path.");
        } else {
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL RUNTIME V2] Native original-draw bridge ACTIVE: exact immediate/deferred command-list contexts are registered; P_Metal/clustered PointLight state wraps the single original D3D11 Draw on that same context.");
        }
    }
    g_upper_lower_hemenv.on_init_device(device);
    g_hemdir3.on_init_device(device);
}

void on_destroy_device(reshade::api::device *device)
{
    dsrrl::runtime::pixel_srv_shadow_reset();

    if (k_drawtime_islands_runtime_enabled) {
        g_pmetal_native_draw.uninstall();
        g_draw_transactions.on_destroy_device(device);
        g_bloom_scene_sidecar.on_destroy_device(device);
        g_upper_lower.on_destroy_device(device);
        g_upper_lower_hemenv.on_destroy_device(device);
        g_hemdir3.on_destroy_device(device);
        g_pmetal_envspec.on_destroy_device(device);
        g_fixed_pointlight.on_destroy_device(device);
        g_fixed_pointlight_pipeline.on_destroy_device(device);
        g_clustered_pnts.on_destroy_device(device);
        g_clustered_pnts_pipeline.on_destroy_device(device);
        g_mr_draw_runtime.on_destroy_device(device);
    }
    g_a1_bridge.on_destroy_device(device);
}

void on_init_command_list(
    reshade::api::command_list *cmd_list)
{
    if (!k_drawtime_islands_runtime_enabled ||
        !k_pmetal_native_draw_runtime_enabled)
        return;

    (void)g_pmetal_native_draw.register_command_list(
        cmd_list);
}

void on_destroy_command_list(
    reshade::api::command_list *cmd_list)
{
    if (!k_drawtime_islands_runtime_enabled ||
        !k_pmetal_native_draw_runtime_enabled)
        return;

    g_pmetal_native_draw.unregister_command_list(
        cmd_list);
}

bool register_subsurface_plain_target_chain(
    const std::uint8_t *plain_source,
    std::size_t plain_size,
    std::uint32_t expected_receiver_id) noexcept
{
    if (plain_source == nullptr ||
        plain_size == 0u ||
        expected_receiver_id < 33u ||
        expected_receiver_id > 35u)
        return false;

    std::vector<std::uint8_t> mr_payload;
    const auto mr =
        dsrrl::operators::material_response::
            materialize_ptde_diffuse_response_v1(
                g_core.features(),
                plain_source,
                plain_size,
                mr_payload);

    using mr_result =
        dsrrl::operators::material_response::
            diffuse_v1_result;

    if (mr.result != mr_result::applied ||
        mr.family !=
            dsrrl::operators::material_response::
                diffuse_v1_family::stable_hemenv ||
        mr.receiver_id != expected_receiver_id)
        return false;

    // Subsurface removes only the DSR-only SSS fork. Its ordinary PTDE
    // surface target must preserve stock DSR Upper/Lower unless the
    // independently-authorized U/L island is active. Therefore the dedicated
    // SpecRGB consumer is derived directly from the stable MR target, never
    // from the MR+U/L composed shader and never from a b13 dependency.
    std::vector<std::uint8_t> subsurface_spec_payload;
    const auto spec_result =
        dsrrl::operators::resource_bridges::
            materialize_spec_rgb_consumer(
                mr_payload.data(),
                mr_payload.size(),
                subsurface_spec_payload,
                false);

    if (spec_result !=
        dsrrl::operators::resource_bridges::
            spec_rgb_consumer_result::applied)
        return false;

    const auto spec_owner =
        dsrrl::core::operator_bit(
            dsrrl::core::operator_id::spec_rgb);

    const bool mr_ok =
        g_mr_draw_runtime.register_receiver_replacement(
            expected_receiver_id,
            mr_payload.data(),
            mr_payload.size(),
            mr.composed_owners);
    const bool spec_ok =
        g_mr_draw_runtime.register_subsurface_spec_replacement(
            expected_receiver_id,
            subsurface_spec_payload.data(),
            subsurface_spec_payload.size(),
            mr.composed_owners |
                spec_owner);

    if (mr_ok)
        ++g_mr_payload_materialize_ok;
    else
        ++g_mr_payload_materialize_fail;

    if (spec_ok)
        ++g_subsurface_spec_payload_materialize_ok;

    return mr_ok && spec_ok;
}
bool on_create_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline_layout layout,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects)
{
    if (!k_drawtime_islands_runtime_enabled) {
        const bool a1_changed =
            g_a1_bridge.on_create_pipeline(
                device,
                layout,
                subobject_count,
                subobjects);
        const bool motion_blur_changed =
            motion_blur_camera_fallback_disable::
                on_create_pipeline(
                    subobject_count,
                    subobjects);
        return a1_changed || motion_blur_changed;
    }

    const auto *pixel_shader =
        find_pixel_shader(
            subobject_count,
            subobjects);

    // Create-pipeline carries the authoritative ReShade device for this PS.
    // Re-arm the two draw runtimes idempotently here so replacement
    // materialization cannot be lost solely because init_device ordering was
    // different on a host/runtime variant. A different native device still
    // quarantines through the runtimes' existing fail-open contract.
    if (pixel_shader != nullptr &&
        pixel_shader->code != nullptr &&
        pixel_shader->code_size != 0u) {
        g_mr_draw_runtime.on_init_device(device);
        g_pmetal_envspec.on_init_device(device);
    }

    dsrrl::operators::lightbank::
        hemdir3_b13_materialize_outcome h3{};
    dsrrl::operators::lightbank::
        upper_lower_hemenv_materialize_outcome ul{};
    bool h3_identity_ready = false;
    bool h3_replacement_ready = false;
    bool ul_identity_ready = false;
    bool ul_replacement_ready = false;
    dsrrl::operators::point_light::
        clustered_pnts_direct_materialize_outcome clustered_pnts{};
    std::vector<std::uint8_t> clustered_pnts_payload;
    bool clustered_pnts_candidate = false;

    if (pixel_shader != nullptr &&
        pixel_shader->code != nullptr &&
        pixel_shader->code_size != 0u) {
        const auto *source =
            static_cast<const std::uint8_t *>(
                pixel_shader->code);

        // Close the Subsurface creation-order gap at the source receiver
        // itself. For the three exact Subsurf shaders, reconstruct the
        // corresponding ordinary HemEnv target from the certified
        // source->target patch, verify its exact target SHA, then seed the
        // complete MR + U/L + SpecRGB replacement bank before this pipeline
        // can ever reach a draw. Unknown or malformed sources fail open.
        if (g_core.features().enabled(
                dsrrl::core::operator_id::subsurface) &&
            g_core.features().enabled(
                dsrrl::core::operator_id::material_response) &&
            g_core.features().enabled(
                dsrrl::core::operator_id::spec_rgb)) {
            std::vector<std::uint8_t> plain_target;
            const auto plain =
                dsrrl::operators::resource_bridges::
                    materialize_subsurface_plain_target(
                        source,
                        pixel_shader->code_size,
                        plain_target);

            using plain_result =
                dsrrl::operators::resource_bridges::
                    subsurface_plain_target_materialize_result;

            if (plain.result ==
                    plain_result::applied) {
                if (!register_subsurface_plain_target_chain(
                        plain_target.data(),
                        plain_target.size(),
                        plain.target_plain_receiver_id))
                    ++g_subsurface_spec_payload_materialize_fail;
            } else if (
                plain.result !=
                    plain_result::pass_not_candidate) {
                ++g_subsurface_spec_payload_materialize_fail;
            }
        }

        if (k_pointlight_drawtime_runtime_enabled) {
            clustered_pnts =
                dsrrl::operators::point_light::
                    materialize_clustered_pnts_direct_ptde(
                    source,
                    pixel_shader->code_size,
                    clustered_pnts_payload);
        clustered_pnts_candidate =
            clustered_pnts.result ==
                dsrrl::operators::point_light::
                    clustered_pnts_direct_materialize_result::applied;

        // Observe the exact original DSR local-specular host before any
        // create-time island is allowed to replace the shader bytes. This is
        // census-only: local_specular_legacy remains unarmed until the full
        // per-light microfacet window has an exact rewrite recipe.
        dsrrl::operators::point_light::
            local_specular_receiver_identity local_specular_identity{};
        if (dsrrl::operators::point_light::
                local_specular_receiver_for_shader(
                    source,
                    pixel_shader->code_size,
                    local_specular_identity)) {
            ++g_local_specular_receiver_hits;
            using local_class =
                dsrrl::operators::point_light::
                    local_specular_receiver_class;
            switch (local_specular_identity.receiver_class) {
            case local_class::clustered_spc_pnts:
                ++g_local_specular_clustered_hits;
                break;
            case local_class::fixed_spc_pntss:
                ++g_local_specular_fixed2_hits;
                break;
            case local_class::fixed_spc_pntssss:
                ++g_local_specular_fixed4_hits;
                break;
            default:
                break;
            }

            const auto window_scan =
                dsrrl::operators::point_light::
                    scan_local_specular_microfacet_windows(
                        source,
                        pixel_shader->code_size,
                        local_specular_identity.receiver_class);
            if (window_scan.result ==
                    dsrrl::operators::point_light::
                        local_specular_window_result::exact) {
                ++g_local_specular_window_pass;
                g_local_specular_windows_total.fetch_add(
                    window_scan.window_count);

                const auto fixed_plan =
                    dsrrl::operators::point_light::
                        build_fixed_local_specular_patch_plan_from_attested(
                            local_specular_identity,
                            window_scan);
                using plan_result =
                    dsrrl::operators::point_light::
                        fixed_local_specular_plan_result;
                if (fixed_plan.result == plan_result::ready) {
                    ++g_local_specular_fixed_plan_ready;
                    const auto operands =
                        dsrrl::operators::point_light::
                            extract_fixed_local_specular_operand_contract(
                                source,
                                pixel_shader->code_size);
                    if (operands.result ==
                            dsrrl::operators::point_light::
                                fixed_local_specular_operand_result::ready) {
                        ++g_local_specular_operand_ready;
                        const auto output_cut =
                            dsrrl::operators::point_light::
                                locate_fixed_local_specular_output_cut(
                                    source,
                                    pixel_shader->code_size);
                        if (output_cut.result ==
                                dsrrl::operators::point_light::
                                    fixed_local_specular_output_cut_result::exact) {
                            ++g_local_specular_output_cut_ready;
                            const auto island_plan =
                                dsrrl::operators::point_light::
                                    build_fixed_local_specular_island_plan(
                                        source,
                                        pixel_shader->code_size);
                            if (island_plan.result ==
                                    dsrrl::operators::point_light::
                                        fixed_local_specular_island_plan_result::ready) {
                                ++g_local_specular_island_plan_ready;

                                // Construction probe only. Build the exact
                                // replacement from the live original DXBC but
                                // do not expose it to ReShade until draw-local
                                // t19+b12+SpecRGB readiness is proven.
                                std::vector<std::uint8_t> fixed_payload;
                                const auto fixed_materialized =
                                    dsrrl::operators::point_light::
                                        materialize_fixed_local_specular_single(
                                            g_core.features(),
                                            source,
                                            pixel_shader->code_size,
                                            fixed_payload);
                                using fixed_result =
                                    dsrrl::operators::point_light::
                                        fixed_local_single_materialize_result;
                                if (fixed_materialized.result ==
                                        fixed_result::applied) {
                                    if (g_fixed_pointlight_pipeline.
                                            register_candidate(
                                                device,
                                                fixed_materialized,
                                                fixed_payload.data(),
                                                fixed_payload.size()))
                                        ++g_local_specular_single_materialize_ok;
                                    else
                                        ++g_local_specular_materialize_fail;
                                }
                                else if (fixed_materialized.result ==
                                        fixed_result::
                                            pass_blended_requires_endpoint_b)
                                    ++g_local_specular_blended_defer;
                                else
                                    ++g_local_specular_materialize_fail;
                            } else {
                                ++g_local_specular_island_plan_fail;
                            }
                        } else {
                            ++g_local_specular_output_cut_fail;
                        }
                    } else {
                        ++g_local_specular_operand_fail;
                    }
                } else if (fixed_plan.result ==
                           plan_result::
                               pass_clustered_membership_not_owned) {
                    ++g_local_specular_clustered_deferred;
                } else {
                    ++g_local_specular_fixed_plan_fail;
                }
            } else {
                ++g_local_specular_window_fail;
                ++g_local_specular_fixed_plan_fail;
            }
        }
        }

        std::vector<std::uint8_t> mr_payload;
        const auto mr =
            dsrrl::operators::material_response::
                materialize_ptde_diffuse_response_v1(
                    g_core.features(),
                    source,
                    pixel_shader->code_size,
                    mr_payload);

        using mr_result =
            dsrrl::operators::material_response::
                diffuse_v1_result;

        if (mr.result == mr_result::applied &&
            mr.family ==
                dsrrl::operators::material_response::
                    diffuse_v1_family::stable_hemenv) {
            // Base MR always preserves stock t1. SpecRGB is a paired shader
            // variant selected only after the draw-local t10 carrier succeeds.
            if (g_mr_draw_runtime.register_receiver_replacement(
                    mr.receiver_id,
                    mr_payload.data(),
                    mr_payload.size(),
                    mr.composed_owners))
                ++g_mr_payload_materialize_ok;
            else
                ++g_mr_payload_materialize_fail;

            // Subsurface is the only post-reset stable-HemEnv route allowed
            // to pair generic diffuse-v1 with a t10 SpecRGB consumer. Keep it
            // operator-local: derive from the stable MR target so stock DSR
            // Upper/Lower remains untouched while the SSS fork is bypassed.
            bool subsurface_target = false;
            for (const auto &route :
                 dsrrl::operators::resource_bridges::
                     k_subsurface_receiver_routes) {
                if (route.target_plain_receiver_id ==
                    mr.receiver_id) {
                    subsurface_target = true;
                    break;
                }
            }

            if (subsurface_target &&
                g_core.features().enabled(
                    dsrrl::core::operator_id::subsurface) &&
                g_core.features().enabled(
                    dsrrl::core::operator_id::spec_rgb)) {
                std::vector<std::uint8_t>
                    subsurface_spec_payload;
                const auto spec_result =
                    dsrrl::operators::resource_bridges::
                        materialize_spec_rgb_consumer(
                            mr_payload.data(),
                            mr_payload.size(),
                            subsurface_spec_payload,
                            false);

                if (spec_result ==
                    dsrrl::operators::resource_bridges::
                        spec_rgb_consumer_result::applied) {
                    const auto spec_owner =
                        dsrrl::core::operator_bit(
                            dsrrl::core::operator_id::
                                spec_rgb);
                    if (g_mr_draw_runtime.
                            register_subsurface_spec_replacement(
                                mr.receiver_id,
                                subsurface_spec_payload.data(),
                                subsurface_spec_payload.size(),
                                mr.composed_owners |
                                    spec_owner))
                        ++g_subsurface_spec_payload_materialize_ok;
                    else
                        ++g_subsurface_spec_payload_materialize_fail;
                } else {
                    ++g_subsurface_spec_payload_materialize_fail;
                }
            }

            // Upper/Lower composition is construction work for an optional
            // island. When U/L is disabled, do not build/register dead shader
            // variants while an area is streaming.
            if (g_core.features().enabled(
                    dsrrl::core::operator_id::upper_lower)) {
                std::vector<std::uint8_t> mr_ul_payload;
            const auto mr_ul =
                dsrrl::operators::lightbank::
                    augment_upper_lower_hemenv_verified_base(
                        source,
                        pixel_shader->code_size,
                        mr_payload.data(),
                        mr_payload.size(),
                        4u,
                        mr_ul_payload);

            if (mr_ul.result ==
                    dsrrl::operators::lightbank::
                        upper_lower_hemenv_materialize_result::applied &&
                mr_ul.stratum ==
                    dsrrl::operators::lightbank::
                        upper_lower_hemenv_stratum::spc &&
                mr_ul.stable_receiver_id ==
                    mr.receiver_id) {
                if (g_mr_draw_runtime.
                        register_receiver_upper_lower_replacement(
                            mr.receiver_id,
                            mr_ul_payload.data(),
                            mr_ul_payload.size(),
                            mr.composed_owners))
                    ++g_mr_ul_payload_materialize_ok;
                else
                    ++g_mr_ul_payload_materialize_fail;
            } else if (
                mr_ul.result !=
                    dsrrl::operators::lightbank::
                        upper_lower_hemenv_materialize_result::
                            pass_not_candidate &&
                mr_ul.result !=
                    dsrrl::operators::lightbank::
                        upper_lower_hemenv_materialize_result::
                            pass_unknown_exact_sha) {
                ++g_mr_ul_payload_materialize_fail;
            }
            }
        } else if (
            mr.result != mr_result::pass_not_candidate &&
            mr.result != mr_result::pass_unknown_exact_sha &&
            !(mr.result == mr_result::applied &&
              mr.family ==
                  dsrrl::operators::material_response::
                      diffuse_v1_family::hemenvlerp)) {
            ++g_mr_payload_materialize_fail;
        }

        // The single diffuse-v1 materialization above already classifies
        // stable HemEnv versus HemEnvLerp. Reuse its exact output instead of
        // reparsing/rehashing the same DXBC a second time during streaming.
        const auto &lerp_mr = mr;
        const auto &lerp_mr_payload = mr_payload;
        using lerp_mr_result = mr_result;

        if (lerp_mr.result == lerp_mr_result::applied &&
            lerp_mr.family ==
                dsrrl::operators::material_response::
                    diffuse_v1_family::hemenvlerp) {
            const std::uint32_t lerp_receiver_id =
                lerp_mr.receiver_id;

            // Material Response is operator-independent from U/L readiness.
            // Always register the certified Lerp MR-only replacement first.
            if (g_mr_draw_runtime.
                    register_lerp_receiver_replacement(
                        lerp_receiver_id,
                        lerp_mr_payload.data(),
                        lerp_mr_payload.size(),
                        lerp_mr.composed_owners))
                ++g_mr_payload_materialize_ok;
            else
                ++g_mr_payload_materialize_fail;

            // MR reset: do not pair PTDE SpecRGB with the stock DSR
            // HemEnvLerp PBL tail. SpecRGB is operator-local elsewhere.

            // U/L is optional and currently disabled by policy. Build its
            // composed Lerp variant only when that island is explicitly on.
            if (g_core.features().enabled(
                    dsrrl::core::operator_id::upper_lower)) {
                std::vector<std::uint8_t> lerp_mr_ul_payload;
            const auto lerp_mr_ul =
                dsrrl::operators::lightbank::
                    augment_upper_lower_hemenv_verified_base(
                        source,
                        pixel_shader->code_size,
                        lerp_mr_payload.data(),
                        lerp_mr_payload.size(),
                        4u,
                        lerp_mr_ul_payload);

            if (lerp_mr_ul.result ==
                    dsrrl::operators::lightbank::
                        upper_lower_hemenv_materialize_result::applied &&
                lerp_mr_ul.family ==
                    dsrrl::operators::lightbank::
                        upper_lower_hemenv_family::hemenvlerp &&
                lerp_mr_ul.stratum ==
                    dsrrl::operators::lightbank::
                        upper_lower_hemenv_stratum::spc &&
                lerp_mr_ul.stable_receiver_id ==
                    lerp_receiver_id) {
                if (g_mr_draw_runtime.
                        register_lerp_receiver_upper_lower_replacement(
                            lerp_receiver_id,
                            lerp_mr_ul_payload.data(),
                            lerp_mr_ul_payload.size(),
                            lerp_mr.composed_owners))
                    ++g_mr_ul_payload_materialize_ok;
                else
                    ++g_mr_ul_payload_materialize_fail;

                // No generic Lerp MR+SpecRGB variant.
            } else if (
                lerp_mr_ul.result !=
                    dsrrl::operators::lightbank::
                        upper_lower_hemenv_materialize_result::
                            pass_not_candidate &&
                lerp_mr_ul.result !=
                    dsrrl::operators::lightbank::
                        upper_lower_hemenv_materialize_result::
                            pass_unknown_exact_sha) {
                ++g_mr_ul_payload_materialize_fail;
            }
            }
        } else if (
            lerp_mr.result != lerp_mr_result::pass_not_candidate &&
            lerp_mr.result != lerp_mr_result::pass_unknown_exact_sha &&
            !(lerp_mr.result == lerp_mr_result::applied &&
              lerp_mr.family ==
                  dsrrl::operators::material_response::
                      diffuse_v1_family::stable_hemenv)) {
            ++g_mr_payload_materialize_fail;
        }

        // Exact P_Metal EnvSpec is now built from the exact stock shader via
        // the clean diffuse-v1 operator base. No V2.10/V2.11 F0 proxy is
        // consumed or authorized anywhere in this construction.
        if (g_core.features().enabled(
                dsrrl::core::operator_id::env_spec)) {
            std::vector<std::uint8_t>
                envspec_payload;

            const auto envspec =
                dsrrl::operators::env_spec::
                    materialize_pmetal_rgba_receiver(
                        g_core.features(),
                        source,
                        pixel_shader->code_size,
                        false,
                        envspec_payload);

            using envspec_result =
                dsrrl::operators::env_spec::
                    pmetal_rgba_materialize_result;

            if (envspec.result ==
                envspec_result::applied) {
                const bool registered =
                    g_pmetal_envspec.
                        register_replacement(
                            envspec,
                            envspec_payload.data(),
                            envspec_payload.size());
                if (registered) {
                    ++g_envspec_payload_materialize_ok;
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
                    if (!g_pmetal_full_ptde_create_ok_logged.exchange(
                            true,
                            std::memory_order_relaxed)) {
                        char line[256]{};
                        std::snprintf(
                            line,
                            sizeof(line),
                            "[DSRRL PMETAL FULL PTDE HEMENV] create_materialized rx=%u bytes=%zu",
                            static_cast<unsigned>(
                                envspec.receiver_id),
                            envspec_payload.size());
                        reshade::log::message(
                            reshade::log::level::info,
                            line);
                    }
#endif
                } else {
                    ++g_envspec_payload_materialize_fail;
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
                    if (!g_pmetal_full_ptde_create_fail_logged.exchange(
                            true,
                            std::memory_order_relaxed))
                        reshade::log::message(
                            reshade::log::level::warning,
                            "[DSRRL PMETAL FULL PTDE HEMENV] create_register_reject");
#endif
                }
            } else if (
                envspec.result !=
                    envspec_result::pass_not_candidate &&
                envspec.result !=
                    envspec_result::pass_unknown_exact_sha) {
                ++g_envspec_payload_materialize_fail;
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
                if (!g_pmetal_full_ptde_create_fail_logged.exchange(
                        true,
                        std::memory_order_relaxed)) {
                    char line[256]{};
                    std::snprintf(
                        line,
                        sizeof(line),
                        "[DSRRL PMETAL FULL PTDE HEMENV] create_materialize_fail result=%u rx=%u",
                        static_cast<unsigned>(
                            envspec.result),
                        static_cast<unsigned>(
                            envspec.receiver_id));
                    reshade::log::message(
                        reshade::log::level::warning,
                        line);
                }
#endif
            }

            std::vector<std::uint8_t>
                envspec_lerp_payload;
            const auto envspec_lerp =
                dsrrl::operators::env_spec::
                    materialize_pmetal_rgba_lerp_receiver(
                        g_core.features(),
                        source,
                        pixel_shader->code_size,
                        envspec_lerp_payload);

            using envspec_lerp_result =
                dsrrl::operators::env_spec::
                    pmetal_rgba_lerp_materialize_result;

            if (envspec_lerp.result ==
                    envspec_lerp_result::applied) {
                if (g_pmetal_envspec.
                        register_lerp_replacement(
                            envspec_lerp,
                            envspec_lerp_payload.data(),
                            envspec_lerp_payload.size()))
                    ++g_envspec_payload_materialize_ok;
                else
                    ++g_envspec_payload_materialize_fail;
            } else if (
                envspec_lerp.result !=
                    envspec_lerp_result::pass_not_candidate &&
                envspec_lerp.result !=
                    envspec_lerp_result::pass_unknown_exact_sha) {
                ++g_envspec_payload_materialize_fail;
            }
        }

        if (g_core.features().enabled(
                dsrrl::core::operator_id::upper_lower)) {
            std::vector<std::uint8_t> ul_payload;
            ul =
                dsrrl::operators::lightbank::
                    materialize_upper_lower_hemenv_receiver(
                        g_core.features(),
                        source,
                        pixel_shader->code_size,
                        ul_payload);

            if (ul.result ==
                dsrrl::operators::lightbank::
                    upper_lower_hemenv_materialize_result::applied) {
                ul_identity_ready = true;
                ul_replacement_ready =
                    g_upper_lower_hemenv.register_replacement(
                        ul,
                        ul_payload.data(),
                        ul_payload.size());
            }
        }

        if (g_core.features().enabled(
                dsrrl::core::operator_id::hemdir3)) {
            std::vector<std::uint8_t> h3_payload;
            h3 =
                dsrrl::operators::lightbank::
                    materialize_hemdir3_b13_receiver(
                        g_core.features(),
                        source,
                        pixel_shader->code_size,
                        h3_payload);

            if (h3.result ==
                dsrrl::operators::lightbank::
                    hemdir3_b13_materialize_result::applied) {
                h3_identity_ready = true;
                h3_replacement_ready =
                    g_hemdir3.register_replacement(
                        h3,
                        h3_payload.data(),
                        h3_payload.size());
            }
        }
    }

    const bool a1_changed =
        g_a1_bridge.on_create_pipeline(
            device,
            layout,
            subobject_count,
            subobjects);

    if (k_pointlight_drawtime_runtime_enabled &&
        clustered_pnts_candidate) {
        const auto *attested_host =
            find_pixel_shader(
                subobject_count,
                subobjects);
        if (attested_host == nullptr ||
            attested_host->code == nullptr ||
            attested_host->code_size == 0u ||
            !g_clustered_pnts_pipeline.register_candidate(
                device,
                clustered_pnts,
                static_cast<const std::uint8_t *>(
                    attested_host->code),
                attested_host->code_size,
                clustered_pnts_payload.data(),
                clustered_pnts_payload.size()))
            clustered_pnts_candidate = false;
    }

    if (ul_identity_ready) {
        const auto *created_shader =
            find_pixel_shader(
                subobject_count,
                subobjects);

        if (created_shader == nullptr ||
            created_shader->code == nullptr ||
            created_shader->code_size == 0u) {
            ul_identity_ready = false;
        } else {
            dsrrl::runtime::upper_lower_receiver_identity identity{};
            identity.plan_index = ul.plan_index;
            identity.shader_index = ul.shader_index;
            identity.stable_receiver_id =
                ul.stable_receiver_id;
            identity.stratum = ul.stratum;
            identity.family = ul.family;

            ul_identity_ready =
                dsrrl::runtime::
                    upper_lower_receiver_attest_created_code(
                        created_shader->code,
                        created_shader->code_size,
                        identity);
        }
    }

    if (h3_identity_ready) {
        const auto *created_shader =
            find_pixel_shader(
                subobject_count,
                subobjects);

        if (created_shader == nullptr ||
            created_shader->code == nullptr ||
            created_shader->code_size == 0u) {
            h3_identity_ready = false;
        } else {
            dsrrl::runtime::hemdir3_receiver_identity identity{};
            identity.plan_index = h3.plan_index;
            identity.shader_index = h3.shader_index;
            identity.stratum = h3.stratum;
            identity.paired_stable_receiver_id =
                h3.paired_stable_receiver_id;

            h3_identity_ready =
                dsrrl::runtime::
                    hemdir3_receiver_attest_created_code(
                        created_shader->code,
                        created_shader->code_size,
                        identity);
        }
    }

    const bool motion_blur_changed =
        motion_blur_camera_fallback_disable::
            on_create_pipeline(
                subobject_count,
                subobjects);

    (void)ul_identity_ready;
    (void)ul_replacement_ready;
    (void)h3_identity_ready;
    (void)h3_replacement_ready;
    return a1_changed || motion_blur_changed;
}

void on_init_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline_layout layout,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects,
    reshade::api::pipeline pipeline)
{
    g_a1_bridge.on_init_pipeline(
        device, layout, subobject_count, subobjects, pipeline);
    const bool fixed_pointlight_init_exact =
        k_pointlight_drawtime_runtime_enabled &&
        g_fixed_pointlight_pipeline.on_init_pipeline(
            device, subobject_count, subobjects, pipeline);
    const bool clustered_pointlight_init_exact =
        k_pointlight_drawtime_runtime_enabled &&
        g_clustered_pnts_pipeline.on_init_pipeline(
            device, subobject_count, subobjects, pipeline);
    motion_blur_camera_fallback_disable::
        on_init_pipeline(
            subobject_count,
            subobjects,
            pipeline);

    if (!k_drawtime_islands_runtime_enabled)
        return;

    const auto *pixel_shader =
        find_pixel_shader(
            subobject_count,
            subobjects);

    std::uint8_t draw_route_mask = 0u;

    if (g_a1_bridge.pipeline_attested(
            pipeline.handle))
        draw_route_mask |= k_route_a1;

    // Carry the exact PointLight result directly out of the init-time
    // registry transaction. This avoids re-looking up the same pipeline and
    // makes the integrated route map authoritative before any draw bind.
    if (fixed_pointlight_init_exact)
        draw_route_mask |=
            k_route_fixed_pointlight;

    if (clustered_pointlight_init_exact)
        draw_route_mask |=
            k_route_clustered_pointlight;

    if (pixel_shader != nullptr &&
        pixel_shader->code != nullptr &&
        pixel_shader->code_size != 0u) {
        if (dsrrl::runtime::
                stable_receiver_observe_pipeline(
                    pipeline.handle,
                    pixel_shader->code,
                    pixel_shader->code_size))
            draw_route_mask |= k_route_stable;

        if (dsrrl::runtime::
                hemenvlerp_receiver_observe_pipeline(
                    pipeline.handle,
                    pixel_shader->code,
                    pixel_shader->code_size))
            draw_route_mask |= k_route_hemenvlerp;

        if (g_core.features().enabled(
                dsrrl::core::operator_id::subsurface) &&
            dsrrl::runtime::
                subsurface_receiver_observe_pipeline(
                    pipeline.handle,
                    pixel_shader->code,
                    pixel_shader->code_size))
            draw_route_mask |= k_route_subsurface;

        if (g_core.features().enabled(
                dsrrl::core::operator_id::hemdir3) &&
            dsrrl::runtime::
                hemdir3_receiver_observe_pipeline(
                    pipeline.handle,
                    pixel_shader->code,
                    pixel_shader->code_size))
            draw_route_mask |= k_route_hemdir3;

        if (g_core.features().enabled(
                dsrrl::core::operator_id::upper_lower) &&
            dsrrl::runtime::
                upper_lower_receiver_observe_pipeline(
                    pipeline.handle,
                    pixel_shader->code,
                    pixel_shader->code_size))
            draw_route_mask |= k_route_upper_lower;
    }

    remember_integrated_draw_route(
        pipeline.handle,
        draw_route_mask);
}

void on_destroy_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline pipeline)
{
    if (k_drawtime_islands_runtime_enabled) {
        forget_integrated_draw_route(
            pipeline.handle);
        dsrrl::runtime::stable_receiver_forget_pipeline(
            pipeline.handle);
        dsrrl::runtime::hemenvlerp_receiver_forget_pipeline(
            pipeline.handle);
        dsrrl::runtime::subsurface_receiver_forget_pipeline(
            pipeline.handle);
        dsrrl::runtime::hemdir3_receiver_forget_pipeline(
            pipeline.handle);
        dsrrl::runtime::upper_lower_receiver_forget_pipeline(
            pipeline.handle);
        g_fixed_pointlight_pipeline.on_destroy_pipeline(pipeline);
        g_clustered_pnts_pipeline.on_destroy_pipeline(pipeline);
    }
    motion_blur_camera_fallback_disable::
        on_destroy_pipeline(
            pipeline);
    g_a1_bridge.on_destroy_pipeline(device, pipeline);
}

void on_bind_pipeline(
    reshade::api::command_list *cmd_list,
    reshade::api::pipeline_stage stages,
    reshade::api::pipeline pipeline)
{
    const bool pixel_stage_bound =
        (static_cast<std::uint32_t>(stages) &
         static_cast<std::uint32_t>(
             reshade::api::pipeline_stage::pixel_shader)) != 0u;

    motion_blur_camera_fallback_disable::
        on_bind_pipeline(
            stages,
            pipeline);

    if (!k_drawtime_islands_runtime_enabled)
        return;

    auto route_mask =
        observe_integrated_draw_route_bind(
            cmd_list,
            pixel_stage_bound,
            pipeline.handle);

    if (pixel_stage_bound && route_mask != 0u) {
        if ((route_mask & k_route_stable) != 0u)
            dsrrl::runtime::stable_receiver_observe_bind(
                cmd_list, true, pipeline.handle);
        if ((route_mask & k_route_hemenvlerp) != 0u)
            dsrrl::runtime::hemenvlerp_receiver_observe_bind(
                cmd_list, true, pipeline.handle);
        if ((route_mask & k_route_subsurface) != 0u)
            dsrrl::runtime::subsurface_receiver_observe_bind(
                cmd_list, true, pipeline.handle);
        if ((route_mask & k_route_hemdir3) != 0u)
            dsrrl::runtime::hemdir3_receiver_observe_bind(
                cmd_list, true, pipeline.handle);
        if ((route_mask & k_route_upper_lower) != 0u)
            dsrrl::runtime::upper_lower_receiver_observe_bind(
                cmd_list, true, pipeline.handle);
    }

    // PointLight bind work is restricted to pipelines that were attested by
    // the exact registry during init. Never query the PointLight registries on
    // unrelated pixel binds.
    if (pixel_stage_bound &&
        (route_mask &
         k_route_fixed_pointlight) != 0u)
        (void)g_fixed_pointlight_pipeline.on_bind_pipeline(
            cmd_list,
            stages,
            pipeline);

    if (pixel_stage_bound &&
        (route_mask &
         k_route_clustered_pointlight) != 0u)
        (void)g_clustered_pnts_pipeline.on_bind_pipeline(
            cmd_list,
            stages,
            pipeline);

    std::uint16_t first_plan = 0xFFFFu;
    dsrrl::core::operator_mask selected_owners = 0u;
    std::uint16_t selected_ops = 0u;
    std::uint32_t receiver_id = 0u;

    const bool target =
        pixel_stage_bound &&
        (route_mask & k_route_a1) != 0u &&
        g_a1_bridge.on_bind_pipeline(
            stages,
            pipeline,
            &first_plan,
            &selected_owners,
            &selected_ops,
            &receiver_id);

    if (pixel_stage_bound)
        observe_integrated_a1_owner_bind(
            cmd_list,
            target ? selected_owners : 0u);

    if (target) {
        const auto a1_effects =
            static_shader_owner_effect_mask(
                selected_owners);
        mark_effect_probe_mask(
            a1_effects,
            effect_probe_stage::candidate,
            receiver_id);
        mark_effect_probe_mask(
            a1_effects,
            effect_probe_stage::authority,
            receiver_id);
        mark_effect_probe_mask(
            a1_effects,
            effect_probe_stage::prepared,
            receiver_id);
    }

    if (target && first_plan != 0xFFFFu) {
        char line[240]{};
        std::snprintf(
            line,
            sizeof(line),
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] "
            "FIRST_BIND plan=%u receiver=%u owners=0x%08X ops=%u",
            static_cast<unsigned>(first_plan),
            static_cast<unsigned>(receiver_id),
            static_cast<unsigned>(selected_owners),
            static_cast<unsigned>(selected_ops));
        reshade::log::message(reshade::log::level::info, line);
    }
}

struct prepared_island_batch {
    dsrrl::runtime::island_draw_batch batch{};
    dsrrl::runtime::prepared_material_response_draw mr{};
    dsrrl::runtime::prepared_material_resource_draw resources{};
    dsrrl::runtime::prepared_subsurface_draw subsurface{};
    dsrrl::runtime::prepared_hemdir3_draw hemdir3{};
    dsrrl::runtime::prepared_upper_lower_hemenv_draw upper_lower{};
    dsrrl::runtime::prepared_pmetal_envspec_draw envspec{};
    dsrrl::runtime::prepared_fixed_pointlight_shader fixed_shader{};
    dsrrl::runtime::prepared_fixed_pointlight_draw fixed_carrier{};
    ID3D11Buffer *fixed_b12 = nullptr;
    dsrrl::runtime::prepared_clustered_pnts_shader clustered_shader{};
    dsrrl::runtime::prepared_clustered_pnts_draw clustered_carrier{};
    bool clustered_in_batch = false;
    bool clustered_neutral_noop = false;
    bool fixed_in_batch = false;
    bool mr_in_batch = false;
    bool lerp_mr_in_batch = false;
    bool upper_lower_combined = false;
    bool subsurface_in_batch = false;
    bool hemdir3_in_batch = false;
    bool envspec_in_batch = false;
};

effect_probe_mask resource_effect_mask(
    const dsrrl::runtime::prepared_material_resource_draw &resources) noexcept
{
    effect_probe_mask mask = 0u;
    if (resources.spec_rgb)
        mask |= effect_probe_bit(
            effect_probe_id::spec_rgb);
    if (resources.diffuse)
        mask |= effect_probe_bit(
            effect_probe_id::diffuse);
    if (resources.normal)
        mask |= effect_probe_bit(
            effect_probe_id::normal);
    return mask;
}

effect_probe_mask prepared_effect_mask(
    const prepared_island_batch &prepared) noexcept
{
    effect_probe_mask mask = 0u;

    if (prepared.mr_in_batch)
        mask |= effect_probe_bit(
            effect_probe_id::material_response);

    if (prepared.lerp_mr_in_batch)
        mask |= effect_probe_bit(
            effect_probe_id::material_response_lerp);

    mask |= resource_effect_mask(
        prepared.resources);

    if (prepared.upper_lower_combined ||
        prepared.upper_lower.ready)
        mask |= effect_probe_bit(
            effect_probe_id::upper_lower);

    if (prepared.subsurface_in_batch) {
        mask |= effect_probe_bit(
            effect_probe_id::subsurface);
        mask |= effect_probe_bit(
            effect_probe_id::material_response);
        mask |= resource_effect_mask(
            prepared.subsurface.resources);
        // Subsurface is isolated from the visible Upper/Lower bridge. Do not
        // report U/L as prepared/applied merely because Subsurface is active.
    }

    if (prepared.hemdir3_in_batch) {
        mask |= effect_probe_bit(
            effect_probe_id::hemdir3);
        if (prepared.hemdir3.carrier.upper_lower_ready)
            mask |= effect_probe_bit(
                effect_probe_id::upper_lower);
    }

    if (prepared.envspec_in_batch) {
        mask |= effect_probe_bit(
            effect_probe_id::pmetal_envspec);
        mask |= effect_probe_bit(
            effect_probe_id::material_response);
        mask |= resource_effect_mask(
            prepared.envspec.material_resources);
        // P_Metal EnvSpec is U/L-independent by construction. Never promote
        // UpperLower telemetry from an EnvSpec batch.
    }

    if (prepared.clustered_in_batch) {
        mask |= static_shader_owner_effect_mask(
            prepared.clustered_shader.
                composed_shader_owners);
        mask |= effect_probe_bit(
            effect_probe_id::pointlight);
        if (prepared.clustered_shader.spc)
            mask |= effect_probe_bit(
                effect_probe_id::local_specular);
        mask |= effect_probe_bit(
            effect_probe_id::material_response);
        mask |= resource_effect_mask(
            prepared.resources);
    }

    if (prepared.fixed_in_batch) {
        mask |= effect_probe_bit(
            effect_probe_id::pointlight);
        mask |= effect_probe_bit(
            effect_probe_id::local_specular);
        mask |= effect_probe_bit(
            effect_probe_id::material_response);
        mask |= resource_effect_mask(
            prepared.resources);
    }

    return mask;
}

effect_probe_mask route_candidate_effect_mask(
    std::uint8_t route_mask) noexcept
{
    effect_probe_mask mask = 0u;

    if ((route_mask &
         (k_route_stable |
          k_route_hemenvlerp |
          k_route_subsurface |
          k_route_fixed_pointlight)) != 0u)
        mask |= effect_probe_bit(
            effect_probe_id::material_response);

    if ((route_mask &
         k_route_hemenvlerp) != 0u)
        mask |= effect_probe_bit(
            effect_probe_id::material_response_lerp);

    if ((route_mask &
         k_route_upper_lower) != 0u)
        mask |= effect_probe_bit(
            effect_probe_id::upper_lower);

    if ((route_mask &
         k_route_subsurface) != 0u)
        mask |= effect_probe_bit(
            effect_probe_id::subsurface);

    if ((route_mask &
         k_route_hemdir3) != 0u)
        mask |= effect_probe_bit(
            effect_probe_id::hemdir3);

    if ((route_mask &
         k_route_fixed_pointlight) != 0u) {
        mask |= effect_probe_bit(
            effect_probe_id::pointlight);
        mask |= effect_probe_bit(
            effect_probe_id::local_specular);
    }

    if ((route_mask &
         k_route_clustered_pointlight) != 0u)
        mask |= effect_probe_bit(
            effect_probe_id::pointlight);

    return mask;
}

effect_probe_mask candidate_effect_mask(
    std::uint8_t route_mask,
    bool fixed_pointlight_bound,
    bool clustered_pointlight_bound,
    bool clustered_pointlight_spc,
    bool hemenvlerp_bound,
    bool subsurface_bound,
    bool hemdir3_bound,
    bool upper_lower_bound,
    const dsrrl::operators::material_response::
        material_identity &material,
    const dsrrl::operators::material_response::
        decision &decision) noexcept
{
    effect_probe_mask mask = 0u;

    if ((route_mask &
         (k_route_stable |
          k_route_hemenvlerp |
          k_route_subsurface |
          k_route_fixed_pointlight)) != 0u)
        mask |= effect_probe_bit(
            effect_probe_id::material_response);

    if (hemenvlerp_bound)
        mask |= effect_probe_bit(
            effect_probe_id::material_response_lerp);

    if (upper_lower_bound ||
        g_upper_lower.direct_producer_active())
        mask |= effect_probe_bit(
            effect_probe_id::upper_lower);

    if (subsurface_bound)
        mask |= effect_probe_bit(
            effect_probe_id::subsurface);

    if (hemdir3_bound)
        mask |= effect_probe_bit(
            effect_probe_id::hemdir3);

    if (fixed_pointlight_bound) {
        mask |= effect_probe_bit(
            effect_probe_id::pointlight);
        mask |= effect_probe_bit(
            effect_probe_id::local_specular);
    }

    if (clustered_pointlight_bound) {
        mask |= effect_probe_bit(
            effect_probe_id::pointlight);
        if (clustered_pointlight_spc)
            mask |= effect_probe_bit(
                effect_probe_id::local_specular);
    }

    const bool equipment_owner_exact =
        material.valid &&
        material.owner_tuple_exact &&
        material.material_slot_valid;

    const bool pmetal_envspec_candidate =
        decision.active &&
        decision.route_index == 345u &&
        decision.receiver_id >= 33u &&
        decision.receiver_id <= 35u;

    // PTDE SpecRGB sidecars are equipment-only resources, but SpecRGB is a
    // carrier rather than a generic visible material effect. Do not pre-mark
    // it as an effect candidate here. The effect matrix promotes SpecRGB only
    // when an explicit consumer actually materializes a retained resource
    // request (fixed local-specular, P_Metal EnvSpec or DSBT Subsurface).
    // Carrier liveness is observed separately without mutating draw state.
    if (equipment_owner_exact) {
        mask |= effect_probe_bit(
            effect_probe_id::diffuse);
        mask |= effect_probe_bit(
            effect_probe_id::normal);
    }

    if (pmetal_envspec_candidate)
        mask |= effect_probe_bit(
            effect_probe_id::pmetal_envspec);

    return mask;
}

effect_probe_mask authority_effect_mask(
    bool fixed_pointlight_bound,
    bool clustered_pointlight_bound,
    bool clustered_pointlight_spc,
    bool hemenvlerp_bound,
    bool upper_lower_bound,
    const dsrrl::operators::material_response::
        material_identity &material,
    const dsrrl::operators::material_response::
        decision &decision) noexcept
{
    (void)material;
    effect_probe_mask mask = 0u;

    if (decision.active)
        mask |= effect_probe_bit(
            effect_probe_id::material_response);

    if (hemenvlerp_bound &&
        decision.active)
        mask |= effect_probe_bit(
            effect_probe_id::material_response_lerp);

    if (upper_lower_bound ||
        g_upper_lower.direct_producer_active())
        mask |= effect_probe_bit(
            effect_probe_id::upper_lower);

    if (fixed_pointlight_bound &&
        decision.active &&
        decision.ptde_specular_power_verified) {
        mask |= effect_probe_bit(
            effect_probe_id::pointlight);
        mask |= effect_probe_bit(
            effect_probe_id::local_specular);
    }

    if (clustered_pointlight_bound &&
        decision.active) {
        mask |= effect_probe_bit(
            effect_probe_id::pointlight);
        if (clustered_pointlight_spc &&
            decision.ptde_specular_power_verified)
            mask |= effect_probe_bit(
                effect_probe_id::local_specular);
    }

    return mask;
}

void account_effect_dispatch(
    effect_probe_mask mask,
    dsrrl::runtime::draw_tx_result result,
    std::uint32_t receiver_id,
    std::uint32_t route_index) noexcept
{
    if (!g_effect_telemetry_enabled ||
        mask == 0u)
        return;

    if (dsrrl::runtime::draw_tx_issued(
            result))
        mark_effect_probe_mask(
            mask,
            effect_probe_stage::applied,
            receiver_id,
            route_index);

    if (result ==
        dsrrl::runtime::draw_tx_result::
            issued_restore_failed)
        mark_effect_probe_mask(
            mask,
            effect_probe_stage::restore_failed,
            receiver_id,
            route_index);

    if (result ==
        dsrrl::runtime::draw_tx_result::
            not_issued)
        mark_effect_probe_mask(
            mask,
            effect_probe_stage::fail_open,
            receiver_id,
            route_index);
}

void release_prepared_island_batch(
    prepared_island_batch &prepared) noexcept
{
    if (prepared.clustered_in_batch) {
        g_clustered_pnts.release_prepared_draw(
            prepared.clustered_carrier);
        g_clustered_pnts_pipeline.release_prepared_shader(
            prepared.clustered_shader);
    } else if (prepared.fixed_in_batch) {
        g_material_resources.release_prepared_draw(
            prepared.resources);
        g_fixed_pointlight.release_prepared_draw(
            prepared.fixed_carrier);
        g_fixed_pointlight_pipeline.release_prepared_shader(
            prepared.fixed_shader);
        if (prepared.fixed_b12 != nullptr)
            prepared.fixed_b12->Release();
    } else if (prepared.hemdir3_in_batch) {
        g_hemdir3.release_prepared_draw(
            prepared.hemdir3);
    } else if (prepared.subsurface_in_batch) {
        g_subsurface.release(
            prepared.subsurface);
    } else if (prepared.envspec_in_batch) {
        g_pmetal_envspec.release(
            prepared.envspec);
    } else {
        g_upper_lower_hemenv.release_prepared_draw(
            prepared.upper_lower);
        g_material_resources.release_prepared_draw(
            prepared.resources);
        g_mr_draw_runtime.release_prepared_draw(
            prepared.mr);
    }

    prepared = {};
}

struct draw_semantic_selection_guard {
    ~draw_semantic_selection_guard()
    {
        if (!g_any_draw_selection_transport_active.load(
                std::memory_order_relaxed))
            return;

        if (g_upper_lower_selection_transport_active.load(
                std::memory_order_relaxed))
            g_upper_lower.consume_draw_selection();
        if (g_fixed_pointlight_selection_transport_active.load(
                std::memory_order_relaxed))
            g_fixed_pointlight.consume_draw_selection();
        if (g_clustered_pointlight_selection_transport_active.load(
                std::memory_order_relaxed))
            g_clustered_pnts.consume_draw_selection();
        if (g_hemdir3_selection_transport_active.load(
                std::memory_order_relaxed))
            dsrrl::runtime::hemdir3_mode_transport::
                consume_draw_selection();
    }
};

void observe_equipment_specrgb_carrier(
    reshade::api::command_list *cmd_list,
    const dsrrl::operators::material_response::material_identity &material,
    const dsrrl::operators::material_response::decision &decision) noexcept
{
    if (!g_effect_telemetry_enabled ||
        cmd_list == nullptr ||
        !material.valid ||
        !material.owner_tuple_exact ||
        !material.material_slot_valid)
        return;

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
    if (context == nullptr)
        return;

    static std::atomic_bool ready_logged{false};
    static std::atomic_bool miss_logged{false};

    const auto probe =
        g_material_resources.
            probe_exact_specular_companion(
                context);
    const bool ready =
        probe.ready();

    if (ready) {
        if (!ready_logged.exchange(
                true,
                std::memory_order_relaxed)) {
            char line[512]{};
            std::snprintf(
                line,
                sizeof(line),
                "[DSRRL SPECRGB CARRIER] stage=equipment_companion_ready owner=%016llx slot=%u route=%u stock=%u snapshot=%u hash=%016llx allowed=%u companion=%u",
                static_cast<unsigned long long>(
                    material.flver_identity_hash),
                static_cast<unsigned>(
                    material.material_slot),
                static_cast<unsigned>(
                    decision.route_index),
                probe.stock_bound ? 1u : 0u,
                probe.snapshot_resolved ? 1u : 0u,
                static_cast<unsigned long long>(
                    probe.logical_hash),
                probe.logical_hash_allowed ? 1u : 0u,
                probe.companion_ready ? 1u : 0u);
            reshade::log::message(
                reshade::log::level::info,
                line);
        }
    } else if (!miss_logged.exchange(
                   true,
                   std::memory_order_relaxed)) {
        char line[512]{};
        std::snprintf(
            line,
            sizeof(line),
            "[DSRRL SPECRGB CARRIER] stage=equipment_companion_not_ready owner=%016llx slot=%u route=%u stock=%u snapshot=%u hash=%016llx allowed=%u companion=%u quarantine=%u",
            static_cast<unsigned long long>(
                material.flver_identity_hash),
            static_cast<unsigned>(
                material.material_slot),
            static_cast<unsigned>(
                decision.route_index),
            probe.stock_bound ? 1u : 0u,
            probe.snapshot_resolved ? 1u : 0u,
            static_cast<unsigned long long>(
                probe.logical_hash),
            probe.logical_hash_allowed ? 1u : 0u,
            probe.companion_ready ? 1u : 0u,
            probe.quarantined ? 1u : 0u);
        reshade::log::message(
            reshade::log::level::info,
            line);
    }
}

bool prepare_island_batch(
    reshade::api::command_list *cmd_list,
    bool fixed_pointlight_bound,
    bool clustered_pointlight_bound,
    std::uint32_t receiver_id,
    bool hemenvlerp_bound,
    bool subsurface_bound,
    bool hemdir3_bound,
    const dsrrl::runtime::hemdir3_receiver_identity &hemdir3_identity,
    bool upper_lower_bound,
    const dsrrl::runtime::upper_lower_receiver_identity &upper_lower_identity,
    const dsrrl::operators::material_response::material_identity &material,
    const dsrrl::operators::material_response::decision &decision,
    prepared_island_batch &prepared) noexcept
{
    prepared = {};

    // Passive diagnostic only: prove the equipment SpecRGB producer/cache
    // independently from any downstream consumer. This performs no bind and
    // never changes stock t1/t10 state.
    observe_equipment_specrgb_carrier(
        cmd_list,
        material,
        decision);

    // Clustered PntS owns PTDE first-four membership, source geometry/raw-q,
    // PointLight material constants and (for Spc) legacy local specular.
    // The replacement PS is attested independently from the post-A1 host.
    // Equipment Diffuse/Normal/SpecRGB replacement is not a PointLight
    // prerequisite; b12/t18/t19 are bound atomically and restored by the
    // shared transaction.
    if (clustered_pointlight_bound)
        hot_count(g_clustered_draw_candidates);

    if (clustered_pointlight_bound &&
        g_clustered_pnts_pipeline.prepare_bound_shader(
            cmd_list,
            prepared.clustered_shader)) {
        hot_count(g_clustered_draw_pipeline_ready);

        // Clustered Spc may replay only when the direct-PTDE journal has
        // passed its historical byte-exact replacement SHA and the generated
        // shader has then been migrated to the current Runtime-v2 b12 ABI.
        // Any future journal/carrier drift remains fail-open.
        if (prepared.clustered_shader.spc &&
            (!prepared.clustered_shader.current_b12_abi ||
             !prepared.clustered_shader.legacy_specular_complete)) {
            static std::atomic_bool
                clustered_spc_incomplete_logged{false};
            if (!clustered_spc_incomplete_logged.exchange(
                    true,
                    std::memory_order_relaxed)) {
                reshade::log::message(
                    reshade::log::level::warning,
                    "[DSRRL POINTLIGHT APPLY] stage=clustered_spc_failopen_legacy_specular_attestation");
            }

            g_clustered_pnts_pipeline.release_prepared_shader(
                prepared.clustered_shader);
            prepared.batch = {};
            hot_count(g_clustered_draw_fail_open);
            return false;
        }

        if (prepared.clustered_shader.spc) {
            static std::atomic_bool
                clustered_spc_complete_logged{false};
            if (!clustered_spc_complete_logged.exchange(
                    true,
                    std::memory_order_relaxed)) {
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL POINTLIGHT APPLY] stage=clustered_spc_legacy_specular_current_b12_ready");
            }
        }

        if (!g_pointlight_once_shader_ready.exchange(true)) {
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL POINTLIGHT APPLY] stage=shader_ready");
        }
        auto *context =
            reinterpret_cast<ID3D11DeviceContext *>(
                cmd_list->get_native());

        const bool direct_material_ready =
            decision.active &&
            (!prepared.clustered_shader.spc ||
             (decision.ptde_specular_power_verified &&
              decision.ptde_specular_power > 0.0f));
        if (direct_material_ready)
            hot_count(g_clustered_draw_material_ready);

        // Clustered PointLight material constants and source carriers are
        // independent of the equipment PTDE texture bridge. The exact
        // replacement journals add only the PointLight-local t19 carrier;
        // stock material texture reads remain on the host path unless a
        // separate equipment draw route authorizes replacement.
        // The native command list may wrap either an immediate or a deferred
        // D3D11 context; prepare_sidecar owns the API-valid context handling.
        const bool operator_gate_ready =
            context != nullptr &&
            direct_material_ready;

        if (operator_gate_ready) {
            hot_count(g_clustered_draw_operator_gate_ready);
        } else {
            log_pointlight_prep_once(
                1u << 0,
                "clustered_operator_gate_not_ready",
                prepared.clustered_shader.spc,
                prepared.clustered_shader.blended_material,
                direct_material_ready,
                false,
                false);
        }

        const bool sidecar_ready =
            operator_gate_ready &&
            g_clustered_pnts.prepare_sidecar(
                context,
                decision,
                prepared.clustered_carrier);

        const bool neutral_no_pointlights =
            operator_gate_ready &&
            !sidecar_ready &&
            prepared.clustered_carrier.neutral_no_pointlights;

        if (neutral_no_pointlights) {
            prepared.clustered_neutral_noop = true;
            hot_count(g_clustered_draw_neutral_noop);
            log_pointlight_prep_once(
                1u << 3,
                "clustered_no_pointlights_neutral",
                prepared.clustered_shader.spc,
                prepared.clustered_shader.blended_material,
                direct_material_ready,
                true,
                false);
        } else if (operator_gate_ready && !sidecar_ready) {
            log_pointlight_prep_once(
                1u << 1,
                "clustered_sidecar_not_ready",
                prepared.clustered_shader.spc,
                prepared.clustered_shader.blended_material,
                direct_material_ready,
                true,
                false,
                prepared.clustered_carrier.failure,
                prepared.clustered_carrier.sidecar_result_code);
        }

        if (sidecar_ready) {
            hot_count(g_clustered_draw_sidecar_ready);
            if (!g_pointlight_once_sidecar_ready.exchange(true)) {
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL POINTLIGHT APPLY] stage=sidecar_ready");
            }
            const auto point =
                dsrrl::core::operator_bit(
                    dsrrl::core::operator_id::point_light);
            const auto mr =
                dsrrl::core::operator_bit(
                    dsrrl::core::operator_id::material_response);
            const auto local =
                dsrrl::core::operator_bit(
                    dsrrl::core::operator_id::
                        local_specular_legacy);
            const auto local_if_spc =
                prepared.clustered_shader.spc
                    ? local
                    : dsrrl::core::operator_mask{0u};
            const auto static_shader_owners =
                prepared.clustered_shader.
                    composed_shader_owners;
            const auto expected_static_shader_owners =
                dsrrl::operators::point_light::
                    clustered_pnts_required_composed_shader_owners(
                        prepared.clustered_shader.spc);

            if (static_shader_owners !=
                    expected_static_shader_owners ||
                static_shader_owners == 0u) {
                g_clustered_pnts.release_prepared_draw(
                    prepared.clustered_carrier);
                g_clustered_pnts_pipeline.release_prepared_shader(
                    prepared.clustered_shader);
                prepared.batch = {};
                hot_count(g_clustered_draw_fail_open);
                return false;
            }

            dsrrl::runtime::island_draw_adapter_request clustered{};
            clustered.primary =
                prepared.clustered_shader.spc
                    ? dsrrl::core::operator_id::
                        local_specular_legacy
                    : dsrrl::core::operator_id::point_light;

            // Never duplicate the primary island in additional_owners:
            // build_island_draw_mutation rejects that shape by contract.
            // Spc is owned primarily by local_specular_legacy and composes
            // PointLight + MR. NoSpc is owned primarily by PointLight and
            // composes MR. In both classes the replacement shader also
            // contains exact create-time operator islands recovered from the
            // journal (diffuse domain, clustered attenuation, terminal SAT,
            // plus NoSpc EnvSpec deletion). They are shader-only owners here:
            // draw-local b12/t18/t19 mutations retain their narrower
            // PointLight/MR/local-specular ownership. Equipment texture
            // replacement is a separate authority and is not inherited here.
            const auto dynamic_additional_owners =
                prepared.clustered_shader.spc
                    ? (point | mr)
                    : mr;

            clustered.additional_owners =
                dynamic_additional_owners |
                static_shader_owners;
            clustered.additional_shader_owners =
                clustered.additional_owners;
            clustered.additional_constant_buffer_owners =
                dynamic_additional_owners;
            clustered.additional_resource_owners =
                prepared.clustered_shader.spc
                    ? point
                    : dsrrl::core::operator_mask{0u};
            clustered.receiver_verified = true;
            clustered.material_verified = true;
            clustered.pixel_shader =
                prepared.clustered_shader.shader;
            clustered.replace_pixel_shader = true;
            clustered.constant_buffers[0] = {
                12u,
                prepared.clustered_carrier.b12,
                point | mr | local_if_spc
            };
            clustered.constant_buffer_count = 1u;
            clustered.srvs[0] = {
                18u,
                prepared.clustered_carrier.t18
            };
            clustered.srvs[1] = {
                19u,
                prepared.clustered_carrier.t19
            };
            clustered.srv_count = 2u;

            if (dsrrl::runtime::append_island_draw_request(
                    prepared.batch,
                    clustered) ==
                dsrrl::runtime::island_draw_batch_result::ready) {
                prepared.clustered_in_batch = true;
                hot_count(g_clustered_draw_batch_ready);
                if (!g_pointlight_once_batch_ready.exchange(true)) {
                    reshade::log::message(
                        reshade::log::level::info,
                        "[DSRRL POINTLIGHT APPLY] stage=batch_ready");
                }
                return true;
            }
        }

        g_clustered_pnts.release_prepared_draw(
            prepared.clustered_carrier);
        g_clustered_pnts_pipeline.release_prepared_shader(
            prepared.clustered_shader);
        prepared.batch = {};
    } else if (clustered_pointlight_bound) {
        log_pointlight_prep_once(
            1u << 2,
            "clustered_pipeline_not_ready",
            false,
            false,
            decision.active,
            false,
            false);
    }

    if (clustered_pointlight_bound &&
        !prepared.clustered_neutral_noop)
        hot_count(g_clustered_draw_fail_open);

    // Fixed PntSS/PntSSSS is a separate exact receiver namespace. Compose
    // the entire visible island atomically: replacement PS, authored b12
    // material state, fresh raw-q t19 and PTDE SpecRGB t10 (plus t16 for
    // audited Mul/blended bodies).
    if (fixed_pointlight_bound &&
        g_fixed_pointlight_pipeline.prepare_bound_shader(
            cmd_list,
            prepared.fixed_shader)) {
        hot_count(g_fixed_draw_candidates);
        auto *context =
            reinterpret_cast<ID3D11DeviceContext *>(
                cmd_list->get_native());

        const bool direct_material_ready =
            decision.active &&
            decision.ptde_specular_power_verified;
        if (direct_material_ready)
            hot_count(g_fixed_draw_material_ready);

        dsrrl::operators::material_response::
            mtd_semantic_query fixed_query{};
        fixed_query.material = material;
        fixed_query.receiver_id = 0u;
        fixed_query.ownership.flver_sha256 =
            material.flver_sha256;
        fixed_query.ownership.flver_identity_hash =
            material.flver_identity_hash;
        fixed_query.ownership.material_slot =
            material.material_slot;
        fixed_query.ownership.material_slot_valid =
            material.material_slot_valid;
        fixed_query.ownership.exact =
            material.owner_tuple_exact;

        const bool resources_ready =
            context != nullptr &&
            direct_material_ready &&
            g_material_resources.
                prepare_fixed_pointlight_material_requests(
                    context,
                    fixed_query,
                    true,
                    true,
                    prepared.fixed_shader.blended_material,
                    prepared.resources) &&
            prepared.resources.spec_rgb &&
            prepared.resources.diffuse &&
            prepared.resources.normal;

        if (resources_ready)
            hot_count(g_fixed_draw_spec_ready);

        const bool b12_ready =
            resources_ready &&
            g_mr_draw_runtime.prepare_b12_carrier(
                decision,
                prepared.fixed_b12);

        if (b12_ready)
            hot_count(g_fixed_draw_b12_ready);

        const bool t19_ready =
            b12_ready &&
            g_fixed_pointlight.prepare_t19(
                context,
                prepared.fixed_shader.light_count,
                prepared.fixed_carrier);

        if (t19_ready) {
            hot_count(g_fixed_draw_t19_ready);
            dsrrl::runtime::island_draw_adapter_request fixed{};
            const auto local =
                dsrrl::core::operator_bit(
                    dsrrl::core::operator_id::
                        local_specular_legacy);
            const auto point =
                dsrrl::core::operator_bit(
                    dsrrl::core::operator_id::
                        point_light);
            const auto mr =
                dsrrl::core::operator_bit(
                    dsrrl::core::operator_id::
                        material_response);
            fixed.primary =
                dsrrl::core::operator_id::
                    local_specular_legacy;
            fixed.additional_owners =
                point | mr;
            fixed.additional_shader_owners =
                point | mr;
            fixed.additional_constant_buffer_owners =
                point | mr;
            fixed.additional_resource_owners =
                point;
            fixed.receiver_verified = true;
            fixed.material_verified = true;
            fixed.pixel_shader =
                prepared.fixed_shader.shader;
            fixed.replace_pixel_shader = true;
            fixed.constant_buffers[0] = {
                12u,
                prepared.fixed_b12,
                local | point | mr
            };
            fixed.constant_buffer_count = 1u;
            fixed.srvs[0] = {
                19u,
                prepared.fixed_carrier.t19
            };
            fixed.srv_count = 1u;

            if (dsrrl::runtime::append_island_draw_request(
                    prepared.batch,
                    fixed) ==
                dsrrl::runtime::island_draw_batch_result::ready) {
                bool resources_appended = true;
                for (std::uint32_t i = 0u;
                     i < prepared.resources.request_count;
                     ++i) {
                    if (dsrrl::runtime::append_island_draw_request(
                            prepared.batch,
                            prepared.resources.requests[i]) !=
                        dsrrl::runtime::island_draw_batch_result::ready) {
                        resources_appended = false;
                        break;
                    }
                }

                if (resources_appended) {
                    prepared.fixed_in_batch = true;
                    hot_count(g_fixed_draw_batch_ready);
                    return true;
                }
            }
        }

        g_material_resources.release_prepared_draw(
            prepared.resources);
        g_fixed_pointlight.release_prepared_draw(
            prepared.fixed_carrier);
        g_fixed_pointlight_pipeline.release_prepared_shader(
            prepared.fixed_shader);
        if (prepared.fixed_b12 != nullptr) {
            prepared.fixed_b12->Release();
            prepared.fixed_b12 = nullptr;
        }
        prepared.batch = {};
        hot_count(g_fixed_draw_fail_open);
    }

    if (hemdir3_bound) {
        if (!g_hemdir3.prepare_draw_request(
                cmd_list,
                hemdir3_identity,
                material,
                prepared.hemdir3))
            return false;

        prepared.hemdir3_in_batch = true;

        if (dsrrl::runtime::append_island_draw_request(
                prepared.batch,
                prepared.hemdir3.request) !=
            dsrrl::runtime::island_draw_batch_result::ready) {
            release_prepared_island_batch(prepared);
            return false;
        }

        return true;
    }

    if (subsurface_bound) {
        if (!g_subsurface.prepare(
                cmd_list,
                material,
                prepared.subsurface))
            return false;

        prepared.subsurface_in_batch = true;
        prepared.mr_in_batch = true;

        if (dsrrl::runtime::append_island_draw_request(
                prepared.batch,
                prepared.subsurface.subsurface) !=
            dsrrl::runtime::island_draw_batch_result::ready) {
            release_prepared_island_batch(prepared);
            return false;
        }

        if (dsrrl::runtime::append_island_draw_request(
                prepared.batch,
                prepared.subsurface.mr.request) !=
            dsrrl::runtime::island_draw_batch_result::ready) {
            release_prepared_island_batch(prepared);
            return false;
        }

        for (std::uint32_t i = 0u;
             i < prepared.subsurface.resources.request_count;
             ++i) {
            if (dsrrl::runtime::append_island_draw_request(
                    prepared.batch,
                    prepared.subsurface.resources.requests[i]) !=
                dsrrl::runtime::island_draw_batch_result::ready) {
                release_prepared_island_batch(prepared);
                return false;
            }
        }

        return true;
    }

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());

    const bool diffuse_mr_active =
        decision.active &&
        (decision.certified_operations &
         dsrrl::operators::material_response::
             diffuse_material_domain_linear) != 0u;

    const bool direct_ul_draw_applied =
        upper_lower_bound &&
        g_upper_lower.
            direct_producer_ready_for_draw();
    const bool direct_ul_producer =
        upper_lower_bound &&
        dsrrl::runtime::
            upper_lower_direct_producer_bypass_allowed(
                direct_ul_draw_applied,
                upper_lower_identity);

    // P_Metal EnvSpec owns the complete PS+b12+t10+t12/t14+s12/s14
    // semantic island. Exact P_Metal must be atomic at draw time: once the
    // exact material/owner/route/receiver gate matches, a failure anywhere in
    // the dedicated island fails open to the original stock DSR draw. Never
    // continue into generic diffuse-only MR, because that creates a partial
    // P_Metal hybrid (PTDE c100/diffuse with stock DSR EnvSpec).
    const auto envspec_family =
        hemenvlerp_bound
            ? dsrrl::runtime::pmetal_envspec_receiver_family::hemenvlerp
            : dsrrl::runtime::pmetal_envspec_receiver_family::stable_hemenv;

    const bool exact_pmetal_envspec =
        dsrrl::runtime::exact_pmetal_envspec_candidate(
            material,
            decision);

    if (exact_pmetal_envspec) {
        if (!g_pmetal_envspec.prepare(
                cmd_list,
                material,
                decision,
                envspec_family,
                prepared.envspec))
            return false;

        prepared.envspec_in_batch = true;

        if (dsrrl::runtime::append_island_draw_request(
                prepared.batch,
                prepared.envspec.request) !=
            dsrrl::runtime::island_draw_batch_result::ready) {
            release_prepared_island_batch(prepared);
            return false;
        }

        for (std::uint32_t i = 0u;
             i < prepared.envspec.material_resources.request_count;
             ++i) {
            if (dsrrl::runtime::append_island_draw_request(
                    prepared.batch,
                    prepared.envspec.material_resources.requests[i]) !=
                dsrrl::runtime::island_draw_batch_result::ready) {
                release_prepared_island_batch(prepared);
                return false;
            }
        }

        return true;
    }

    // HemEnvLerp is a distinct executable consumer and must use its own
    // replacement shader namespace. Build the draw from the base t1 consumer
    // first. Only after the exact draw-local SpecRGB resource request exists
    // may the paired t10-consuming shader replace it.
    if (hemenvlerp_bound) {
        if (!g_lerp_once_receiver_hit.exchange(true)) {
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL LERP MR DIFFUSE ACT] stage=receiver_hit");
        }

        const bool lerp_ul_exact =
            upper_lower_bound &&
            upper_lower_identity.family ==
                dsrrl::operators::lightbank::
                    upper_lower_hemenv_family::hemenvlerp &&
            upper_lower_identity.stratum ==
                dsrrl::operators::lightbank::
                    upper_lower_hemenv_stratum::spc &&
            upper_lower_identity.stable_receiver_id ==
                decision.receiver_id;

        bool lerp_mr_prepared = false;

        if (diffuse_mr_active &&
            lerp_ul_exact &&
            context != nullptr &&
            g_upper_lower.prepare_upper_lower_carrier(
                context,
                prepared.upper_lower.carrier) &&
            g_mr_draw_runtime.
                prepare_lerp_draw_request_with_upper_lower(
                    decision,
                    prepared.upper_lower.carrier.b13,
                    prepared.mr)) {
            prepared.upper_lower_combined = true;
            lerp_mr_prepared = true;
            if (!g_lerp_once_mr_ul_ready.exchange(true)) {
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL LERP MR DIFFUSE ACT] stage=mr_ul_ready");
            }
        } else if (
            diffuse_mr_active &&
            g_mr_draw_runtime.prepare_lerp_draw_request(
                decision,
                prepared.mr)) {
            // Material Response is independently valid for Lerp. Failure of
            // the U/L composed operator must not demote MR to stock DSR.
            prepared.upper_lower_combined = false;
            lerp_mr_prepared = true;
            if (!g_lerp_once_mr_only_ready.exchange(true)) {
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL LERP MR DIFFUSE ACT] stage=mr_only_ready");
            }
        }

        if (lerp_mr_prepared) {
            prepared.mr_in_batch = true;
            prepared.lerp_mr_in_batch = true;

            dsrrl::operators::material_response::
                mtd_semantic_query lerp_query{};
            lerp_query.material = material;
            lerp_query.receiver_id = receiver_id;
            lerp_query.ownership.flver_sha256 =
                material.flver_sha256;
            lerp_query.ownership.flver_identity_hash =
                material.flver_identity_hash;
            lerp_query.ownership.material_slot =
                material.material_slot;
            lerp_query.ownership.material_slot_valid =
                material.material_slot_valid;
            lerp_query.ownership.exact =
                material.owner_tuple_exact;

            // MR reset invariant: generic diffuse MR must never advertise
            // a SpecRGB consumer. PTDE SpecRGB is owned by explicit EnvSpec/
            // local-specular islands, not by the surviving DSR PBL tail.
            (void)g_material_resources.prepare_draw_requests(
                context,
                receiver_id,
                lerp_query,
                true,
                false,
                prepared.resources);

            bool lerp_resource_fallback = false;
            if (prepared.resources.spec_rgb) {
                // Defensive anti-hybrid fail-open: a future resource-router
                // regression must not silently resurrect generic MR+SpecRGB.
                if (!g_material_resources.drop_spec_rgb_request(
                        prepared.resources))
                    g_material_resources.release_prepared_draw(
                        prepared.resources);
                lerp_resource_fallback = true;
            }

            if (dsrrl::runtime::append_island_draw_request(
                    prepared.batch,
                    prepared.mr.request) !=
                dsrrl::runtime::island_draw_batch_result::ready) {
                release_prepared_island_batch(prepared);
                return false;
            }

            for (std::uint32_t i = 0u;
                 i < prepared.resources.request_count;
                 ++i) {
                if (dsrrl::runtime::append_island_draw_request(
                        prepared.batch,
                        prepared.resources.requests[i]) !=
                    dsrrl::runtime::island_draw_batch_result::ready) {
                    release_prepared_island_batch(prepared);
                    return false;
                }
            }

            if (lerp_resource_fallback)
                hot_count(g_lerp_full_draw_fallback);
            else
                hot_count(g_lerp_full_draw_ready);

            if (!g_lerp_once_batch_ready.exchange(true)) {
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL LERP MR DIFFUSE ACT] stage=batch_ready");
            }
            return true;
        }

        hot_count(g_lerp_full_draw_fallback);

        // Conservative fallback for exact Lerp receivers that cannot prove
        // the full material/resource composition. Keep the already-certified
        // U/L-only route rather than borrowing a stable-HemEnv shader.
        g_upper_lower_hemenv.release_prepared_draw(
            prepared.upper_lower);

        if (upper_lower_bound &&
            upper_lower_identity.family ==
                dsrrl::operators::lightbank::
                    upper_lower_hemenv_family::hemenvlerp &&
            g_upper_lower_hemenv.prepare_draw_request(
                cmd_list,
                upper_lower_identity,
                false,
                prepared.upper_lower)) {
            if (dsrrl::runtime::append_island_draw_request(
                    prepared.batch,
                    prepared.upper_lower.request) !=
                dsrrl::runtime::island_draw_batch_result::ready) {
                release_prepared_island_batch(prepared);
                return false;
            }

            return true;
        }

        return false;
    }

    const bool ul_spc =
        upper_lower_bound &&
        upper_lower_identity.stratum ==
            dsrrl::operators::lightbank::
                upper_lower_hemenv_stratum::spc;

    // Prepare the base MR shader first. It is deliberately the stock-t1
    // consumer; the paired t10 variant is selected only after resource proof.
    if (!direct_ul_producer &&
        diffuse_mr_active &&
        ul_spc &&
        context != nullptr &&
        g_upper_lower.prepare_upper_lower_carrier(
            context,
            prepared.upper_lower.carrier)) {
        if (g_mr_draw_runtime.
                prepare_draw_request_with_upper_lower(
                    decision,
                    prepared.upper_lower.carrier.b13,
                    prepared.mr)) {
            prepared.mr_in_batch = true;
            prepared.upper_lower_combined = true;
        } else {
            g_upper_lower_hemenv.release_prepared_draw(
                prepared.upper_lower);
        }
    }

    // Independent fallback: if combined MR+U/L is unavailable, keep MR
    // active by itself and leave U/L at stock DSR for this draw.
    if (!prepared.mr_in_batch &&
        diffuse_mr_active &&
        g_mr_draw_runtime.prepare_draw_request(
            decision,
            prepared.mr)) {
        prepared.mr_in_batch = true;
    }

    dsrrl::operators::material_response::mtd_semantic_query query{};
    query.material = material;
    query.receiver_id = receiver_id;
    query.ownership.flver_sha256 =
        material.flver_sha256;
    query.ownership.flver_identity_hash =
        material.flver_identity_hash;
    query.ownership.material_slot =
        material.material_slot;
    query.ownership.material_slot_valid =
        material.material_slot_valid;
    query.ownership.exact =
        material.owner_tuple_exact;

    if (context != nullptr) {
        // MR reset invariant: generic stable MR is diffuse-v1 only.
        // It must never request/promote a t10 SpecRGB consumer.
        (void)g_material_resources.prepare_draw_requests(
            context,
            receiver_id,
            query,
            prepared.mr_in_batch,
            false,
            prepared.resources);

        if (prepared.resources.spec_rgb) {
            // Defensive anti-hybrid fail-open. Keep independently valid
            // diffuse/normal resource requests when the SpecRGB request can
            // be removed cleanly; otherwise discard the whole resource batch.
            if (!g_material_resources.drop_spec_rgb_request(
                    prepared.resources))
                g_material_resources.release_prepared_draw(
                    prepared.resources);
        }
    }

    if (!direct_ul_producer &&
        upper_lower_bound &&
        !prepared.upper_lower_combined &&
        g_upper_lower_hemenv.prepare_draw_request(
            cmd_list,
            upper_lower_identity,
            prepared.mr_in_batch,
            prepared.upper_lower)) {
        // Request is appended below, after the final MR shader variant is
        // known, preserving the historical MR->U/L batch ordering.
    }

    if (prepared.mr_in_batch) {
        if (!g_mr_once_batch_ready.exchange(true)) {
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL MR DIFFUSE ACT] stage=batch_ready");
        }
        if (dsrrl::runtime::append_island_draw_request(
                prepared.batch,
                prepared.mr.request) !=
            dsrrl::runtime::island_draw_batch_result::ready) {
            release_prepared_island_batch(prepared);
            return false;
        }
    }

    if (!prepared.upper_lower_combined &&
        prepared.upper_lower.ready) {
        if (dsrrl::runtime::append_island_draw_request(
                prepared.batch,
                prepared.upper_lower.request) !=
            dsrrl::runtime::island_draw_batch_result::ready) {
            release_prepared_island_batch(prepared);
            return false;
        }
    }

    for (std::uint32_t i = 0u;
         i < prepared.resources.request_count;
         ++i) {
        if (dsrrl::runtime::append_island_draw_request(
                prepared.batch,
                prepared.resources.requests[i]) !=
            dsrrl::runtime::island_draw_batch_result::ready) {
            release_prepared_island_batch(prepared);
            return false;
        }
    }

    return prepared.batch.island_count != 0u ||
           prepared.clustered_neutral_noop;
}

void observe_bloom_fx_draw_authority() noexcept
{
    dsrrl::runtime::bloom_fx_draw_transport::fx_draw_snapshot snapshot{};
    if (!dsrrl::runtime::bloom_fx_draw_transport::snapshot(snapshot))
        return;

    hot_count(g_bloom_fx_draw_snapshots);
    mark_effect_probe(
        effect_probe_id::bloom_q8,
        effect_probe_stage::candidate);

    const auto authority =
        dsrrl::runtime::bloom_fx_draw_transport::
            validate_waterwave_draw_authority(snapshot);

    if (authority ==
        dsrrl::runtime::bloom_fx_draw_transport::
            waterwave_draw_authority_result::authorized) {
        hot_count(g_bloom_fx_draw_authorized);
        mark_effect_probe(
            effect_probe_id::bloom_q8,
            effect_probe_stage::authority);
    } else {
        hot_count(g_bloom_fx_draw_rejected);
        mark_effect_probe(
            effect_probe_id::bloom_q8,
            effect_probe_stage::fail_open);
    }
}

bool on_draw(
    reshade::api::command_list *cmd_list,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance)
{
    if (g_raw_draw_replay_recursing)
        return false;

    if (k_empty_draw_callback_bisect)
        return false;

    if (k_draw_callback_only_bisect)
        return false;

    if (k_raw_draw_replay_bisect ||
        k_raw_native_draw_reentry_min_bisect) {
        g_raw_draw_replay_recursing = true;
        const bool issued =
            g_draw_transactions.raw_replay_draw(
                cmd_list,
                vertex_count,
                instance_count,
                first_vertex,
                first_instance);
        g_raw_draw_replay_recursing = false;
        return issued;
    }

    if (g_hot_telemetry_enabled &&
        dsrrl::runtime::bloom_fx_draw_transport::
            active_draw_scope())
        observe_bloom_fx_draw_authority();

    hot_count(g_draw_events);
    draw_semantic_selection_guard semantic_guard{};

    const auto route_mask =
        active_integrated_draw_route(
            integrated_draw_route_bound(
                cmd_list));

    if (route_mask == 0u) {
        hot_count(g_draw_fast_skip);
        dsrrl::runtime::
            material_owner_selection_clear();
        return false;
    }

    const auto a1_effects =
        static_shader_owner_effect_mask(
            integrated_a1_owners_bound(
                cmd_list));
    if ((route_mask & k_route_a1) != 0u &&
        a1_effects != 0u)
        mark_effect_probe_mask(
            a1_effects,
            effect_probe_stage::applied);

    // A1 owners are fully materialized at CreatePipeline. If no island on
    // this bound pipeline needs a draw-specific carrier, the stock draw
    // already executes the replacement shader and there is nothing to join,
    // snapshot, replay or restore here.
    if ((route_mask & k_dynamic_draw_route_mask) == 0u) {
        hot_count(g_draw_fast_skip);
        dsrrl::runtime::
            material_owner_selection_clear();
        return false;
    }

    const auto route_candidates =
        route_candidate_effect_mask(
            route_mask);
    mark_effect_probe_mask(
        route_candidates,
        effect_probe_stage::candidate);

    const bool fixed_pointlight_bound =
        (route_mask &
         k_route_fixed_pointlight) != 0u;

    bool clustered_pointlight_spc = false;
    bool clustered_pointlight_blended = false;
    const bool clustered_route_present =
        (route_mask &
         k_route_clustered_pointlight) != 0u;
    const bool clustered_pointlight_bound =
        clustered_route_present &&
        g_clustered_pnts_pipeline.bound_metadata(
            cmd_list,
            clustered_pointlight_spc,
            clustered_pointlight_blended);
    (void)clustered_pointlight_blended;

    if (clustered_route_present &&
        !clustered_pointlight_bound) {
        constexpr std::uint32_t bit = 1u << 5;
        const auto observed =
            g_pointlight_gate_log_mask.load(
                std::memory_order_relaxed);
        if ((observed & bit) == 0u &&
            (g_pointlight_gate_log_mask.fetch_or(
                 bit,
                 std::memory_order_relaxed) & bit) == 0u) {
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL POINTLIGHT GATE] stage=clustered_metadata_unbound");
        }
    }

    std::uint32_t receiver_id = 0u;
    bool hemenvlerp_bound = false;
    dsrrl::runtime::hemenvlerp_receiver_identity hemenvlerp_identity{};
    bool subsurface_bound = false;
    bool hemdir3_bound = false;
    dsrrl::runtime::hemdir3_receiver_identity hemdir3_identity{};
    bool upper_lower_bound = false;
    dsrrl::runtime::upper_lower_receiver_identity upper_lower_identity{};
    dsrrl::operators::material_response::material_identity material{};
    dsrrl::operators::material_response::decision decision{};

    const bool direct_pointlight_route =
        fixed_pointlight_bound ||
        clustered_pointlight_bound;
    const bool draw_identity_ready =
        direct_pointlight_route
            ? observe_pointlight_draw_identity(
                route_mask,
                fixed_pointlight_bound,
                clustered_pointlight_bound,
                clustered_pointlight_spc,
                material,
                decision)
            : observe_draw_identity(
                cmd_list,
                route_mask,
                fixed_pointlight_bound,
                clustered_pointlight_bound,
                clustered_pointlight_spc,
                receiver_id,
                hemenvlerp_bound,
                hemenvlerp_identity,
                subsurface_bound,
                hemdir3_bound,
                hemdir3_identity,
                upper_lower_bound,
                upper_lower_identity,
                material,
                decision);

    if (!draw_identity_ready) {
        mark_effect_probe_mask(
            route_candidates,
            effect_probe_stage::fail_open);
        return false;
    }

    // Direct PointLight routing is exclusive at this point. A material
    // decision that is not active can only fail open to stock DSR, so do not
    // enter the draw-time shader/resource preparation path. In PointLight
    // heavy scenes the attested clustered PS may be bound on many host draws
    // whose material tuple is outside the exact PTDE authority; preparing and
    // AddRef/Release'ing the replacement for all of those draws created a
    // large reject-only hot path.
    if (direct_pointlight_route &&
        !decision.active)
        return false;

    const auto effect_candidates =
        candidate_effect_mask(
            route_mask,
            fixed_pointlight_bound,
            clustered_pointlight_bound,
            clustered_pointlight_spc,
            hemenvlerp_bound,
            subsurface_bound,
            hemdir3_bound,
            upper_lower_bound,
            material,
            decision);
    mark_effect_probe_mask(
        effect_candidates,
        effect_probe_stage::candidate,
        receiver_id,
        decision.route_index);
    mark_effect_probe_mask(
        authority_effect_mask(
            fixed_pointlight_bound,
            clustered_pointlight_bound,
            clustered_pointlight_spc,
            hemenvlerp_bound,
            upper_lower_bound,
            material,
            decision),
        effect_probe_stage::authority,
        receiver_id,
        decision.route_index);

    prepared_island_batch prepared{};
    if (!prepare_island_batch(
            cmd_list,
            fixed_pointlight_bound,
            clustered_pointlight_bound,
            receiver_id,
            hemenvlerp_bound,
            subsurface_bound,
            hemdir3_bound,
            hemdir3_identity,
            upper_lower_bound,
            upper_lower_identity,
            material,
            decision,
            prepared)) {
        mark_effect_probe_mask(
            effect_candidates,
            effect_probe_stage::fail_open,
            receiver_id,
            decision.route_index);
        return false;
    }

    if (prepared.clustered_neutral_noop &&
        prepared.batch.island_count == 0u) {
        release_prepared_island_batch(prepared);
        return false;
    }

    const auto effect_prepared =
        prepared_effect_mask(prepared);
    mark_effect_probe_mask(
        effect_prepared,
        effect_probe_stage::authority,
        receiver_id,
        decision.route_index);
    mark_effect_probe_mask(
        effect_prepared,
        effect_probe_stage::prepared,
        receiver_id,
        decision.route_index);

    if (k_pmetal_native_draw_runtime_enabled &&
        (prepared.envspec_in_batch ||
         prepared.clustered_in_batch) &&
        g_pmetal_native_draw.arm_draw(
            cmd_list,
            prepared.batch.mutation,
            vertex_count,
            instance_count,
            first_vertex,
            first_instance)) {
        release_prepared_island_batch(
            prepared);
        // Return false so ReShade continues into its single original
        // _orig->Draw/DrawInstanced call. The native bridge wraps that call
        // with the exact prepared P_Metal mutation and restores afterwards.
        return false;
    }

    if (k_state_capture_only_bisect) {
        (void)g_draw_transactions.capture_only(
            cmd_list,
            prepared.batch.mutation);
        release_prepared_island_batch(prepared);
        return false;
    }

    if (k_native_state_mutate_restore_only_bisect) {
        (void)g_draw_transactions.native_mutate_restore_only(
            cmd_list,
            prepared.batch.mutation);
        release_prepared_island_batch(prepared);
        return false;
    }

    if (k_core_transaction_only_bisect) {
        (void)g_draw_transactions.core_transaction_only(
            cmd_list,
            prepared.batch.mutation);
        release_prepared_island_batch(prepared);
        return false;
    }

    if (k_state_capture_only_bisect) {
        (void)g_draw_transactions.capture_only(
            cmd_list,
            prepared.batch.mutation);
        release_prepared_island_batch(prepared);
        return false;
    }

    if (k_native_state_mutate_restore_only_bisect) {
        (void)g_draw_transactions.native_mutate_restore_only(
            cmd_list,
            prepared.batch.mutation);
        release_prepared_island_batch(prepared);
        return false;
    }

    if (k_core_transaction_only_bisect) {
        (void)g_draw_transactions.core_transaction_only(
            cmd_list,
            prepared.batch.mutation);
        release_prepared_island_batch(prepared);
        return false;
    }

    if (k_state_transaction_only_bisect) {
        (void)g_draw_transactions.mutate_restore_only(
            cmd_list,
            prepared.batch.mutation);
        release_prepared_island_batch(prepared);
        return false;
    }

    if (!k_draw_replay_runtime_enabled) {
        release_prepared_island_batch(prepared);
        return false;
    }

    const bool direct_current_native =
        k_pmetal_direct_current_native_dispatch &&
        (prepared.envspec_in_batch ||
         prepared.clustered_in_batch);

    if (direct_current_native &&
        prepared.clustered_in_batch)
        prepared.batch.mutation.
            synchronous_core_transaction = true;

    if (direct_current_native)
        g_raw_draw_replay_recursing = true;

    const auto dispatch =
        dsrrl::runtime::dispatch_island_draw_batch(
            g_draw_transactions,
            cmd_list,
            prepared.batch,
            vertex_count,
            instance_count,
            first_vertex,
            first_instance);

    if (direct_current_native)
        g_raw_draw_replay_recursing = false;

    const bool mr_in_batch =
        prepared.mr_in_batch;
    const bool lerp_mr_in_batch =
        prepared.lerp_mr_in_batch;
    const bool clustered_in_batch =
        prepared.clustered_in_batch;

    release_prepared_island_batch(
        prepared);

    account_effect_dispatch(
        effect_prepared,
        dispatch.transaction,
        receiver_id,
        decision.route_index);

    if (clustered_in_batch) {
        if (dsrrl::runtime::draw_tx_issued(
                dispatch.transaction)) {
            hot_count(g_clustered_draw_applied);
            if (direct_current_native &&
                !g_pointlight_once_direct_native_applied.exchange(
                    true,
                    std::memory_order_relaxed))
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL POINTLIGHT APPLY] stage=direct_current_native_applied");
            if (!g_pointlight_once_applied.exchange(true)) {
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL POINTLIGHT APPLY] stage=applied");
                // Diagnostic only: capture RT identity on the same first-hit
                // latch. No native RT/viewport queries remain on subsequent
                // PointLight draws, including the 480x270 water reflection pass.
                observe_pointlight_render_target(
                    cmd_list);
            }
        } else {
            hot_count(g_clustered_draw_fail_open);
        }

        if (dispatch.transaction ==
            dsrrl::runtime::draw_tx_result::
                issued_restore_failed)
            hot_count(g_clustered_draw_restore_fail);
    }

    if (mr_in_batch) {
        g_mr_draw_runtime.account_dispatch_result(
            dispatch.transaction);
#if defined(DSRRL_MR_MATERIAL_TRACE)
        if (dsrrl::runtime::draw_tx_issued(dispatch.transaction))
            trace_mr_material(receiver_id, material, decision, 4u);
#endif
        if (dsrrl::runtime::draw_tx_issued(
                dispatch.transaction) &&
            !g_mr_once_draw_issued.exchange(true)) {
            char line[320]{};
            std::snprintf(
                line,
                sizeof(line),
                "[DSRRL MR DIFFUSE ACT] stage=draw_issued rx=%u route=%u c100=%.6f,%.6f,%.6f",
                static_cast<unsigned>(receiver_id),
                static_cast<unsigned>(decision.route_index),
                decision.c100[0],
                decision.c100[1],
                decision.c100[2]);
            reshade::log::message(
                reshade::log::level::info,
                line);
        }
        if (lerp_mr_in_batch &&
            dsrrl::runtime::draw_tx_issued(
                dispatch.transaction) &&
            !g_lerp_once_draw_issued.exchange(true)) {
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL LERP MR DIFFUSE ACT] stage=draw_issued");
        }
    }

    return dsrrl::runtime::draw_tx_issued(
        dispatch.transaction);
}

bool on_draw_indexed(
    reshade::api::command_list *cmd_list,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance)
{
    if (g_raw_draw_replay_recursing)
        return false;

    if (k_empty_draw_callback_bisect)
        return false;

    if (k_draw_callback_only_bisect)
        return false;

    if (k_raw_draw_replay_bisect ||
        k_raw_native_draw_reentry_min_bisect) {
        g_raw_draw_replay_recursing = true;
        const bool issued =
            g_draw_transactions.raw_replay_draw_indexed(
                cmd_list,
                index_count,
                instance_count,
                first_index,
                vertex_offset,
                first_instance);
        g_raw_draw_replay_recursing = false;
        return issued;
    }

    if (g_hot_telemetry_enabled &&
        dsrrl::runtime::bloom_fx_draw_transport::
            active_draw_scope())
        observe_bloom_fx_draw_authority();

    hot_count(g_draw_events);
    draw_semantic_selection_guard semantic_guard{};

    const auto route_mask =
        active_integrated_draw_route(
            integrated_draw_route_bound(
                cmd_list));

    if (route_mask == 0u) {
        hot_count(g_draw_fast_skip);
        dsrrl::runtime::
            material_owner_selection_clear();
        return false;
    }

    const auto a1_effects =
        static_shader_owner_effect_mask(
            integrated_a1_owners_bound(
                cmd_list));
    if ((route_mask & k_route_a1) != 0u &&
        a1_effects != 0u)
        mark_effect_probe_mask(
            a1_effects,
            effect_probe_stage::applied);

    // A1 owners are fully materialized at CreatePipeline. If no island on
    // this bound pipeline needs a draw-specific carrier, the stock draw
    // already executes the replacement shader and there is nothing to join,
    // snapshot, replay or restore here.
    if ((route_mask & k_dynamic_draw_route_mask) == 0u) {
        hot_count(g_draw_fast_skip);
        dsrrl::runtime::
            material_owner_selection_clear();
        return false;
    }

    const auto route_candidates =
        route_candidate_effect_mask(
            route_mask);
    mark_effect_probe_mask(
        route_candidates,
        effect_probe_stage::candidate);

    const bool fixed_pointlight_bound =
        (route_mask &
         k_route_fixed_pointlight) != 0u;

    bool clustered_pointlight_spc = false;
    bool clustered_pointlight_blended = false;
    const bool clustered_pointlight_bound =
        (route_mask &
         k_route_clustered_pointlight) != 0u &&
        g_clustered_pnts_pipeline.bound_metadata(
            cmd_list,
            clustered_pointlight_spc,
            clustered_pointlight_blended);
    (void)clustered_pointlight_blended;

    std::uint32_t receiver_id = 0u;
    bool hemenvlerp_bound = false;
    dsrrl::runtime::hemenvlerp_receiver_identity hemenvlerp_identity{};
    bool subsurface_bound = false;
    bool hemdir3_bound = false;
    dsrrl::runtime::hemdir3_receiver_identity hemdir3_identity{};
    bool upper_lower_bound = false;
    dsrrl::runtime::upper_lower_receiver_identity upper_lower_identity{};
    dsrrl::operators::material_response::material_identity material{};
    dsrrl::operators::material_response::decision decision{};

    const bool direct_pointlight_route =
        fixed_pointlight_bound ||
        clustered_pointlight_bound;
    const bool draw_identity_ready =
        direct_pointlight_route
            ? observe_pointlight_draw_identity(
                route_mask,
                fixed_pointlight_bound,
                clustered_pointlight_bound,
                clustered_pointlight_spc,
                material,
                decision)
            : observe_draw_identity(
                cmd_list,
                route_mask,
                fixed_pointlight_bound,
                clustered_pointlight_bound,
                clustered_pointlight_spc,
                receiver_id,
                hemenvlerp_bound,
                hemenvlerp_identity,
                subsurface_bound,
                hemdir3_bound,
                hemdir3_identity,
                upper_lower_bound,
                upper_lower_identity,
                material,
                decision);

    if (!draw_identity_ready) {
        mark_effect_probe_mask(
            route_candidates,
            effect_probe_stage::fail_open);
        return false;
    }

    // Direct PointLight routing is exclusive at this point. A material
    // decision that is not active can only fail open to stock DSR, so do not
    // enter the draw-time shader/resource preparation path. In PointLight
    // heavy scenes the attested clustered PS may be bound on many host draws
    // whose material tuple is outside the exact PTDE authority; preparing and
    // AddRef/Release'ing the replacement for all of those draws created a
    // large reject-only hot path.
    if (direct_pointlight_route &&
        !decision.active)
        return false;

    const auto effect_candidates =
        candidate_effect_mask(
            route_mask,
            fixed_pointlight_bound,
            clustered_pointlight_bound,
            clustered_pointlight_spc,
            hemenvlerp_bound,
            subsurface_bound,
            hemdir3_bound,
            upper_lower_bound,
            material,
            decision);
    mark_effect_probe_mask(
        effect_candidates,
        effect_probe_stage::candidate,
        receiver_id,
        decision.route_index);
    mark_effect_probe_mask(
        authority_effect_mask(
            fixed_pointlight_bound,
            clustered_pointlight_bound,
            clustered_pointlight_spc,
            hemenvlerp_bound,
            upper_lower_bound,
            material,
            decision),
        effect_probe_stage::authority,
        receiver_id,
        decision.route_index);

    prepared_island_batch prepared{};
    if (!prepare_island_batch(
            cmd_list,
            fixed_pointlight_bound,
            clustered_pointlight_bound,
            receiver_id,
            hemenvlerp_bound,
            subsurface_bound,
            hemdir3_bound,
            hemdir3_identity,
            upper_lower_bound,
            upper_lower_identity,
            material,
            decision,
            prepared)) {
        mark_effect_probe_mask(
            effect_candidates,
            effect_probe_stage::fail_open,
            receiver_id,
            decision.route_index);
        return false;
    }

    if (prepared.clustered_neutral_noop &&
        prepared.batch.island_count == 0u) {
        release_prepared_island_batch(prepared);
        return false;
    }

    const auto effect_prepared =
        prepared_effect_mask(prepared);
    mark_effect_probe_mask(
        effect_prepared,
        effect_probe_stage::authority,
        receiver_id,
        decision.route_index);
    mark_effect_probe_mask(
        effect_prepared,
        effect_probe_stage::prepared,
        receiver_id,
        decision.route_index);

    if (k_pmetal_native_draw_runtime_enabled &&
        (prepared.envspec_in_batch ||
         prepared.clustered_in_batch) &&
        g_pmetal_native_draw.arm_draw_indexed(
            cmd_list,
            prepared.batch.mutation,
            index_count,
            instance_count,
            first_index,
            vertex_offset,
            first_instance)) {
        release_prepared_island_batch(
            prepared);
        // Return false so ReShade executes exactly one original
        // _orig->DrawIndexed/DrawIndexedInstanced call under the native
        // P_Metal state wrapper.
        return false;
    }

    if (k_state_transaction_only_bisect) {
        (void)g_draw_transactions.mutate_restore_only(
            cmd_list,
            prepared.batch.mutation);
        release_prepared_island_batch(prepared);
        return false;
    }

    if (!k_draw_replay_runtime_enabled) {
        release_prepared_island_batch(prepared);
        return false;
    }

    const bool direct_current_native =
        k_pmetal_direct_current_native_dispatch &&
        (prepared.envspec_in_batch ||
         prepared.clustered_in_batch);

    if (direct_current_native &&
        prepared.clustered_in_batch)
        prepared.batch.mutation.
            synchronous_core_transaction = true;

    if (direct_current_native)
        g_raw_draw_replay_recursing = true;

    const auto dispatch =
        dsrrl::runtime::dispatch_island_draw_indexed_batch(
            g_draw_transactions,
            cmd_list,
            prepared.batch,
            index_count,
            instance_count,
            first_index,
            vertex_offset,
            first_instance);

    if (direct_current_native)
        g_raw_draw_replay_recursing = false;

    const bool mr_in_batch =
        prepared.mr_in_batch;
    const bool lerp_mr_in_batch =
        prepared.lerp_mr_in_batch;
    const bool clustered_in_batch =
        prepared.clustered_in_batch;

    release_prepared_island_batch(
        prepared);

    account_effect_dispatch(
        effect_prepared,
        dispatch.transaction,
        receiver_id,
        decision.route_index);

    if (clustered_in_batch) {
        if (dsrrl::runtime::draw_tx_issued(
                dispatch.transaction)) {
            hot_count(g_clustered_draw_applied);
            if (direct_current_native &&
                !g_pointlight_once_direct_native_applied.exchange(
                    true,
                    std::memory_order_relaxed))
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL POINTLIGHT APPLY] stage=direct_current_native_applied");
            if (!g_pointlight_once_applied.exchange(true)) {
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL POINTLIGHT APPLY] stage=applied");
                // Diagnostic only: capture RT identity on the same first-hit
                // latch. No native RT/viewport queries remain on subsequent
                // PointLight draws, including the 480x270 water reflection pass.
                observe_pointlight_render_target(
                    cmd_list);
            }
        } else {
            hot_count(g_clustered_draw_fail_open);
        }

        if (dispatch.transaction ==
            dsrrl::runtime::draw_tx_result::
                issued_restore_failed)
            hot_count(g_clustered_draw_restore_fail);
    }

    if (mr_in_batch) {
        g_mr_draw_runtime.account_dispatch_result(
            dispatch.transaction);
#if defined(DSRRL_MR_MATERIAL_TRACE)
        if (dsrrl::runtime::draw_tx_issued(dispatch.transaction))
            trace_mr_material(receiver_id, material, decision, 4u);
#endif
        if (dsrrl::runtime::draw_tx_issued(
                dispatch.transaction) &&
            !g_mr_once_draw_issued.exchange(true)) {
            char line[320]{};
            std::snprintf(
                line,
                sizeof(line),
                "[DSRRL MR DIFFUSE ACT] stage=draw_issued rx=%u route=%u c100=%.6f,%.6f,%.6f",
                static_cast<unsigned>(receiver_id),
                static_cast<unsigned>(decision.route_index),
                decision.c100[0],
                decision.c100[1],
                decision.c100[2]);
            reshade::log::message(
                reshade::log::level::info,
                line);
        }
        if (lerp_mr_in_batch &&
            dsrrl::runtime::draw_tx_issued(
                dispatch.transaction) &&
            !g_lerp_once_draw_issued.exchange(true)) {
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL LERP MR DIFFUSE ACT] stage=draw_issued");
        }
    }

    return dsrrl::runtime::draw_tx_issued(
        dispatch.transaction);
}

void on_present(
    reshade::api::command_queue *,
    reshade::api::swapchain *,
    const reshade::api::rect *,
    const reshade::api::rect *,
    std::uint32_t,
    const reshade::api::rect *)
{
    const auto present =
        g_present_count.fetch_add(
            1u,
            std::memory_order_relaxed) + 1u;
    g_clustered_pnts.frame_event(present);
    if (present == 1u ||
        (g_hot_telemetry_enabled &&
         (present % 300u) == 0u)) {
        log_state("LIVE");
        motion_blur_camera_fallback_disable::
            log_state("LIVE");
    }

    if (g_effect_telemetry_enabled &&
        (present == 1u ||
         (present % 300u) == 0u))
        log_effect_matrix("LIVE");
}

void on_push_descriptors(
    reshade::api::command_list *cmd_list,
    reshade::api::shader_stage stages,
    reshade::api::pipeline_layout layout,
    std::uint32_t param_index,
    const reshade::api::descriptor_table_update &update)
{
    dsrrl::runtime::pixel_srv_shadow_on_push_descriptors(
        cmd_list,
        stages,
        layout,
        param_index,
        update);
}

void register_events()
{
    reshade::register_event<reshade::addon_event::init_device>(on_init_device);
    reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
    if (k_drawtime_islands_runtime_enabled &&
        k_pmetal_native_draw_runtime_enabled) {
        reshade::register_event<reshade::addon_event::init_command_list>(on_init_command_list);
        reshade::register_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
    }
    reshade::register_event<reshade::addon_event::create_pipeline>(on_create_pipeline);
    reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
    reshade::register_event<reshade::addon_event::destroy_pipeline>(on_destroy_pipeline);
    reshade::register_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
    if (k_drawtime_islands_runtime_enabled &&
        k_pmetal_srv_shadow_runtime_enabled)
        reshade::register_event<reshade::addon_event::push_descriptors>(on_push_descriptors);
    if (k_drawtime_islands_runtime_enabled &&
        k_draw_callbacks_runtime_enabled) {
        reshade::register_event<reshade::addon_event::draw>(on_draw);
        reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
        reshade::register_event<reshade::addon_event::present>(on_present);
    }
}

void unregister_events()
{
    if (k_drawtime_islands_runtime_enabled &&
        k_draw_callbacks_runtime_enabled) {
        reshade::unregister_event<reshade::addon_event::present>(on_present);
        reshade::unregister_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
        reshade::unregister_event<reshade::addon_event::draw>(on_draw);
    }
    if (k_drawtime_islands_runtime_enabled &&
        k_pmetal_srv_shadow_runtime_enabled)
        reshade::unregister_event<reshade::addon_event::push_descriptors>(on_push_descriptors);
    reshade::unregister_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
    reshade::unregister_event<reshade::addon_event::destroy_pipeline>(on_destroy_pipeline);
    reshade::unregister_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
    reshade::unregister_event<reshade::addon_event::create_pipeline>(on_create_pipeline);
    if (k_drawtime_islands_runtime_enabled &&
        k_pmetal_native_draw_runtime_enabled) {
        reshade::unregister_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
        reshade::unregister_event<reshade::addon_event::init_command_list>(on_init_command_list);
    }
    reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
    reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
}

} // namespace

extern "C" __declspec(dllexport)
const char *NAME =
    DSRRL_CORE_ISLANDS_PRODUCT_LINE " " DSRRL_CORE_ISLANDS_VERSION;

extern "C" __declspec(dllexport)
const char *AUTHOR =
    "DSR Restored Lighting";

extern "C" __declspec(dllexport)
const char *DESCRIPTION =
    DSRRL_CORE_ISLANDS_PRODUCT_LINE " " DSRRL_CORE_ISLANDS_VERSION
    " active main development line; native Renderer Core operator islands.";

extern "C" __declspec(dllexport)
bool AddonInit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    if (!reshade::register_addon(addon_module, reshade_module))
        return false;

    if (k_addon_loaded_only_bisect) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL CALLBACK CUT] ADDON_LOADED_ONLY: addon registered; no DSRRL events, pipeline callbacks, draw callbacks or runtime transports installed.");
        char build_identity[768]{};
        std::snprintf(
            build_identity,
            sizeof(build_identity),
            "[DSRRL BUILD_ID] version=%s source_commit=%s flavor=%s",
            DSRRL_CORE_ISLANDS_VERSION,
            DSRRL_SOURCE_COMMIT,
            DSRRL_BUILD_FLAVOR);
        reshade::log::message(reshade::log::level::info, build_identity);
        return true;
    }

    if (k_draw_callback_only_bisect) {
        reshade::register_event<reshade::addon_event::draw>(on_draw);
        reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL CALLBACK CUT] DRAW_CALLBACK_ONLY: only empty draw/draw_indexed callbacks registered; no DSRRL pipeline events or runtime transports installed.");
        char build_identity[768]{};
        std::snprintf(
            build_identity,
            sizeof(build_identity),
            "[DSRRL BUILD_ID] version=%s source_commit=%s flavor=%s",
            DSRRL_CORE_ISLANDS_VERSION,
            DSRRL_SOURCE_COMMIT,
            DSRRL_BUILD_FLAVOR);
        reshade::log::message(reshade::log::level::info, build_identity);
        return true;
    }

    // Render-hot telemetry is opt-in. Production/default execution avoids
    // synchronized counter RMWs on every draw; set DSRRL_RUNTIME_TELEMETRY=1
    // for receiver/island census sessions.
    g_hot_telemetry_enabled =
        runtime_hot_telemetry_requested();
    g_effect_telemetry_enabled =
        runtime_effect_telemetry_requested();
    reset_effect_probe();
    motion_blur_camera_fallback_disable::
        reset();

    g_a1_bridge.reset();
    g_draw_transactions.reset();
    g_mr_draw_runtime.reset();
    g_material_resources.reset();
    g_envspec_resources.reset_stats();
    dsrrl::runtime::pixel_srv_shadow_reset();
    g_bloom_scene_sidecar.reset();
    dsrrl::runtime::bloom_fx_draw_transport::reset_stats();
    g_pmetal_envspec.reset();
    g_pmetal_source.reset();
    g_pmetal_native_draw.reset_telemetry();
    g_upper_lower.reset();
    g_upper_lower_hemenv.reset();
    g_hemdir3.reset();
    g_fixed_pointlight.reset();
    g_fixed_pointlight_pipeline.reset();
    g_clustered_pnts.reset();
    g_clustered_pnts_pipeline.reset();
    dsrrl::runtime::hemdir3_mode_transport::reset_stats();
    g_present_count.store(0);
    g_mr_draw_eval.store(0);
    g_mr_would_activate.store(0);
    g_mr_fail_open.store(0);
    g_mr_payload_materialize_ok.store(0);
    g_mr_payload_materialize_fail.store(0);
    g_mr_once_receiver_hit.store(false);
    g_mr_once_owner_join.store(false);
    g_mr_once_decision_active.store(false);
    g_mr_once_identity.store(false);
    g_mr_once_batch_ready.store(false);
    g_mr_once_draw_issued.store(false);
    g_pointlight_gate_log_mask.store(0u);
    g_pointlight_prep_log_mask.store(0u);
    g_pointlight_active_logged.store(false);
    g_pointlight_once_shader_ready.store(false);
    g_pointlight_once_sidecar_ready.store(false);
    g_pointlight_once_batch_ready.store(false);
    g_pointlight_once_applied.store(false);
    for (auto &slot : g_pointlight_rt_signatures)
        slot.store(0u, std::memory_order_relaxed);
    g_upper_lower_selection_transport_active.store(false);
    g_hemdir3_selection_transport_active.store(false);
    g_fixed_pointlight_selection_transport_active.store(false);
    g_clustered_pointlight_selection_transport_active.store(false);
    g_any_draw_selection_transport_active.store(false);
    g_mr_ul_payload_materialize_ok.store(0);
    g_mr_ul_payload_materialize_fail.store(0);
    g_subsurface_spec_payload_materialize_ok.store(0);
    g_subsurface_spec_payload_materialize_fail.store(0);
    g_lerp_full_draw_ready.store(0);
    g_lerp_full_draw_fallback.store(0);
    g_lerp_once_receiver_hit.store(false);
    g_lerp_once_mr_ul_ready.store(false);
    g_lerp_once_mr_only_ready.store(false);
    g_lerp_once_batch_ready.store(false);
    g_lerp_once_draw_issued.store(false);
    g_envspec_payload_materialize_ok.store(0);
    g_envspec_payload_materialize_fail.store(0);
    g_draw_events.store(0);
    g_draw_receiver_hits.store(0);
    g_draw_owner_hits.store(0);
    g_draw_joins.store(0);
    g_draw_owner_only.store(0);
    g_draw_receiver_only.store(0);
    g_draw_fast_skip.store(0);
    g_local_specular_receiver_hits.store(0);
    g_local_specular_clustered_hits.store(0);
    g_local_specular_fixed2_hits.store(0);
    g_local_specular_fixed4_hits.store(0);
    g_local_specular_window_pass.store(0);
    g_local_specular_window_fail.store(0);
    g_local_specular_windows_total.store(0);
    g_local_specular_fixed_plan_ready.store(0);
    g_local_specular_clustered_deferred.store(0);
    g_local_specular_fixed_plan_fail.store(0);
    g_local_specular_operand_ready.store(0);
    g_local_specular_operand_fail.store(0);
    g_local_specular_output_cut_ready.store(0);
    g_local_specular_output_cut_fail.store(0);
    g_local_specular_island_plan_ready.store(0);
    g_local_specular_island_plan_fail.store(0);
    g_local_specular_single_materialize_ok.store(0);
    g_local_specular_blended_defer.store(0);
    g_local_specular_materialize_fail.store(0);
    g_fixed_draw_candidates.store(0);
    g_fixed_draw_material_ready.store(0);
    g_fixed_draw_spec_ready.store(0);
    g_fixed_draw_b12_ready.store(0);
    g_fixed_draw_t19_ready.store(0);
    g_fixed_draw_batch_ready.store(0);
    g_fixed_draw_fail_open.store(0);
    g_clustered_draw_candidates.store(0);
    g_clustered_draw_pipeline_ready.store(0);
    g_clustered_draw_material_ready.store(0);
    g_clustered_draw_operator_gate_ready.store(0);
    g_clustered_draw_sidecar_ready.store(0);
    g_clustered_draw_batch_ready.store(0);
    g_clustered_draw_applied.store(0);
    g_clustered_draw_neutral_noop.store(0);
    g_clustered_draw_fail_open.store(0);
    g_clustered_draw_restore_fail.store(0);
    reset_integrated_draw_routes();
    for (auto &rx : g_material_receiver_runtime) {
        rx.seen.store(0);
        rx.accepted.store(0);
        rx.joined.store(0);
        rx.mr_active.store(0);
        rx.fail_open.store(0);
    }
    g_bloom_fx_draw_snapshots.store(0);
    g_bloom_fx_draw_authorized.store(0);
    g_bloom_fx_draw_rejected.store(0);
    dsrrl::runtime::stable_receiver_pipeline_reset();
    dsrrl::runtime::hemenvlerp_receiver_pipeline_reset();
    dsrrl::runtime::subsurface_receiver_pipeline_reset();
    dsrrl::runtime::hemdir3_receiver_pipeline_reset();
    dsrrl::runtime::upper_lower_receiver_pipeline_reset();
    g_subsurface.reset();
    dsrrl::runtime::material_owner_selection_reset_stats();

    const auto receivers =
        dsrrl::operators::material_response::
            register_confirmed_material_receivers_v1(
                g_material_response);
    const auto routes =
        dsrrl::operators::material_response::
            register_confirmed_material_routes_v1(
                g_material_response);
    const bool mr_finalized =
        g_material_response.finalize_registration();

    g_mr_ready.store(
        receivers == 24u &&
        routes == 43u &&
        mr_finalized &&
        g_material_response.registration_finalized() &&
        g_material_response.receiver_recipe_count() == 24u &&
        g_material_response.material_profile_count() == 43u);

    if (!enable_integrated_islands()) {
        disable_integrated_islands();
        reshade::unregister_addon(addon_module, reshade_module);
        return false;
    }

    register_events();

    if (k_drawtime_islands_runtime_enabled &&
        k_draw_callbacks_runtime_enabled &&
        k_raw_draw_replay_bisect) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] RAW DRAW REPLAY BISECT: draw callbacks issue one stock native Draw/DrawIndexed and suppress the original; DSRRL semantic preparation and state mutation are skipped.");
    } else if (k_drawtime_islands_runtime_enabled &&
               k_draw_callbacks_runtime_enabled &&
               k_state_transaction_only_bisect) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] STATE TRANSACTION ONLY BISECT: full routing/preparation plus begin(snapshot+mutation) and restore are active, but no replacement Draw/DrawIndexed is issued.");
    } else if (k_drawtime_islands_runtime_enabled &&
        k_draw_callbacks_runtime_enabled &&
        k_empty_draw_callback_bisect) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] EMPTY DRAW CALLBACK BISECT: dynamic transports and draw callbacks are installed, but draw/draw_indexed return immediately before all DSRRL draw work.");
    } else if (k_drawtime_islands_runtime_enabled &&
        !k_draw_callbacks_runtime_enabled) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] DRAW CALLBACKS BYPASSED: all dynamic FLVER/MTD/resource transports remain installed; draw/draw_indexed/present callbacks are omitted.");
    } else if (k_drawtime_islands_runtime_enabled &&
               !k_draw_replay_runtime_enabled) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] DRAW REPLAY BYPASSED: dynamic transports, draw routing, semantic joins and island preparation remain active; prepared islands are released before snapshot/mutate/replay/restore.");
    }

    if (!k_drawtime_islands_runtime_enabled) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] DRAW-TIME ISLANDS BYPASSED: A1 and MotionBlur create-time replacements remain active; FLVER/MTD selector transport, texture/resource bridges, P_Metal, PointLight and draw replay are not installed.");

        char build_identity[768]{};
        std::snprintf(
            build_identity,
            sizeof(build_identity),
            "[DSRRL BUILD_ID] version=%s source_commit=%s flavor=%s",
            DSRRL_CORE_ISLANDS_VERSION,
            DSRRL_SOURCE_COMMIT,
            DSRRL_BUILD_FLAVOR);
        reshade::log::message(
            reshade::log::level::info,
            build_identity);
        return true;
    }

    if (!g_material_resources.register_events()) {
        unregister_events();
        disable_integrated_islands();
        reshade::unregister_addon(addon_module, reshade_module);
        return false;
    }

    const bool envspec_resource_events =
        g_envspec_resources.register_events();

    if (!envspec_resource_events) {
        (void)g_core.features().set(
            dsrrl::core::operator_id::env_spec,
            false);
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] EnvSpec resource events FAIL-OPEN: P_Metal EnvSpec stays stock; other islands remain active.");
    }

    const bool texture_hooks =
        dsrrl::runtime::texture_identity_transport::install();

    if (!texture_hooks) {
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] texture identity hooks FAIL-OPEN: SpecRGB/Diffuse/Normal sidecars remain stock.");
    }

    const bool upper_lower_enabled =
        g_core.features().enabled(
            dsrrl::core::operator_id::upper_lower);
    const bool hemdir3_enabled =
        g_core.features().enabled(
            dsrrl::core::operator_id::hemdir3);

    const bool flver_hooks =
        dsrrl::runtime::flver_identity_transport::install(
            k_pointlight_drawtime_runtime_enabled,
            upper_lower_enabled,
            hemdir3_enabled);

    if (!flver_hooks) {
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] FLVER identity hooks FAIL-OPEN: stock DSR preserved for exact owner routing.");
    }

    const bool fixed_pointlight_hooks =
        k_pointlight_drawtime_runtime_enabled &&
        flver_hooks &&
        g_fixed_pointlight.install();

    g_fixed_pointlight_selection_transport_active.store(
        fixed_pointlight_hooks,
        std::memory_order_release);

#if defined(DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL)
    reshade::log::message(
        reshade::log::level::info,
        "[DSRRL PHYSICAL CUT POINTLIGHT] fixed_r40=PHYSICALLY_CUT clustered_spc=PHYSICALLY_CUT clustered_nospc=PHYSICALLY_CUT stock_dsr=ON");
#endif
    if (!k_pointlight_drawtime_runtime_enabled) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] PointLight draw-time runtime BYPASSED by diagnostic policy: fixed producer hook, clustered producer hook, PointLight pipeline routing and draw-time carrier/replay are inactive; stock DSR PointLight is preserved.");
    } else if (!fixed_pointlight_hooks) {
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] Fixed PointLight transport FAIL-OPEN: raw-q t19 remains unavailable; stock DSR fixed PointLight preserved.");
    }

    const bool clustered_pointlight_hooks =
        k_pointlight_drawtime_runtime_enabled &&
        flver_hooks &&
        g_clustered_pnts.install();

    g_clustered_pointlight_selection_transport_active.store(
        clustered_pointlight_hooks,
        std::memory_order_release);

    if (k_pointlight_drawtime_runtime_enabled &&
        !clustered_pointlight_hooks) {
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] Clustered PntS transport FAIL-OPEN: first-four sidecar remains unavailable; stock DSR clustered PointLight preserved.");
    } else if (clustered_pointlight_hooks) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL POINTLIGHT R20] selector_authority_cache=ACTIVE source_selection_cache=PER_PRODUCER_SERIAL gpu_payload_dedupe=PER_CONTEXT draw_cpu_traversal=OFF");
    }

    // Bloom FX transport is diagnostic-only: it does not authorize Q8,
    // Bloom, WaterWaveSfx, or pixels. Do not keep five inline FX hooks and
    // their registry/census work armed in production. The same explicit
    // telemetry switch used for render-hot counters enables the diagnostic
    // transport for census sessions.
    const bool bloom_fx_diagnostics =
        g_hot_telemetry_enabled;
    const bool bloom_fx_hooks =
        bloom_fx_diagnostics &&
        flver_hooks &&
        dsrrl::runtime::bloom_fx_draw_transport::install();

    if (bloom_fx_diagnostics &&
        !bloom_fx_hooks) {
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] Bloom FX diagnostic transport FAIL-OPEN: Q8 sidecar remains unauthorised; stock SFX preserved.");
    } else if (!bloom_fx_diagnostics) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] Bloom FX diagnostic hooks disabled for production runtime.");
    }

    const bool hemdir3_mode_hooks =
        !hemdir3_enabled ||
        (flver_hooks &&
         dsrrl::runtime::hemdir3_mode_transport::install());

    g_hemdir3_selection_transport_active.store(
        hemdir3_enabled && hemdir3_mode_hooks,
        std::memory_order_release);

    if (hemdir3_enabled && !hemdir3_mode_hooks) {
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] HemDir3 effective-mode hooks FAIL-OPEN: HemDir3 remains stock.");
    } else if (!hemdir3_enabled) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] HemDir3 visible bridge DISABLED by runtime policy; mode hooks not installed and stock DSR is preserved.");
    }

    if (!g_core.features().enabled(
            dsrrl::core::operator_id::subsurface)) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] Subsurface visible bridge DISABLED by runtime policy; stock DSR body/Subsurface is preserved.");
    }

    bool pmetal_envspec_enabled =
        g_core.features().enabled(
            dsrrl::core::operator_id::env_spec);
    const bool pmetal_source_ready =
        !pmetal_envspec_enabled ||
        (flver_hooks &&
         g_pmetal_source.install());

    // EnvSpec uses embedded PTDE donors selected by an exact FLVER callback.
    // Only visible U/L may arm the global LightBank hook set.
    const bool lightbank_reference_transport_required =
        upper_lower_enabled;
    const bool lightbank_reference_hooks =
        !lightbank_reference_transport_required ||
        (flver_hooks &&
         g_upper_lower.install(false));

    g_upper_lower_selection_transport_active.store(
        upper_lower_enabled && lightbank_reference_hooks,
        std::memory_order_release);

    g_any_draw_selection_transport_active.store(
        g_upper_lower_selection_transport_active.load(
            std::memory_order_relaxed) ||
        g_hemdir3_selection_transport_active.load(
            std::memory_order_relaxed) ||
        g_fixed_pointlight_selection_transport_active.load(
            std::memory_order_relaxed) ||
        g_clustered_pointlight_selection_transport_active.load(
            std::memory_order_relaxed),
        std::memory_order_release);

    if (!upper_lower_enabled) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] Upper/Lower visible bridge DISABLED by runtime policy: stock DSR U/L preserved; LightBank U/L/reference hooks are not installed.");
    }

    if (upper_lower_enabled &&
        !lightbank_reference_hooks) {
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] LightBank transport FAIL-OPEN: U/L stays stock.");
    }

    if (
        pmetal_envspec_enabled &&
        !pmetal_source_ready) {
        (void)g_core.features().set(
            dsrrl::core::operator_id::env_spec,
            false);
        pmetal_envspec_enabled = false;
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL PMETAL ENVSPEC] SOURCE FAIL-OPEN: exact selector PTDE donor carrier unavailable; stock DSR EnvSpec preserved.");
    } else if (
        pmetal_envspec_enabled &&
        pmetal_source_ready) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL PMETAL ENVSPEC] exact selector carrier + narrow retail LightBank single/blend PTDE source fallback ACTIVE; visible U/L remains stock/off.");
    }

    publish_active_dynamic_draw_routes();

    {
        char route_line[256]{};
        std::snprintf(
            route_line,
            sizeof(route_line),
            "[DSRRL RUNTIME V2] active_dynamic_route_mask=0x%02X (Subsurface/U-L/HemDir3 absent when disabled or unavailable)",
            static_cast<unsigned>(
                g_active_dynamic_draw_route_mask.load(
                    std::memory_order_relaxed)));
        reshade::log::message(
            reshade::log::level::info,
            route_line);
    }

    {
        char build_identity[768]{};
        std::snprintf(
            build_identity,
            sizeof(build_identity),
            "[DSRRL BUILD_ID] version=%s source_commit=%s flavor=%s",
            DSRRL_CORE_ISLANDS_VERSION,
            DSRRL_SOURCE_COMMIT,
            DSRRL_BUILD_FLAVOR);
        reshade::log::message(
            reshade::log::level::info,
            build_identity);
    }

    reshade::log::message(
        reshade::log::level::info,
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
        "] READY: shared Core draw-state transaction layer (PS/CB/SRV/sampler) "
        "is active; generic Material Response is the direct stock->PTDE diffuse "
        "material-domain operator for HemEnv/HemEnvLerp. PTDE c101/c102 remain operator-local "
        "carriers for verified EnvSpec/local-specular consumers; generic SpecRGB->DSR PBL "
        "pairing is forbidden. P_Metal EnvSpec stays stock when its narrow source carrier is "
        "unavailable; the U/L-off runtime does not arm shared LightBank hooks merely to feed "
        "EnvSpec. Runtime activation and "
        "PTDE pixel behavior remain separate validation stages; "
        "frozen legacy monolith is not linked.");

    return true;
}

extern "C" __declspec(dllexport)
void AddonUninit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    if (k_addon_loaded_only_bisect) {
        reshade::unregister_addon(addon_module, reshade_module);
        return;
    }

    if (k_draw_callback_only_bisect) {
        reshade::unregister_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
        reshade::unregister_event<reshade::addon_event::draw>(on_draw);
        reshade::unregister_addon(addon_module, reshade_module);
        return;
    }

    unregister_events();
    log_state("PRE_UNLOAD");
    log_effect_matrix("PRE_UNLOAD");
    motion_blur_camera_fallback_disable::
        log_state("PRE_UNLOAD");
    g_pmetal_native_draw.uninstall();
    g_pmetal_source.uninstall();
    g_upper_lower.uninstall();

    if (g_upper_lower.telemetry().restore_failed)
        log_state("UL_UNLOAD_RESTORE_FAIL");

    dsrrl::runtime::hemdir3_mode_transport::uninstall();

    if (dsrrl::runtime::hemdir3_mode_transport::status().restore_failed)
        log_state("MODE2_UNLOAD_RESTORE_FAIL");

    dsrrl::runtime::bloom_fx_draw_transport::uninstall();

    if (dsrrl::runtime::bloom_fx_draw_transport::status().restore_failed)
        log_state("BLOOM_FX_UNLOAD_RESTORE_FAIL");

    g_clustered_pnts.uninstall();

    g_fixed_pointlight.uninstall();

    if (g_fixed_pointlight.telemetry().restore_failed)
        log_state("FIXED_PL_UNLOAD_RESTORE_FAIL");

    dsrrl::runtime::flver_identity_transport::uninstall();

    if (dsrrl::runtime::flver_identity_transport::status().restore_failed)
        log_state("UNLOAD_RESTORE_FAIL");

    dsrrl::runtime::stable_receiver_pipeline_reset();
    dsrrl::runtime::hemenvlerp_receiver_pipeline_reset();
    dsrrl::runtime::subsurface_receiver_pipeline_reset();
    dsrrl::runtime::hemdir3_receiver_pipeline_reset();
    dsrrl::runtime::upper_lower_receiver_pipeline_reset();
    reset_integrated_draw_routes();
    dsrrl::runtime::texture_identity_transport::uninstall();
    g_envspec_resources.unregister_events();
    g_material_resources.unregister_events();
    g_pmetal_envspec.reset();
    g_pmetal_source.reset();
    g_envspec_resources.reset_stats();
    g_bloom_scene_sidecar.reset();
    dsrrl::runtime::bloom_fx_draw_transport::reset_stats();
    g_material_resources.reset();
    g_upper_lower.reset();
    g_upper_lower_hemenv.reset();
    g_hemdir3.reset();
    g_fixed_pointlight.reset();
    g_fixed_pointlight_pipeline.reset();
    g_clustered_pnts.reset();
    g_clustered_pnts_pipeline.reset();
    g_mr_draw_runtime.reset();
    g_draw_transactions.reset();
    g_a1_bridge.reset();
    motion_blur_camera_fallback_disable::
        reset();
    disable_integrated_islands();

    reshade::unregister_addon(addon_module, reshade_module);
}
