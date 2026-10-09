#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"
#if defined(DSRRL_PMETAL_ASYLUM_NATIVE_MAP_CUT)
#include "dsrrl/runtime/pmetal_native_map_cut.hpp"
#endif
#if defined(DSRRL_PMETAL_ASYLUM_DEFERRED_TOPOLOGY_TRACE)
#include "dsrrl/runtime/pmetal_asylum_cmdlist_trace.hpp"
#endif
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include "dsrrl/runtime/pixel_srv_shadow.hpp"
#if defined(DSRRL_PMETAL_ASYLUM_CB_WRITER_TRACE)
#include "dsrrl/runtime/pmetal_native_cb_writer_trace.hpp"
#endif

#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <Windows.h>
#include <d3d11.h>
#if defined(DSRRL_PMETAL_ASYLUM_NATIVE_PASS_BINDING_TRACE)
#include <d3d11_1.h>
#endif

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace dsrrl::runtime {
namespace {

namespace mr = operators::material_response;
namespace hashing = operators::legacy_plan::hashing;

constexpr std::uint32_t k_pmetal_route_index = 345u;
constexpr const char *k_pmetal_name =
    "P_Metal[DSB].mtd";
constexpr const char *k_pmetal_sha256 =
    "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b";

constexpr std::uint32_t k_effect_fail_feature = 1u << 0u;
constexpr std::uint32_t k_effect_fail_lerp_feature = 1u << 1u;
constexpr std::uint32_t k_effect_fail_material = 1u << 2u;
constexpr std::uint32_t k_effect_fail_semantic = 1u << 3u;
constexpr std::uint32_t k_effect_fail_source = 1u << 4u;
constexpr std::uint32_t k_effect_fail_blend = 1u << 5u;
constexpr std::uint32_t k_effect_fail_context = 1u << 7u;
constexpr std::uint32_t k_effect_fail_replacement = 1u << 8u;
constexpr std::uint32_t k_effect_fail_probe = 1u << 10u;
constexpr std::uint32_t k_effect_fail_spec_rgb = 1u << 11u;
constexpr std::uint32_t k_effect_fail_device = 1u << 12u;
constexpr std::uint32_t k_effect_fail_b12 = 1u << 13u;
constexpr std::uint32_t k_effect_fail_mutation = 1u << 14u;
constexpr std::uint32_t k_effect_fail_native_envdiffuse = 1u << 15u;

std::atomic_bool g_source_cut_logged{false};
std::atomic_bool g_resource_mode_logged{false};
std::atomic_bool g_native_envdiffuse_logged{false};
std::atomic_bool g_envdiffuse_consumer_logged{false};
std::atomic_bool g_source_frontier_logged{false};
std::atomic_bool g_shadow_r7_logged{false};
#if !defined(DSRRL_RELEASE_CLEANUP)
std::atomic<std::uint32_t> g_prepare_stage_log_mask{0u};
std::atomic<std::uint32_t> g_value_cut_log_mask{0u};
#endif

void log_prepare_stage_once(
    std::uint32_t bit,
    const char *stage,
    const mr::material_identity &material,
    const mr::decision &decision,
    pmetal_envspec_receiver_family family) noexcept
{
#if defined(DSRRL_RELEASE_CLEANUP)
    (void)bit;
    (void)stage;
    (void)material;
    (void)decision;
    (void)family;
#else
    const auto observed =
        g_prepare_stage_log_mask.load(
            std::memory_order_relaxed);
    if ((observed & bit) != 0u ||
        (g_prepare_stage_log_mask.fetch_or(
             bit,
             std::memory_order_relaxed) & bit) != 0u)
        return;

    char line[640]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL ENVSPEC APPLY] stage=%s family=%u rx=%u route=%u active=%u mat_valid=%u owner_exact=%u slot=%u slot_valid=%u sem=%016llx flver=%016llx",
        stage,
        static_cast<unsigned>(family),
        static_cast<unsigned>(decision.receiver_id),
        static_cast<unsigned>(decision.route_index),
        decision.active ? 1u : 0u,
        material.valid ? 1u : 0u,
        material.owner_tuple_exact ? 1u : 0u,
        static_cast<unsigned>(material.material_slot),
        material.material_slot_valid ? 1u : 0u,
        static_cast<unsigned long long>(
            material.semantic_name_hash),
        static_cast<unsigned long long>(
            material.flver_identity_hash));
    reshade::log::message(
        reshade::log::level::info,
        line);
#endif
}

#if defined(DSRRL_PMETAL_FORCE_PTDE_PACKEDGI)
constexpr bool k_native_dsr_cubemap_feed = false;
#elif defined(DSRRL_PMETAL_NATIVE_DSR_CUBEMAP_FEED) || \
      defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) || \
      defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
constexpr bool k_native_dsr_cubemap_feed = true;
#else
constexpr bool k_native_dsr_cubemap_feed = false;
#endif

#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG)
constexpr bool k_v13_native_dsr_no_tail_diag = true;
#else
constexpr bool k_v13_native_dsr_no_tail_diag = false;
#endif

#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
constexpr bool k_v13_native_dsr_material_mod_diag = true;
#else
constexpr bool k_v13_native_dsr_material_mod_diag = false;
#endif

constexpr bool k_v13_preserve_stock_envdiffuse =
    k_v13_native_dsr_no_tail_diag ||
    k_v13_native_dsr_material_mod_diag;

void effect_latch(std::atomic_bool &flag) noexcept
{
    if (!telemetry::effect_enabled() ||
        flag.load(std::memory_order_relaxed))
        return;
    flag.store(true, std::memory_order_relaxed);
}

void effect_fail(
    std::atomic<std::uint32_t> &mask,
    std::uint32_t bit) noexcept
{
    if (!telemetry::effect_enabled())
        return;

    const auto current =
        mask.load(std::memory_order_relaxed);
    if ((current & bit) != 0u)
        return;

    mask.fetch_or(bit, std::memory_order_relaxed);
}

bool exact_pmetal_material(
    const mr::material_identity &material) noexcept
{
    return
        material.valid &&
        material.owner_tuple_exact &&
        material.material_slot_valid &&
        material.semantic_name_hash ==
            mr::mtd_semantic_hash(
                k_pmetal_name) &&
        hashing::matches_hex(
            material.raw_mtd_sha256,
            k_pmetal_sha256);
}

bool exact_pmetal_decision(
    const mr::decision &decision) noexcept
{
    // EnvSpec owns its own PTDE material consumer. Do not borrow the generic
    // Material Response specular-operation bit: generic MR is diffuse-only.
    // Exact P_Metal identity + route/receiver select the verified profile, and
    // raw c101 is consumed explicitly by the EnvSpec SpecRGB material tail.
    return
        decision.active &&
        decision.route_index ==
            k_pmetal_route_index &&
        decision.receiver_id >= 33u &&
        decision.receiver_id <= 35u &&
        std::isfinite(decision.c101) &&
        decision.c101 >= 0.0f;
}

mr::mtd_semantic_query make_query(
    const mr::material_identity &material,
    std::uint32_t receiver_id) noexcept
{
    mr::mtd_semantic_query query{};
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
    return query;
}

struct f4 {
    float x;
    float y;
    float z;
    float w;
};

} // namespace

bool exact_pmetal_envspec_candidate(
    const operators::material_response::material_identity &material,
    const operators::material_response::decision &decision) noexcept
{
    return
        exact_pmetal_material(material) &&
        exact_pmetal_decision(decision);
}

pmetal_envspec_draw_runtime::
pmetal_envspec_draw_runtime(
    core::renderer_core &core,
    pmetal_env_source_runtime &source,
    envspec_resource_runtime &env_resources,
    material_resource_draw_runtime &material_resources) noexcept
    : core_(core),
      source_(source),
      env_resources_(env_resources),
      material_resources_(material_resources)
{
}

pmetal_envspec_draw_runtime::
~pmetal_envspec_draw_runtime()
{
    release_resources();
}

void pmetal_envspec_draw_runtime::
release_resources() noexcept
{
    std::lock_guard<std::mutex> lock(
        mutex_);

    for (auto &[_,record] :
         replacements_) {
        if (record.shader != nullptr)
            record.shader->Release();
    }
    replacements_.clear();

    for (auto &[_,shader] :
         lerp_replacements_) {
        if (shader != nullptr)
            shader->Release();
    }
    lerp_replacements_.clear();
    lerp_replacement_sha256_.clear();
    lerp_replacement_size_.clear();

    for (auto &[_,entry] :
         b12_by_context_) {
        if (entry.buffer != nullptr)
            entry.buffer->Release();
    }
    b12_by_context_.clear();

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW)
    if (shadow_sampler_ != nullptr) {
        shadow_sampler_->Release();
        shadow_sampler_ = nullptr;
    }
#endif

    if (device_ != nullptr) {
        device_->Release();
        device_ = nullptr;
    }
}

void pmetal_envspec_draw_runtime::
on_init_device(
    reshade::api::device *device) noexcept
{
    if (device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11)
        return;

    auto *native =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());

    if (native == nullptr)
        return;

    std::lock_guard<std::mutex> lock(
        mutex_);

    if (device_ == nullptr) {
        native->AddRef();
        device_ = native;

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW)
        // Exact D3D11 semantic equivalent of PTDE DATA.exe stage-7 shadow
        // sampler: D3DTEXF_POINT for MIN/MAG/MIP, BORDER on U/V/W, white
        // border, MaxMipLevel=0. D3D11 MaxAnisotropy must be >=1 even though
        // it is ignored by point filtering; ComparisonFunc is likewise
        // ignored for the non-comparison SAMPLE path.
        D3D11_SAMPLER_DESC shadow_desc{};
        shadow_desc.Filter =
            D3D11_FILTER_MIN_MAG_MIP_POINT;
        shadow_desc.AddressU =
            D3D11_TEXTURE_ADDRESS_BORDER;
        shadow_desc.AddressV =
            D3D11_TEXTURE_ADDRESS_BORDER;
        shadow_desc.AddressW =
            D3D11_TEXTURE_ADDRESS_BORDER;
        shadow_desc.MipLODBias = 0.0f;
        shadow_desc.MaxAnisotropy = 1u;
        shadow_desc.ComparisonFunc =
            D3D11_COMPARISON_NEVER;
        shadow_desc.BorderColor[0] = 1.0f;
        shadow_desc.BorderColor[1] = 1.0f;
        shadow_desc.BorderColor[2] = 1.0f;
        shadow_desc.BorderColor[3] = 1.0f;
        shadow_desc.MinLOD = 0.0f;
        shadow_desc.MaxLOD =
            D3D11_FLOAT32_MAX;

        if (FAILED(
                native->CreateSamplerState(
                    &shadow_desc,
                    &shadow_sampler_)))
            shadow_sampler_ = nullptr;
#endif
        return;
    }

    if (device_ != native)
        quarantined_.store(true);
}

void pmetal_envspec_draw_runtime::
on_destroy_device(
    reshade::api::device *device) noexcept
{
    if (device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11)
        return;

    auto *native =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());

    std::lock_guard<std::mutex> lock(
        mutex_);

    if (native != device_)
        return;

    for (auto &[_,record] :
         replacements_) {
        if (record.shader != nullptr)
            record.shader->Release();
    }
    replacements_.clear();

    for (auto &[_,shader] :
         lerp_replacements_) {
        if (shader != nullptr)
            shader->Release();
    }
    lerp_replacements_.clear();
    lerp_replacement_sha256_.clear();
    lerp_replacement_size_.clear();

    for (auto &[_,entry] :
         b12_by_context_) {
        if (entry.buffer != nullptr)
            entry.buffer->Release();
    }
    b12_by_context_.clear();

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW)
    if (shadow_sampler_ != nullptr) {
        shadow_sampler_->Release();
        shadow_sampler_ = nullptr;
    }
#endif

    if (device_ != nullptr) {
        device_->Release();
        device_ = nullptr;
    }
}

bool pmetal_envspec_draw_runtime::
register_replacement(
    const operators::env_spec::
        pmetal_rgba_materialize_outcome &outcome,
    const void *dxbc,
    std::size_t dxbc_size) noexcept
{
    using result =
        operators::env_spec::
            pmetal_rgba_materialize_result;

    // Final Renderer Edition policy keeps visible U/L disabled. P_Metal
    // EnvSpec therefore accepts only the U/L-independent payload that
    // preserves the stock DSR b0[7]/b0[8] continuation.
    if (outcome.result != result::applied ||
        outcome.receiver_id < 33u ||
        outcome.receiver_id > 35u ||
        outcome.upper_lower_composed ||
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW)
        (((outcome.receiver_id == 33u ||
           outcome.receiver_id == 34u) &&
          !outcome.shadow_visibility_kernel_composed) ||
         (outcome.receiver_id == 35u &&
          outcome.shadow_visibility_kernel_composed)) ||
#endif
#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG)
        outcome.spec_rgb_consumer ||
        outcome.envdiffuse_linear_consumer_diag ||
#elif defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
        !outcome.spec_rgb_consumer ||
        outcome.envdiffuse_linear_consumer_diag ||
#else
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
        !outcome.phn_scene_encoding_composed ||
#endif
        !outcome.spec_rgb_consumer ||
        !outcome.envdiffuse_linear_consumer_diag ||
#endif
        dxbc == nullptr ||
        dxbc_size == 0u ||
        quarantined_.load()) {
        ++replacement_register_fail_;
        return false;
    }

    const auto payload_sha256 =
        hashing::sha256(
            static_cast<const std::uint8_t *>(
                dxbc),
            dxbc_size);

    std::lock_guard<std::mutex> lock(
        mutex_);

    if (device_ == nullptr) {
        ++replacement_register_fail_;
        return false;
    }

    auto &record =
        replacements_[outcome.receiver_id];

    if (record.shader != nullptr) {
        const bool same_payload =
            record.composed_owners ==
                outcome.composed_owners &&
            record.payload_size ==
                dxbc_size &&
            record.payload_sha256 ==
                payload_sha256;

        if (same_payload) {
            ++replacement_register_ok_;
            return true;
        }

        quarantined_.store(
            true,
            std::memory_order_release);
        ++replacement_register_fail_;
        return false;
    }

    ID3D11PixelShader *shader = nullptr;
    if (FAILED(
            device_->CreatePixelShader(
                dxbc,
                dxbc_size,
                nullptr,
                &shader)) ||
        shader == nullptr) {
        ++replacement_register_fail_;
        return false;
    }

    record.shader = shader;
    record.composed_owners =
        outcome.composed_owners;
    record.payload_sha256 =
        payload_sha256;
    record.payload_size =
        dxbc_size;

    ++replacement_register_ok_;
    return true;
}

bool pmetal_envspec_draw_runtime::
register_lerp_replacement(
    const operators::env_spec::
        pmetal_rgba_lerp_materialize_outcome &outcome,
    const void *dxbc,
    std::size_t dxbc_size) noexcept
{
    using result =
        operators::env_spec::
            pmetal_rgba_lerp_materialize_result;

    if (outcome.result != result::applied ||
        outcome.semantic_receiver_id < 33u ||
        outcome.semantic_receiver_id > 35u ||
        outcome.pair_index + 24u !=
            outcome.semantic_receiver_id ||
#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
        outcome.envdiffuse_preserved ||
#else
        !outcome.envdiffuse_preserved ||
#endif
        outcome.upper_lower_composed ||
        !outcome.upper_lower_preserved_stock ||
#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG)
        outcome.terminal_sat_rgb_composed ||
        outcome.spec_rgb_consumer ||
#elif defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
        outcome.terminal_sat_rgb_composed ||
        !outcome.spec_rgb_consumer ||
#else
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
        !outcome.phn_scene_encoding_composed ||
#endif
        !outcome.terminal_sat_rgb_composed ||
        !outcome.spec_rgb_consumer ||
#endif
        dxbc == nullptr ||
        dxbc_size == 0u ||
        quarantined_.load()) {
        ++lerp_replacement_register_fail_;
        return false;
    }

    const auto payload_sha256 =
        hashing::sha256(
            static_cast<const std::uint8_t *>(
                dxbc),
            dxbc_size);

    std::lock_guard<std::mutex> lock(
        mutex_);

    if (device_ == nullptr) {
        ++lerp_replacement_register_fail_;
        return false;
    }

    const auto existing =
        lerp_replacements_.find(
            outcome.semantic_receiver_id);

    if (existing !=
            lerp_replacements_.end() &&
        existing->second != nullptr) {
        const auto sha_it =
            lerp_replacement_sha256_.find(
                outcome.semantic_receiver_id);
        const auto size_it =
            lerp_replacement_size_.find(
                outcome.semantic_receiver_id);
        const bool same_payload =
            sha_it !=
                lerp_replacement_sha256_.end() &&
            size_it !=
                lerp_replacement_size_.end() &&
            sha_it->second ==
                payload_sha256 &&
            size_it->second ==
                dxbc_size;

        if (same_payload) {
            ++lerp_replacement_register_ok_;
            return true;
        }

        quarantined_.store(
            true,
            std::memory_order_release);
        ++lerp_replacement_register_fail_;
        return false;
    }

    ID3D11PixelShader *shader = nullptr;
    if (FAILED(
            device_->CreatePixelShader(
                dxbc,
                dxbc_size,
                nullptr,
                &shader)) ||
        shader == nullptr) {
        ++lerp_replacement_register_fail_;
        return false;
    }

    lerp_replacements_[
        outcome.semantic_receiver_id] =
        shader;
    lerp_replacement_sha256_[
        outcome.semantic_receiver_id] =
        payload_sha256;
    lerp_replacement_size_[
        outcome.semantic_receiver_id] =
        dxbc_size;

    ++lerp_replacement_register_ok_;
    return true;
}

bool pmetal_envspec_draw_runtime::prepare(
    reshade::api::command_list *cmd_list,
    const mr::material_identity &material,
    const mr::decision &decision,
    prepared_pmetal_envspec_draw &prepared) noexcept
{
    return prepare(
        cmd_list,
        material,
        decision,
        pmetal_envspec_receiver_family::
            stable_hemenv,
        prepared);
}

bool pmetal_envspec_draw_runtime::prepare(
    reshade::api::command_list *cmd_list,
    const mr::material_identity &material,
    const mr::decision &decision,
    pmetal_envspec_receiver_family family,
    prepared_pmetal_envspec_draw &prepared) noexcept
{
    prepared = {};
    effect_latch(effect_entry_seen_);

    if (cmd_list == nullptr ||
        quarantined_.load()) {
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_feature);
        log_prepare_stage_once(
            1u << 1u,
            "feature_reject",
            material,
            decision,
            family);
        return false;
    }

    // P_Metal is a very narrow route. Reject ordinary MR draws before taking
    // the feature-registry mutexes below; otherwise every active material draw
    // pays EnvSpec feature checks even though only exact route 345 / P_Metal
    // can ever reach this island.
    telemetry::hot_count(candidates_);
    if (family ==
        pmetal_envspec_receiver_family::
            hemenvlerp)
        telemetry::hot_count(lerp_candidates_);

    if (!exact_pmetal_envspec_candidate(
            material,
            decision)) {
        telemetry::hot_count(material_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_material);
        log_prepare_stage_once(
            1u << 0u,
            "material_or_decision_reject",
            material,
            decision,
            family);
        return false;
    }
    effect_latch(effect_material_ready_);

    if (!core_.features().enabled(
            core::operator_id::env_spec)) {
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_feature);
        return false;
    }
    effect_latch(effect_feature_ready_);

    if (family ==
            pmetal_envspec_receiver_family::
                hemenvlerp &&
        (!core_.features().enabled(
             core::operator_id::material_response) ||
         !core_.features().enabled(
             core::operator_id::diffuse_material_domain) ||
         !core_.features().enabled(
             core::operator_id::spec_rgb) ||
         !core_.features().enabled(
             core::operator_id::terminal_sat_rgb))) {
        telemetry::hot_count(semantic_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_lerp_feature);
        log_prepare_stage_once(
            1u << 2u,
            "lerp_feature_reject",
            material,
            decision,
            family);
        return false;
    }

    const auto query =
        make_query(
            material,
            decision.receiver_id);

    const auto env_semantics =
        mr::classify_mtd_envspec_semantics(
            query);

    if (!env_semantics.exact_identity_match ||
        env_semantics.presence !=
            mr::ptde_envspec_presence::
                present ||
        env_semantics.router_state !=
            mr::mtd_envspec_router_state::
                present ||
        !env_semantics.envspc_slot_valid ||
        env_semantics.envspc_slot != 2u) {
        telemetry::hot_count(semantic_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_semantic);
        log_prepare_stage_once(
            1u << 3u,
            "semantic_reject",
            material,
            decision,
            family);
        return false;
    }
    effect_latch(effect_semantic_ready_);

    pmetal_envspec_source source{};
    if (!source_.latest(material, source) ||
        !std::isfinite(source.beta)) {
        telemetry::hot_count(source_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_source);
        log_prepare_stage_once(
            1u << 4u,
            "source_reject",
            material,
            decision,
            family);

        if (!g_source_frontier_logged.exchange(
                true,
                std::memory_order_relaxed)) {
            const auto source_state =
                source_.telemetry();
            char source_line[1280]{};
            std::snprintf(
                source_line,
                sizeof(source_line),
                "[DSRRL ENVSPEC SOURCE] exact_seen=%u parent=%u descriptor=%u endpoint=%u manager=%u decode_a=%u decode_b=%u exact_publish=%llu decode_fail=%llu consumer_ok=%llu consumer_fail=%llu hook_single=%llu hook_blend=%llu hook_publish=%llu hook_consume=%llu hook_decode=%u ver=%u count=%u index=%u row=%u sig=%016llx layout=%016llx bankscan=%llu/%u bankstage=%u entry=%u name_off=%08x consumed=%u pub_tid=%u con_tid=%u con_tls=%u selector_active=%u hook_single_armed=%u hook_blend_armed=%u",
                source_state.selector_exact_seen ? 1u : 0u,
                source_state.parent_gate_ok ? 1u : 0u,
                source_state.descriptor_gate_ok ? 1u : 0u,
                source_state.endpoint_gate_ok ? 1u : 0u,
                source_state.manager_gate_ok ? 1u : 0u,
                source_state.source_a_decode_ok ? 1u : 0u,
                source_state.source_b_decode_ok ? 1u : 0u,
                static_cast<unsigned long long>(
                    source_state.exact_publish),
                static_cast<unsigned long long>(
                    source_state.decode_fail),
                static_cast<unsigned long long>(
                    source_state.consumer_ok),
                static_cast<unsigned long long>(
                    source_state.consumer_fail),
                static_cast<unsigned long long>(
                    source_state.hook_single_seen),
                static_cast<unsigned long long>(
                    source_state.hook_blend_seen),
                static_cast<unsigned long long>(
                    source_state.hook_publish),
                static_cast<unsigned long long>(
                    source_state.hook_consume),
                static_cast<unsigned>(
                    source_state.hook_decode_stage),
                static_cast<unsigned>(
                    source_state.hook_decode_version),
                static_cast<unsigned>(
                    source_state.hook_decode_count),
                static_cast<unsigned>(
                    source_state.hook_decode_index),
                static_cast<unsigned>(
                    source_state.hook_decode_row_id),
                static_cast<unsigned long long>(
                    source_state.hook_decode_signature),
                static_cast<unsigned long long>(
                    source_state.bank_layout_signature),
                static_cast<unsigned long long>(
                    source_state.bank_signature_scan_count),
                static_cast<unsigned>(
                    source_state.bank_signature_attempt),
                static_cast<unsigned>(
                    source_state.bank_signature_stage),
                static_cast<unsigned>(
                    source_state.bank_signature_entry),
                static_cast<unsigned>(
                    source_state.bank_signature_name_offset),
                static_cast<unsigned>(
                    source_state.bank_signature_consumed),
                static_cast<unsigned>(
                    source_state.last_publish_tid),
                static_cast<unsigned>(
                    source_state.last_consumer_tid),
                source_state.last_consumer_local_valid ? 1u : 0u,
                source_state.selector_carrier_active ? 1u : 0u,
                source_state.hook_single_armed ? 1u : 0u,
                source_state.hook_blend_armed ? 1u : 0u);
            reshade::log::message(
                reshade::log::level::info,
                source_line);
        }

        if (telemetry::effect_enabled() &&
            !g_source_cut_logged.exchange(
                true,
                std::memory_order_relaxed)) {
            const auto cut =
                source_.telemetry();
            char line[768]{};
            std::snprintf(
                line,
                sizeof(line),
                "[DSRRL PMETAL SOURCE CUT] rx=%u route=%u owner=%016llx slot=%u steady=%llu blend=%llu publish=%llu decode_fail=%llu consume_ok=%llu consume_fail=%llu publish_tid=%u consumer_tid=%u local_valid=%u frontier=%u/%u/%u/%u/%u/%u/%u selector_active=%u global_hooks=%u/%u",
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(decision.route_index),
                static_cast<unsigned long long>(
                    material.flver_identity_hash),
                static_cast<unsigned>(
                    material.material_slot),
                static_cast<unsigned long long>(
                    cut.steady_seen),
                static_cast<unsigned long long>(
                    cut.blend_seen),
                static_cast<unsigned long long>(
                    cut.exact_publish),
                static_cast<unsigned long long>(
                    cut.decode_fail),
                static_cast<unsigned long long>(
                    cut.consumer_ok),
                static_cast<unsigned long long>(
                    cut.consumer_fail),
                static_cast<unsigned>(
                    cut.last_publish_tid),
                static_cast<unsigned>(
                    cut.last_consumer_tid),
                cut.last_consumer_local_valid ? 1u : 0u,
                cut.selector_exact_seen ? 1u : 0u,
                cut.parent_gate_ok ? 1u : 0u,
                cut.descriptor_gate_ok ? 1u : 0u,
                cut.endpoint_gate_ok ? 1u : 0u,
                cut.manager_gate_ok ? 1u : 0u,
                cut.source_a_decode_ok ? 1u : 0u,
                cut.source_b_decode_ok ? 1u : 0u,
                cut.selector_carrier_active ? 1u : 0u,
                cut.steady_carrier_active ? 1u : 0u,
                cut.blend_carrier_active ? 1u : 0u);
            reshade::log::message(
                reshade::log::level::info,
                line);
        }

        return false;
    }
    effect_latch(effect_source_ready_);
#if defined(DSRRL_PMETAL_ASYLUM_NATIVE_MAP_CUT) && defined(DSRRL_PMETAL_ASYLUM_CB_WRITER_TRACE)
    // Exact per-draw RX33/RX34 native context and CURRENT PS b0.
    // Register target on first observation; later witnessed native Map/Unmap
    // is associated by physical context AND exact resource object identity.
    // A CPU Map receipt is NOT proof of subsequent GPU PS consumption.
    auto *native_map_ctx=reinterpret_cast<ID3D11DeviceContext *>(
        static_cast<std::uintptr_t>(cmd_list->get_native()));
    if (native_map_ctx) {
        (void)pmetal_native_map_cut::global_observer().install(native_map_ctx);
        ID3D11Buffer *bound_b0=nullptr;
        native_map_ctx->PSGetConstantBuffers(0u,1u,&bound_b0);
        if(bound_b0) {
            const auto b0_key=reinterpret_cast<std::uintptr_t>(bound_b0);
            const auto ctx_key=reinterpret_cast<std::uintptr_t>(native_map_ctx);
            const auto info=pmetal_native_map_cut::global_observer().observe(ctx_key,b0_key);
            const auto writer=pmetal_native_cb_writer_lookup(b0_key);
            static std::atomic<std::uint32_t> observations_logged{0};
            static std::atomic<std::uint64_t> last_emit_ms{0};
            const auto now=GetTickCount64();
            auto previous=last_emit_ms.load(std::memory_order_relaxed);
            if (now>=previous+250u &&
                last_emit_ms.compare_exchange_strong(previous,now,
                    std::memory_order_relaxed) &&
                observations_logged.fetch_add(1u,std::memory_order_relaxed)<180u) {
                char line[1000]{};
                std::snprintf(line,sizeof(line),
                    "[DSRRL PMETAL NATIVE MAP CUT] rx=%u family=%u sha0=%02x%02x%02x%02x slot=%u"
                    " ctx=%llx ps_b0=%llx src_bank=%016llx row=%u beta=%.6f"
                    " native_hook=%u registered_vtables=%u"
                    " exact_buffer_watch=%u map=%llu unmap=%llu write_unmap=%llu"
                    " last_map_ctx=%llx last_unmap_ctx=%llx map_type=%u"
                    " map_tid=%u unmap_tid=%u same_ctx=%u complete_pair=%u"
                    " writer_epoch=%llu writer_method=%u writer_complete=%u"
                    " pixel=UNVERIFIED gpu_consumer=UNVERIFIED",
                    decision.receiver_id,
                    static_cast<unsigned>(family),
                    static_cast<unsigned>(material.flver_sha256[0]),
                    static_cast<unsigned>(material.flver_sha256[1]),
                    static_cast<unsigned>(material.flver_sha256[2]),
                    static_cast<unsigned>(material.flver_sha256[3]),
                    static_cast<unsigned>(material.material_slot),
                    static_cast<unsigned long long>(ctx_key),
                    static_cast<unsigned long long>(b0_key),
                    static_cast<unsigned long long>(source.bank_signature_a),
                    source.row_id_a,source.beta,
                    info.hook_active?1u:0u,
                    pmetal_native_map_cut::global_observer().hook_count(),
                    info.ever_watched?1u:0u,
                    static_cast<unsigned long long>(info.map_count),
                    static_cast<unsigned long long>(info.unmap_count),
                    static_cast<unsigned long long>(info.write_unmap_count),
                    static_cast<unsigned long long>(info.last_map_ctx),
                    static_cast<unsigned long long>(info.last_unmap_ctx),
                    info.last_map_type,info.last_map_tid,info.last_unmap_tid,
                    info.source_context_match?1u:0u,
                    info.complete_map_unmap?1u:0u,
                    static_cast<unsigned long long>(writer.epoch),
                    static_cast<unsigned>(writer.method),writer.complete?1u:0u);
                reshade::log::message(reshade::log::level::info,line);
            }
            bound_b0->Release();
        }
    }
#endif
#if defined(DSRRL_PMETAL_ASYLUM_DEFERRED_TOPOLOGY_TRACE)
    // Source candidate in this receiver's recording context; not GPU draw authority.
    const std::uint32_t sha_prefix =
        (std::uint32_t(material.flver_sha256[0]) << 24u) |
        (std::uint32_t(material.flver_sha256[1]) << 16u) |
        (std::uint32_t(material.flver_sha256[2]) << 8u) |
        std::uint32_t(material.flver_sha256[3]);
    dsrrl::runtime::pmetal_asylum_cmdlist_trace::global_observer()
        .observe_receiver_candidate(
            reinterpret_cast<std::uintptr_t>(cmd_list),
            static_cast<std::uintptr_t>(cmd_list->get_native()),
            decision.receiver_id, source.bank_signature_a,
            source.row_id_a, sha_prefix, material.material_slot);
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
#if !defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
    // Stable-only lineage: HemEnvLerp must fail open until the complete
    // two-endpoint PTDE EnvDiffuse island is compiled in.
    if (family !=
            pmetal_envspec_receiver_family::
                stable_hemenv) {
        telemetry::hot_count(
            blended_receiver_hold_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_blend);
        log_prepare_stage_once(
            1u << 5u,
            "full_ptde_stable_only_hold",
            material,
            decision,
            family);
        return false;
    }
#else
    // R19 admits only the two exact P_Metal receiver families. Unknown
    // families still fail open rather than inheriting the Lerp bridge.
    if (family !=
            pmetal_envspec_receiver_family::
                stable_hemenv &&
        family !=
            pmetal_envspec_receiver_family::
                hemenvlerp) {
        telemetry::hot_count(
            blended_receiver_hold_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_blend);
        return false;
    }
#endif
#endif

    if (family ==
            pmetal_envspec_receiver_family::
                stable_hemenv &&
        source.beta != 0.0f) {
        telemetry::hot_count(blended_receiver_hold_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_blend);
        log_prepare_stage_once(
            1u << 5u,
            "blend_hold",
            material,
            decision,
            family);
        return false;
    }
    const bool exact_envdiffuse_source_required =
        family ==
            pmetal_envspec_receiver_family::
                stable_hemenv
#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
        || family ==
            pmetal_envspec_receiver_family::
                hemenvlerp
#endif
        ;

    if (!k_v13_preserve_stock_envdiffuse &&
        exact_envdiffuse_source_required &&
        !source.envdiffuse_linear_valid) {
        telemetry::hot_count(source_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_source);
        log_prepare_stage_once(
            1u << 12u,
            "envdiffuse_linear_source_reject",
            material,
            decision,
            family);
        return false;
    }
    effect_latch(effect_receiver_source_ready_);

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());

    if (context == nullptr) {
        telemetry::hot_count(source_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_context);
        log_prepare_stage_once(
            1u << 6u,
            "context_reject",
            material,
            decision,
            family);
        return false;
    }

    ID3D11PixelShader *shader = nullptr;
    core::operator_mask composed_owners = 0u;

    {
        std::lock_guard<std::mutex> lock(
            mutex_);

        if (family ==
            pmetal_envspec_receiver_family::
                hemenvlerp) {
            const auto found =
                lerp_replacements_.find(
                    decision.receiver_id);

            if (found ==
                    lerp_replacements_.end() ||
                found->second == nullptr) {
                ++lerp_replacement_register_fail_;
                effect_fail(
                    effect_fail_mask_,
                    k_effect_fail_replacement);
                log_prepare_stage_once(
                    1u << 7u,
                    "replacement_reject",
                    material,
                    decision,
                    family);
                return false;
            }

            shader = found->second;
            shader->AddRef();
        } else {
            const auto found =
                replacements_.find(
                    decision.receiver_id);

            if (found ==
                    replacements_.end() ||
                found->second.shader == nullptr) {
                ++replacement_register_fail_;
                effect_fail(
                    effect_fail_mask_,
                    k_effect_fail_replacement);
                log_prepare_stage_once(
                    1u << 7u,
                    "replacement_reject",
                    material,
                    decision,
                    family);
                return false;
            }

            shader = found->second.shader;
            composed_owners =
                found->second.composed_owners;
            shader->AddRef();
        }
    }

    effect_latch(effect_replacement_ready_);

    const bool probe_b_required =
        family ==
            pmetal_envspec_receiver_family::
                hemenvlerp ||
        source.beta != 0.0f;

    ID3D11ShaderResourceView *shadow_env[3]{};
    const bool shadow_env_ready =
        !k_native_dsr_cubemap_feed &&
        pixel_srv_shadow_snapshot(
            cmd_list,
            12u,
            probe_b_required ? 3u : 1u,
            shadow_env);

    const bool env_resource_ready =
        k_native_dsr_cubemap_feed
            ? env_resources_.prepare_native_dsr(
                  context,
                  env_semantics.envspc_slot,
                  probe_b_required,
                  prepared.env_resources)
            : shadow_env_ready
                ? env_resources_.prepare_bound(
                      shadow_env[0],
                      probe_b_required
                          ? shadow_env[2]
                          : nullptr,
                      env_semantics.envspc_slot,
                      probe_b_required,
                      prepared.env_resources)
                : env_resources_.prepare(
                      context,
                      env_semantics.envspc_slot,
                      probe_b_required,
                      prepared.env_resources);

    if (!env_resource_ready) {
        if (shader != nullptr)
            shader->Release();
        telemetry::hot_count(probe_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_probe);
        log_prepare_stage_once(
            1u << 8u,
            "probe_reject",
            material,
            decision,
            family);
        return false;
    }
    effect_latch(effect_probe_ready_);

    if (!g_resource_mode_logged.exchange(
            true,
            std::memory_order_relaxed)) {
        char line[384]{};
        std::snprintf(
            line,
            sizeof(line),
            k_v13_native_dsr_material_mod_diag && !k_native_dsr_cubemap_feed
                ? "[DSRRL PMETAL V13 MATERIAL MOD] mode=ptde_packedgi_rgba_decode_ptde_ab_beta specrgb_c101_color0=envspec_only envdiffuse=stock lerp=paired slot=%u probe_a=%u probe_b=%u"
                : k_v13_native_dsr_material_mod_diag
                    ? "[DSRRL PMETAL V13 MATERIAL MOD] mode=native_dsr_bc6h_ptde_ab_beta specrgb_c101_color0=envspec_only envdiffuse=stock lerp=paired slot=%u probe_a=%u probe_b=%u"
                : k_v13_native_dsr_no_tail_diag
                    ? "[DSRRL PMETAL V13 NO TAIL] mode=native_dsr_bc6h_ptde_ab_beta no_specrgb_tail=1 envdiffuse=stock lerp=paired slot=%u probe_a=%u probe_b=%u"
                    : k_native_dsr_cubemap_feed
                        ? "[DSRRL PMETAL ENVSPEC RESOURCE] mode=native_dsr_bc6h_ptde_operator sampler=ptde_lod0 slot=%u probe_a=%u probe_b=%u"
                        : "[DSRRL PMETAL ENVSPEC RESOURCE] mode=ptde_packedgi_rgba sampler=ptde_lod0 slot=%u probe_a=%u probe_b=%u",
            static_cast<unsigned>(
                env_semantics.envspc_slot),
            static_cast<unsigned>(
                prepared.env_resources.probe_a),
            static_cast<unsigned>(
                prepared.env_resources.probe_b));
        reshade::log::message(
            reshade::log::level::info,
            line);
    }

#if !defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG)
    ID3D11ShaderResourceView *shadow_material[3]{};
    const bool shadow_material_ready =
        pixel_srv_shadow_snapshot(
            cmd_list,
            0u,
            3u,
            shadow_material);

    if (shadow_env_ready &&
        shadow_material_ready)
        telemetry::hot_count(
            srv_shadow_hits_);
    else
        telemetry::hot_count(
            srv_shadow_fallbacks_);

    bool material_ready =
        shadow_material_ready
            ? material_resources_.
                  prepare_draw_requests_bound(
                      shadow_material,
                      decision.receiver_id,
                      query,
                      true,
                      true,
                      prepared.material_resources)
            : material_resources_.
                  prepare_draw_requests(
                      context,
                      decision.receiver_id,
                      query,
                      true,
                      true,
                      prepared.material_resources);

    // Shadow is only a fast carrier; exact live D3D11 stock texture binding
    // is authoritative. Recheck only on a failed shadow-sourced request.
    bool shadow_diverged = false;
#if !defined(DSRRL_RELEASE_CLEANUP)
    bool live_retry_recovered = false;
#endif
    if (shadow_material_ready &&
        (!material_ready || !prepared.material_resources.spec_rgb)) {
        ID3D11ShaderResourceView *live[3]{};
        context->PSGetShaderResources(0u, 3u, live);
        shadow_diverged =
            live[0] != shadow_material[0] ||
            live[1] != shadow_material[1] ||
            live[2] != shadow_material[2];

        if (shadow_diverged) {
            material_resources_.release_prepared_draw(
                prepared.material_resources);
            material_ready = material_resources_.prepare_draw_requests_bound(
                live, decision.receiver_id, query, true, true,
                prepared.material_resources);
#if !defined(DSRRL_RELEASE_CLEANUP)
            live_retry_recovered =
                material_ready && prepared.material_resources.spec_rgb;
            if (live_retry_recovered) {
                static std::atomic_bool recovery_logged{false};
                if (!recovery_logged.exchange(
                        true, std::memory_order_relaxed)) {
                    reshade::log::message(
                        reshade::log::level::info,
                        "[DSRRL PMETAL SPEC GATE] live_t1_shadow_recovered=1 exact_material_and_sidecar=PASS");
                }
            }
#endif
        }
        for (auto *&view : live)
            if (view != nullptr) { view->Release(); view = nullptr; }
    }

#if defined(DSRRL_PMETAL_SPEC_CUT_TRACE)
    // Receiver-first diagnostic of the same FLVER material slot across
    // successive views. Samples 200 ms, but emits immediately on changes
    // to pass/fail, probe A/B or selector beta. Read-only, never gate authority.
    auto trace_spec_cut = [&](bool spec_ok) noexcept {
        struct state {
            std::uint64_t owner_key = 0;
            std::uint64_t last_ms = 0;
            std::uint32_t beta_bits = 0;
            std::uint16_t probe_a = 0;
            std::uint16_t probe_b = 0;
            std::uint32_t row_a = 0;
            std::uint32_t row_b = 0;
            std::uint8_t origin = 0;
            std::uint8_t stage = 0;
#if defined(DSRRL_PMETAL_ASYLUM_NATIVE_PASS_BINDING_TRACE)
            std::uint64_t native_source_bank = 0u;
#endif
        };
        // Increased sets + slot-inclusive FNV hash prevent unrelated
        // materials from evicting each other each frame. Hard cap remains.
        static thread_local std::array<state,512u> previous{};
        static std::atomic<std::uint32_t> emitted{0u};
        constexpr std::uint32_t max_lines = 4096u;

        std::uint64_t owner_key = 14695981039346656037ull;
        for (std::size_t i = 0; i < 16u; ++i)
            owner_key = (owner_key ^
                static_cast<std::uint64_t>(material.flver_sha256[i])) *
                1099511628211ull;
        // FNV mixing ensures material slot influences the low index bits.
        // Previously the <<16 slot never affected index &127 and caused
        // per-frame cache conflict/log flooding among one FLVER's slots.
        owner_key = (owner_key ^
            static_cast<std::uint64_t>(material.material_slot)) *
            1099511628211ull;
        owner_key = (owner_key ^
            static_cast<std::uint64_t>(decision.receiver_id)) *
            1099511628211ull;
        owner_key = (owner_key ^
            static_cast<std::uint64_t>(
                static_cast<unsigned>(family))) *
            1099511628211ull;
        if (owner_key == 0u)
            owner_key = 1u;

        std::uint32_t beta_bits = 0u;
        static_assert(sizeof(beta_bits) == sizeof(source.beta));
        std::memcpy(&beta_bits, &source.beta, sizeof(beta_bits));
        const std::uint64_t tick = GetTickCount64();
        auto &entry = previous[static_cast<std::size_t>(
            (owner_key ^ (owner_key >> 32u) ^ (owner_key >> 17u)) &
            (previous.size() - 1u))];
        const std::uint8_t stage = spec_ok ? 2u : 1u;
#if defined(DSRRL_PMETAL_ASYLUM_NATIVE_PASS_BINDING_TRACE)
        const bool native_bank_changed =
            entry.owner_key != owner_key ||
            entry.native_source_bank != source.bank_signature_a;
#endif
        const bool changed = entry.owner_key != owner_key ||
            entry.stage != stage ||
            entry.probe_a != prepared.env_resources.probe_a ||
            entry.probe_b != prepared.env_resources.probe_b ||
            entry.row_a != source.row_id_a ||
            entry.row_b != source.row_id_b ||
            entry.origin != source.diagnostic_origin ||
            entry.beta_bits != beta_bits;
        if (!changed && tick - entry.last_ms < 500u)
            return;
        entry = {owner_key, tick, beta_bits,
            prepared.env_resources.probe_a,
            prepared.env_resources.probe_b,
            source.row_id_a, source.row_id_b,
            source.diagnostic_origin, stage
#if defined(DSRRL_PMETAL_ASYLUM_NATIVE_PASS_BINDING_TRACE)
            , source.bank_signature_a
#endif
        };

        if (emitted.fetch_add(1u, std::memory_order_relaxed) >=
            max_lines)
            return;

        // Native D3D state is read only after the sampling decision.
        // PSGetShaderResources AddRefs its outputs; release every view.
        ID3D11ShaderResourceView *env_live[3]{};
        context->PSGetShaderResources(12u, 3u, env_live);
        const auto spec =
            material_resources_.probe_exact_specular_companion(context);
        const auto view_a = reinterpret_cast<std::uintptr_t>(env_live[0]);
        const auto view_b = reinterpret_cast<std::uintptr_t>(env_live[2]);
        for (auto *&view : env_live)
            if (view != nullptr) { view->Release(); view = nullptr; }

        char line[1840]{};
        std::snprintf(
            line, sizeof(line),
            "[DSRRL PMETAL SPEC TRANSITION] ms=%llu state=%s "
            "rx=%u family=%u route=%u owner_sha0=%02x%02x%02x%02x "
            "slot=%u flver_owner=%016llx t1=%016llx "
            "registry=%u ambiguous=%u epoch=%llu snap=%u "
            "hash=%016llx allowed=%u companion=%u quarantine=%u "
            "prep=%u req=%u shadow=%u shadow_diverged=%u "
            "probeA=%u probeB=%u t12=%016llx t14=%016llx "
            "bankA=%016llx rowA=%u bankB=%016llx rowB=%u "
            "source_serial=%llu beta=%.9g c101=%.9g "
            "source_origin=%u srcA=%.8g,%.8g,%.8g "
            "envdiffuseA=%.8g,%.8g,%.8g",
            static_cast<unsigned long long>(tick),
            spec_ok ? "PTDE_PREPARED" : "STOCK_FAILOPEN",
            static_cast<unsigned>(decision.receiver_id),
            static_cast<unsigned>(family),
            static_cast<unsigned>(decision.route_index),
            static_cast<unsigned>(material.flver_sha256[0]),
            static_cast<unsigned>(material.flver_sha256[1]),
            static_cast<unsigned>(material.flver_sha256[2]),
            static_cast<unsigned>(material.flver_sha256[3]),
            static_cast<unsigned>(material.material_slot),
            static_cast<unsigned long long>(
                material.flver_identity_hash),
            static_cast<unsigned long long>(spec.stock_view_key),
            spec.registered_view ? 1u : 0u,
            spec.ambiguous_view ? 1u : 0u,
            static_cast<unsigned long long>(spec.view_epoch),
            spec.snapshot_resolved ? 1u : 0u,
            static_cast<unsigned long long>(spec.logical_hash),
            spec.logical_hash_allowed ? 1u : 0u,
            spec.companion_ready ? 1u : 0u,
            spec.quarantined ? 1u : 0u,
            material_ready ? 1u : 0u,
            prepared.material_resources.spec_rgb ? 1u : 0u,
            shadow_material_ready ? 1u : 0u,
            shadow_diverged ? 1u : 0u,
            static_cast<unsigned>(prepared.env_resources.probe_a),
            static_cast<unsigned>(prepared.env_resources.probe_b),
            static_cast<unsigned long long>(view_a),
            static_cast<unsigned long long>(view_b),
            static_cast<unsigned long long>(source.bank_signature_a),
            static_cast<unsigned>(source.row_id_a),
            static_cast<unsigned long long>(source.bank_signature_b),
            static_cast<unsigned>(source.row_id_b),
            static_cast<unsigned long long>(source.serial),
            static_cast<double>(source.beta),
            static_cast<double>(decision.c101),
            static_cast<unsigned>(source.diagnostic_origin),
            static_cast<double>(source.a[0]),
            static_cast<double>(source.a[1]),
            static_cast<double>(source.a[2]),
            static_cast<double>(source.envdiffuse_a[0]),
            static_cast<double>(source.envdiffuse_a[1]),
            static_cast<double>(source.envdiffuse_a[2]));
#if defined(DSRRL_PMETAL_ASYLUM_DRAW_SOURCE_JOIN_TRACE)
        const std::size_t n = std::strlen(line);
        if (n < sizeof(line))
            std::snprintf(line+n,sizeof(line)-n,
                " producer_rva=%x parent_rva=%x publisher_tid=%u consumer_tid=%u raw=%04x/%04x source_ptr=%llx/%llx parent_ok=%u shadow=%u join=EXACT_MATERIAL_ONLY",
                static_cast<unsigned>(source.producer_callsite_rva),
                static_cast<unsigned>(source.producer_parent_rva),
                static_cast<unsigned>(source.producer_thread_id),
                static_cast<unsigned>(GetCurrentThreadId()),
                static_cast<unsigned>(source.producer_raw_a),
                static_cast<unsigned>(source.producer_raw_b),
                static_cast<unsigned long long>(source.producer_source_a),
                static_cast<unsigned long long>(source.producer_source_b),
                source.producer_parent_verified ? 1u : 0u,
                source.producer_hook_shadow ? 1u : 0u);
#endif
        reshade::log::message(reshade::log::level::info, line);
#if defined(DSRRL_PMETAL_ASYLUM_NATIVE_PASS_BINDING_TRACE)
        // Read-only pre-transaction D3D state captured at the SAME sampled
        // RX33/RX34 receiver as the exact producer/material provenance.
        // A stable PS/CB binding is NOT evidence that its contents are equal.
        // No Map, GetData, CopyResource, flush, bind, or draw replay occurs.
        constexpr std::uint64_t k_m10_bank = 0x4c594553d201d80cULL;
        constexpr std::uint64_t k_m18_bank = 0x1ecfd1e617c59071ULL;
        static std::atomic<std::uint32_t> native_trace_count{0u};
        if (native_bank_changed &&
            (source.bank_signature_a == k_m10_bank ||
             source.bank_signature_a == k_m18_bank) &&
            native_trace_count.fetch_add(1u,
                std::memory_order_relaxed) < 1024u) {
            ID3D11PixelShader *live_ps = nullptr;
            context->PSGetShader(&live_ps,nullptr,nullptr);
            ID3D11Buffer *live_cbs[16]{};
            context->PSGetConstantBuffers(0u,16u,live_cbs);
#if defined(DSRRL_PMETAL_ASYLUM_CB_WRITER_TRACE)
            pmetal_native_cb_writer_stamp writer_stamps[2]{};
            std::uintptr_t watched_handles[2]{};
#endif

            ID3D11DeviceContext1 *ctx1 = nullptr;
            const bool cb1_available = SUCCEEDED(
                context->QueryInterface(
                    __uuidof(ID3D11DeviceContext1),
                    reinterpret_cast<void **>(&ctx1))) &&
                ctx1 != nullptr;
            ID3D11Buffer *window_cbs[16]{};
            UINT firsts[16]{};
            UINT counts[16]{};
            if (cb1_available) {
                ctx1->PSGetConstantBuffers1(
                    0u,16u,window_cbs,firsts,counts);
            }
            char native_line[1850]{};
            int pos=std::snprintf(
                native_line,sizeof(native_line),
                "[DSRRL PMETAL NATIVE PASS BINDINGS]"
                " ms=%llu rx=%u slot=%u owner_sha0=%02x%02x%02x%02x"
                " ctx=%016llx ctx_type=%u ps=%016llx"
                " bank=%016llx row=%u beta=%.7g"
                " producer_rva=%x producer_tid=%u consumer_tid=%u"
                " native_cb1=%u capture=PRE_ISLAND binding_only=1",
                static_cast<unsigned long long>(tick),
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(material.material_slot),
                static_cast<unsigned>(material.flver_sha256[0]),
                static_cast<unsigned>(material.flver_sha256[1]),
                static_cast<unsigned>(material.flver_sha256[2]),
                static_cast<unsigned>(material.flver_sha256[3]),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(context)),
                static_cast<unsigned>(context->GetType()),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(live_ps)),
                static_cast<unsigned long long>(source.bank_signature_a),
                static_cast<unsigned>(source.row_id_a),
                static_cast<double>(source.beta),
                static_cast<unsigned>(source.producer_callsite_rva),
                static_cast<unsigned>(source.producer_thread_id),
                static_cast<unsigned>(GetCurrentThreadId()),
                cb1_available?1u:0u);
            for (UINT i=0u;i<16u;++i) {
                D3D11_BUFFER_DESC desc{};
                if (live_cbs[i] != nullptr)
                    live_cbs[i]->GetDesc(&desc);
#if defined(DSRRL_PMETAL_ASYLUM_CB_WRITER_TRACE)
                if (i < 2u && live_cbs[i] != nullptr) {
                    const auto native_buffer =
                        reinterpret_cast<std::uintptr_t>(live_cbs[i]);
                    watched_handles[i]=native_buffer;
                    pmetal_native_cb_writer_watch(
                        native_buffer,desc.ByteWidth);
                    writer_stamps[i]=pmetal_native_cb_writer_lookup(
                        native_buffer);
                }
#endif
                if (pos > 0 &&
                    static_cast<std::size_t>(pos) < sizeof(native_line)) {
                    pos+=std::snprintf(
                        native_line+pos,sizeof(native_line)-
                            static_cast<std::size_t>(pos),
                        " b%u=%llx:%u:%u:%u:%u",
                        static_cast<unsigned>(i),
                        static_cast<unsigned long long>(
                            reinterpret_cast<std::uintptr_t>(live_cbs[i])),
                        static_cast<unsigned>(desc.ByteWidth),
                        static_cast<unsigned>(cb1_available?firsts[i]:0u),
                        static_cast<unsigned>(cb1_available?counts[i]:0u),
                        static_cast<unsigned>(cb1_available &&
                            live_cbs[i] == window_cbs[i]?1u:0u));
                }
                if (live_cbs[i] != nullptr)
                    live_cbs[i]->Release();
                if (window_cbs[i] != nullptr)
                    window_cbs[i]->Release();
            }
            if (ctx1 != nullptr)ctx1->Release();
            if (live_ps != nullptr)live_ps->Release();
            reshade::log::message(reshade::log::level::info,native_line);
#if defined(DSRRL_PMETAL_ASYLUM_CB_WRITER_TRACE)
            // Epoch zero means no CPU-side writer was observed since watch
            // activation; it is NOT proof of zero/unchanged CB contents.
            const auto &w0=writer_stamps[0];
            const auto &w1=writer_stamps[1];
            char writer_line[780]{};
            std::snprintf(writer_line,sizeof(writer_line),
                "[DSRRL PMETAL CB WRITER JOIN]"
                " ms=%llu rx=%u slot=%u owner_sha0=%02x%02x%02x%02x"
                " ctx=%llx ps=%llx bank=%016llx row=%u"
                " producer_rva=%x consumer_tid=%u"
                " b0=%llx w0_epoch=%llu w0_hash=%016llx w0_tid=%u"
                " w0_method=%u w0_bytes=%u w0_complete=%u w0_age_ms=%llu"
                " b1=%llx w1_epoch=%llu w1_hash=%016llx w1_tid=%u"
                " w1_method=%u w1_bytes=%u w1_complete=%u w1_age_ms=%llu",
                static_cast<unsigned long long>(tick),
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(material.material_slot),
                static_cast<unsigned>(material.flver_sha256[0]),
                static_cast<unsigned>(material.flver_sha256[1]),
                static_cast<unsigned>(material.flver_sha256[2]),
                static_cast<unsigned>(material.flver_sha256[3]),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(context)),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(live_ps)),
                static_cast<unsigned long long>(source.bank_signature_a),
                static_cast<unsigned>(source.row_id_a),
                static_cast<unsigned>(source.producer_callsite_rva),
                static_cast<unsigned>(GetCurrentThreadId()),
                static_cast<unsigned long long>(watched_handles[0]),
                static_cast<unsigned long long>(w0.epoch),
                static_cast<unsigned long long>(w0.hash),
                static_cast<unsigned>(w0.writer_tid),
                static_cast<unsigned>(w0.method),
                static_cast<unsigned>(w0.byte_count),
                w0.complete?1u:0u,
                static_cast<unsigned long long>(
                    w0.epoch!=0u && tick>=w0.timestamp_ms?
                    tick-w0.timestamp_ms:0u),
                static_cast<unsigned long long>(watched_handles[1]),
                static_cast<unsigned long long>(w1.epoch),
                static_cast<unsigned long long>(w1.hash),
                static_cast<unsigned>(w1.writer_tid),
                static_cast<unsigned>(w1.method),
                static_cast<unsigned>(w1.byte_count),
                w1.complete?1u:0u,
                static_cast<unsigned long long>(
                    w1.epoch!=0u && tick>=w1.timestamp_ms?
                    tick-w1.timestamp_ms:0u));
            reshade::log::message(
                reshade::log::level::info,writer_line);
#if defined(DSRRL_PMETAL_ASYLUM_CB_REGISTER_TRACE)
            // Discrete, bounded 16-byte CPU-upload register *fingerprints*.
            // These hash positions are NOT an attested shader ABI and
            // contain no raw GPU data or arbitrary memory reads.
            static std::atomic<std::uint32_t> m10_register_samples{0u};
            static std::atomic<std::uint32_t> m18_register_samples{0u};
            static std::atomic<std::uint32_t> register_sample_id{0u};
            auto &bank_samples =
                source.bank_signature_a == k_m10_bank
                    ? m10_register_samples : m18_register_samples;
            if (w0.complete && w0.register_count == 129u &&
                w0.epoch != 0u &&
                bank_samples.fetch_add(1u,std::memory_order_relaxed)
                    < 160u) {
                const auto sample_id =
                    register_sample_id.fetch_add(
                        1u,std::memory_order_relaxed)+1u;
                for (std::uint32_t part=0u;part<9u;++part) {
                    const auto first=part*16u;
                    const auto last=(first+16u<129u?first+16u:129u);
                    char hash_line[850]{};
                    int written=std::snprintf(
                        hash_line,sizeof(hash_line),
                        "[DSRRL PMETAL B0 REGHASH]"
                        " sample=%u part=%u first=%u end_exclusive=%u"
                        " ms=%llu rx=%u slot=%u"
                        " owner_sha0=%02x%02x%02x%02x"
                        " bank=%016llx row=%u producer_rva=%x"
                        " ctx=%llx ps=%llx b0=%llx"
                        " epoch=%llu whole=%016llx"
                        " count=129 domain=CPU_MAP_WRITE_ONLY"
                        " hashes=",
                        static_cast<unsigned>(sample_id),
                        static_cast<unsigned>(part),
                        static_cast<unsigned>(first),
                        static_cast<unsigned>(last),
                        static_cast<unsigned long long>(tick),
                        static_cast<unsigned>(decision.receiver_id),
                        static_cast<unsigned>(material.material_slot),
                        static_cast<unsigned>(material.flver_sha256[0]),
                        static_cast<unsigned>(material.flver_sha256[1]),
                        static_cast<unsigned>(material.flver_sha256[2]),
                        static_cast<unsigned>(material.flver_sha256[3]),
                        static_cast<unsigned long long>(
                            source.bank_signature_a),
                        static_cast<unsigned>(source.row_id_a),
                        static_cast<unsigned>(source.producer_callsite_rva),
                        static_cast<unsigned long long>(
                            reinterpret_cast<std::uintptr_t>(context)),
                        static_cast<unsigned long long>(
                            reinterpret_cast<std::uintptr_t>(live_ps)),
                        static_cast<unsigned long long>(watched_handles[0]),
                        static_cast<unsigned long long>(w0.epoch),
                        static_cast<unsigned long long>(w0.hash));
                    for (auto i=first;i<last &&
                        written > 0 &&
                        static_cast<std::size_t>(written)+18u
                            < sizeof(hash_line);++i) {
                        written+=std::snprintf(
                            hash_line+written,
                            sizeof(hash_line)-
                                static_cast<std::size_t>(written),
                            "%s%016llx",
                            i==first?"":",",
                            static_cast<unsigned long long>(
                                w0.register_hashes[i]));
                    }
                    reshade::log::message(
                        reshade::log::level::info,hash_line);
                }
            }
#endif
#endif
        }
#endif
    };
#endif

    if (!material_ready ||
        !prepared.material_resources.spec_rgb) {
#if defined(DSRRL_PMETAL_SPEC_CUT_TRACE)
        trace_spec_cut(false);
#endif
#if !defined(DSRRL_RELEASE_CLEANUP)
        static std::atomic_bool spec_frontier_logged{false};
        if (!spec_frontier_logged.exchange(
                true, std::memory_order_relaxed)) {
            const auto probe =
                material_resources_.probe_exact_specular_companion(context);
            const auto semantic = mr::classify_mtd_semantic(
                query, mr::mtd_semantic_operator::spec_rgb);
            char line[640]{};
            std::snprintf(
                line, sizeof(line),
                "[DSRRL PMETAL SPEC GATE] rejected rx=%u ready=%u req=%u shadow=%u diverged=%u recovered=%u t1=%u cache=%u hash=%016llx allowed=%u companion=%u semantic=%u owner=%u",
                static_cast<unsigned>(decision.receiver_id),
                material_ready ? 1u : 0u,
                prepared.material_resources.spec_rgb ? 1u : 0u,
                shadow_material_ready ? 1u : 0u,
                shadow_diverged ? 1u : 0u,
                live_retry_recovered ? 1u : 0u,
                probe.stock_bound ? 1u : 0u,
                probe.snapshot_resolved ? 1u : 0u,
                static_cast<unsigned long long>(probe.logical_hash),
                probe.logical_hash_allowed ? 1u : 0u,
                probe.companion_ready ? 1u : 0u,
                static_cast<unsigned>(semantic.state),
                material.owner_tuple_exact ? 1u : 0u);
            reshade::log::message(reshade::log::level::info, line);
        }
#endif

        if (shader != nullptr)
            shader->Release();
        env_resources_.release(
            prepared.env_resources);
        material_resources_.
            release_prepared_draw(
                prepared.material_resources);
        telemetry::hot_count(spec_rgb_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_spec_rgb);
        log_prepare_stage_once(
            1u << 9u,
            "spec_rgb_reject",
            material,
            decision,
            family);
        return false;
    }
#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
    if (!material_resources_.keep_only_spec_rgb_request(
            prepared.material_resources)) {
        if (shader != nullptr)
            shader->Release();
        env_resources_.release(
            prepared.env_resources);
        material_resources_.
            release_prepared_draw(
                prepared.material_resources);
        telemetry::hot_count(spec_rgb_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_spec_rgb);
        log_prepare_stage_once(
            1u << 9u,
            "spec_rgb_filter_reject",
            material,
            decision,
            family);
        return false;
    }
#endif
    effect_latch(effect_spec_rgb_ready_);
#endif

    const bool stable_envdiffuse_consumer_diag =
#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) || defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
        false;
#else
        family ==
            pmetal_envspec_receiver_family::
                stable_hemenv;
#endif

    std::array<float,3> carrier3{{
        source.b[0],
        source.b[1],
        source.b[2]
    }};
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    if (stable_envdiffuse_consumer_diag)
        carrier3 = source.envdiffuse_a;
#else
    if (stable_envdiffuse_consumer_diag)
        carrier3 = {{0.0f,0.0f,0.0f}};
#endif

#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
    const std::array<f4,6> payload{{
#else
    const std::array<f4,4> payload{{
#endif
        {
            decision.c101,
            decision.c101,
            decision.c101,
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
            source.phn_k135_valid &&
                    std::isfinite(source.phn_k135) &&
                    source.phn_k135 > 0.0f
                ? source.phn_k135
                : 1.0f
#else
            1.0f
#endif
        },
        {
            decision.c100[0],
            decision.c100[1],
            decision.c100[2],
            1.0f
        },
        {
            source.a[0],
            source.a[1],
            source.a[2],
            0.0f
        },
        {
            carrier3[0],
            carrier3[1],
            carrier3[2],
            source.beta
        }
#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
        ,
        {
            source.envdiffuse_a[0],
            source.envdiffuse_a[1],
            source.envdiffuse_a[2],
            0.0f
        },
        {
            source.envdiffuse_b[0],
            source.envdiffuse_b[1],
            source.envdiffuse_b[2],
            source.beta
        }
#endif
    }};
#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
    static_assert(sizeof(payload) == 96u);
#else
    static_assert(sizeof(payload) == 64u);
#endif

    ID3D11Device *device = nullptr;
    context->GetDevice(&device);

    if (device == nullptr) {
        if (shader != nullptr)
            shader->Release();
        env_resources_.release(
            prepared.env_resources);
        material_resources_.
            release_prepared_draw(
                prepared.material_resources);
        telemetry::hot_count(source_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_device);
        return false;
    }

#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
    std::array<std::uint32_t,24> payload_bits{};
#else
    std::array<std::uint32_t,16> payload_bits{};
#endif
    static_assert(
        sizeof(payload_bits) == sizeof(payload));
    std::memcpy(
        payload_bits.data(),
        payload.data(),
        sizeof(payload));

    ID3D11Buffer *b12 = nullptr;
    bool upload_required = true;
    const auto b12_key =
        reinterpret_cast<std::uintptr_t>(
            context);

    {
        std::lock_guard<std::mutex> lock(
            mutex_);

        if (device_ != device) {
            device->Release();

            if (shader != nullptr)
                shader->Release();
            env_resources_.release(
                prepared.env_resources);
            material_resources_.
                release_prepared_draw(
                    prepared.material_resources);

            quarantined_.store(true);
            effect_fail(
                effect_fail_mask_,
                k_effect_fail_device);
            return false;
        }

        const auto found =
            b12_by_context_.find(
                b12_key);

        if (found !=
                b12_by_context_.end() &&
            found->second.buffer != nullptr) {
            b12 =
                found->second.buffer;
            b12->AddRef();
            upload_required =
                !found->second.payload_valid ||
                found->second.payload_bits !=
                    payload_bits;
        } else {
            D3D11_BUFFER_DESC desc{};
#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
            desc.ByteWidth = 96u;
#else
            desc.ByteWidth = 64u;
#endif
            desc.Usage =
                D3D11_USAGE_DYNAMIC;
            desc.BindFlags =
                D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags =
                D3D11_CPU_ACCESS_WRITE;

            if (FAILED(
                    device_->CreateBuffer(
                        &desc,
                        nullptr,
                        &b12)) ||
                b12 == nullptr) {
                device->Release();

                if (shader != nullptr)
                    shader->Release();
                env_resources_.release(
                    prepared.env_resources);
                material_resources_.
                    release_prepared_draw(
                        prepared.material_resources);
                telemetry::hot_count(source_rejects_);
                effect_fail(
                    effect_fail_mask_,
                    k_effect_fail_b12);
                return false;
            }

            b12_context_cache entry{};
            entry.buffer = b12;
            b12_by_context_.emplace(
                b12_key,
                entry);
            b12->AddRef();
        }
    }

    device->Release();

    if (upload_required) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(
                context->Map(
                    b12,
                    0u,
                    D3D11_MAP_WRITE_DISCARD,
                    0u,
                    &mapped)) ||
            mapped.pData == nullptr) {
            b12->Release();

            if (shader != nullptr)
                shader->Release();
            env_resources_.release(
                prepared.env_resources);
            material_resources_.
                release_prepared_draw(
                    prepared.material_resources);
            telemetry::hot_count(source_rejects_);
            effect_fail(
                effect_fail_mask_,
                k_effect_fail_b12);
            return false;
        }

        std::memcpy(
            mapped.pData,
            payload.data(),
            sizeof(payload));

        context->Unmap(
            b12,
            0u);

        {
            std::lock_guard<std::mutex> lock(
                mutex_);
            const auto found =
                b12_by_context_.find(
                    b12_key);
            if (found !=
                    b12_by_context_.end() &&
                found->second.buffer == b12) {
                found->second.payload_bits =
                    payload_bits;
                found->second.payload_valid = true;
            }
        }

        telemetry::hot_count(
            b12_uploads_);
    } else {
        telemetry::hot_count(
            b12_reuses_);
    }

    effect_latch(effect_b12_ready_);

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    // Probe identity is inherited from the already-authenticated EnvSpec
    // t12/t14 pair. The exact PTDE EnvDiffuse pack uses the same canonical
    // probe ordinal and never derives assignment from DSR t11 content.
    const bool ptde_envdiffuse_b_required =
#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
        family ==
            pmetal_envspec_receiver_family::
                hemenvlerp;
#else
        false;
#endif

    if (!env_resources_.prepare_envdiffuse(
            prepared.env_resources.probe_a,
            prepared.env_resources.probe_b,
            ptde_envdiffuse_b_required,
            prepared.envdiffuse_resources)) {
        b12->Release();
        if (shader != nullptr)
            shader->Release();
        env_resources_.release(
            prepared.env_resources);
        material_resources_.
            release_prepared_draw(
                prepared.material_resources);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_native_envdiffuse);
        log_prepare_stage_once(
            1u << 12u,
            "ptde_envdiffuse_resource_reject",
            material,
            decision,
            family);
        return false;
    }

    if (!g_native_envdiffuse_logged.exchange(
            true,
            std::memory_order_relaxed)) {
        char line[512]{};
#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
        const char *envdiffuse_mode =
            family ==
                pmetal_envspec_receiver_family::
                    hemenvlerp
                ? "[DSRRL PMETAL R19 LERP] mode=exact_ptde_envdiffuse_ab_beta probe_a=%u t11=%016llx endpoint_a=%.9g,%.9g,%.9g"
                : "[DSRRL PMETAL FULL PTDE HEMENV] mode=stable_exact_envdiffuse_rgba_div probe=%u t11=%016llx endpoint=%.9g,%.9g,%.9g";
#else
        constexpr const char *envdiffuse_mode =
            "[DSRRL PMETAL FULL PTDE HEMENV] mode=stable_exact_envdiffuse_rgba_div probe=%u t11=%016llx endpoint=%.9g,%.9g,%.9g";
#endif
        std::snprintf(
            line,
            sizeof(line),
            envdiffuse_mode,
            static_cast<unsigned>(
                prepared.envdiffuse_resources.probe_a),
            static_cast<unsigned long long>(
                reinterpret_cast<std::uintptr_t>(
                    prepared.envdiffuse_resources.ptde_a)),
            static_cast<double>(
                source.envdiffuse_a[0]),
            static_cast<double>(
                source.envdiffuse_a[1]),
            static_cast<double>(
                source.envdiffuse_a[2]));
        reshade::log::message(
            reshade::log::level::info,
            line);
    }
#elif !defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) && !defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
    // Legacy diagnostic ownership: explicitly rebind native DSR EnvDiffuse.
    ID3D11ShaderResourceView *native_envdiffuse_a = nullptr;
    ID3D11ShaderResourceView *native_envdiffuse_b = nullptr;
    context->PSGetShaderResources(
        11u,
        1u,
        &native_envdiffuse_a);

    const bool native_envdiffuse_b_required =
        family ==
            pmetal_envspec_receiver_family::
                hemenvlerp;
    if (native_envdiffuse_b_required) {
        context->PSGetShaderResources(
            13u,
            1u,
            &native_envdiffuse_b);
    }

    if (native_envdiffuse_a == nullptr ||
        (native_envdiffuse_b_required &&
         native_envdiffuse_b == nullptr)) {
        if (native_envdiffuse_a != nullptr)
            native_envdiffuse_a->Release();
        if (native_envdiffuse_b != nullptr)
            native_envdiffuse_b->Release();

        b12->Release();
        if (shader != nullptr)
            shader->Release();
        env_resources_.release(
            prepared.env_resources);
        material_resources_.
            release_prepared_draw(
                prepared.material_resources);

        effect_fail(
            effect_fail_mask_,
            k_effect_fail_native_envdiffuse);
        log_prepare_stage_once(
            1u << 12u,
            "native_envdiffuse_reject",
            material,
            decision,
            family);
        return false;
    }

    prepared.native_dsr_envdiffuse_a =
        native_envdiffuse_a;
    prepared.native_dsr_envdiffuse_b =
        native_envdiffuse_b;
#endif


    const auto env_owner =
        core::operator_bit(
            core::operator_id::
                env_spec);
    const auto mr_owner =
        core::operator_bit(
            core::operator_id::
                material_response);
    const auto domain_owner =
        core::operator_bit(
            core::operator_id::
                diffuse_material_domain);
    const auto spec_owner =
        core::operator_bit(
            core::operator_id::spec_rgb);
    const auto envdiff_owner =
        core::operator_bit(
            core::operator_id::env_diffuse);
    const auto sat_owner =
        core::operator_bit(
            core::operator_id::
                terminal_sat_rgb);
#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG)
    (void)spec_owner;
    (void)envdiff_owner;
    (void)sat_owner;
#elif defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
    (void)envdiff_owner;
    (void)sat_owner;
#endif
    prepared.shader = shader;
    prepared.b12 = b12;
    prepared.request.primary =
        core::operator_id::env_spec;

#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG)
    prepared.request.additional_owners =
        mr_owner |
        domain_owner |
        composed_owners;
#elif defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
    prepared.request.additional_owners =
        mr_owner |
        domain_owner |
        spec_owner |
        composed_owners;
#else
    prepared.request.additional_owners =
        mr_owner |
        domain_owner |
        spec_owner |
        envdiff_owner |
        composed_owners;

    if (family ==
        pmetal_envspec_receiver_family::
            hemenvlerp)
        prepared.request.additional_owners |=
            sat_owner;
#endif

    // This diagnostic owns the same narrow stable EnvDiffuse consumer cut as
    // PR197, but feeds b12[3].xyz = 0 on stable HemEnv. Because the replacement
    // shader consumes b12[3].xyz immediately before native t11/s11, this
    // removes only the local t11 EnvDiffuse term. EnvDiffuse therefore owns
    // shader + b12 + the explicit native t11/t13 resource rebind.
    prepared.request.additional_shader_owners =
        prepared.request.additional_owners;
#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) || defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
    prepared.request.additional_constant_buffer_owners =
        mr_owner;
    prepared.request.additional_resource_owners = 0u;
#else
    prepared.request.additional_constant_buffer_owners =
        mr_owner |
        envdiff_owner;
    prepared.request.additional_resource_owners =
        envdiff_owner;
#endif

    prepared.request.receiver_verified = true;
    prepared.request.material_verified = true;
    prepared.request.pixel_shader =
        shader;
    prepared.request.replace_pixel_shader =
        true;

    prepared.request.constant_buffers[0] = {
        12u,
        b12,
#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) || defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
        env_owner | mr_owner
#else
        env_owner | mr_owner | envdiff_owner
#endif
    };
    prepared.request.constant_buffer_count =
        1u;

    std::uint32_t request_srv_count = 0u;
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    prepared.request.srvs[
        request_srv_count++] = {
            11u,
            prepared.envdiffuse_resources.ptde_a
        };
#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
    if (family ==
            pmetal_envspec_receiver_family::
                hemenvlerp) {
        prepared.request.srvs[
            request_srv_count++] = {
                13u,
                prepared.envdiffuse_resources.ptde_b
            };
    }
#endif
    prepared.request.srvs[
        request_srv_count++] = {
            12u,
            prepared.env_resources.ptde_a
        };
    prepared.request.srvs[
        request_srv_count++] = {
            14u,
            prepared.env_resources.ptde_b
        };
#elif !defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) && !defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
    prepared.request.srvs[
        request_srv_count++] = {
            11u,
            prepared.native_dsr_envdiffuse_a
        };
    prepared.request.srvs[
        request_srv_count++] = {
            12u,
            prepared.env_resources.ptde_a
        };
    if (native_envdiffuse_b_required) {
        prepared.request.srvs[
            request_srv_count++] = {
                13u,
                prepared.native_dsr_envdiffuse_b
            };
    }
    prepared.request.srvs[
        request_srv_count++] = {
            14u,
            prepared.env_resources.ptde_b
        };
#else
    prepared.request.srvs[
        request_srv_count++] = {
            12u,
            prepared.env_resources.ptde_a
        };
    prepared.request.srvs[
        request_srv_count++] = {
            14u,
            prepared.env_resources.ptde_b
        };
#endif
    prepared.request.srv_count =
        request_srv_count;

    std::uint32_t request_sampler_count = 0u;

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW)
    const bool shadow_r7_draw =
        family ==
            pmetal_envspec_receiver_family::
                stable_hemenv &&
        (decision.receiver_id == 33u ||
         decision.receiver_id == 34u);

    if (shadow_r7_draw) {
        {
            std::lock_guard<std::mutex> lock(
                mutex_);
            if (shadow_sampler_ != nullptr) {
                shadow_sampler_->AddRef();
                prepared.shadow_sampler =
                    shadow_sampler_;
            }
        }

        if (prepared.shadow_sampler == nullptr) {
            release(prepared);
            effect_fail(
                effect_fail_mask_,
                k_effect_fail_device);
            log_prepare_stage_once(
                1u << 13u,
                "shadow_s7_reject",
                material,
                decision,
                family);
            return false;
        }

        prepared.request.samplers[
            request_sampler_count++] = {
                7u,
                prepared.shadow_sampler
            };

        if (!g_shadow_r7_logged.exchange(
                true,
                std::memory_order_relaxed)) {
            char line[384]{};
            std::snprintf(
                line,
                sizeof(line),
                "[DSRRL PMETAL R7 SHADOW] rx=%u route=%u s7=%016llx kernel=pcf16_packed24 point_border_white",
                static_cast<unsigned>(
                    decision.receiver_id),
                static_cast<unsigned>(
                    decision.route_index),
                static_cast<unsigned long long>(
                    reinterpret_cast<
                        std::uintptr_t>(
                            prepared.shadow_sampler)));
            reshade::log::message(
                reshade::log::level::info,
                line);
        }
    }
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    prepared.request.samplers[
        request_sampler_count++] = {
            11u,
            prepared.envdiffuse_resources.sampler
        };
#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
    if (family ==
            pmetal_envspec_receiver_family::
                hemenvlerp) {
        prepared.request.samplers[
            request_sampler_count++] = {
                13u,
                prepared.envdiffuse_resources.sampler
            };
    }
#endif
    prepared.request.samplers[
        request_sampler_count++] = {
            12u,
            prepared.env_resources.sampler
        };
    prepared.request.samplers[
        request_sampler_count++] = {
            14u,
            prepared.env_resources.sampler
        };
#else
    prepared.request.samplers[
        request_sampler_count++] = {
            12u,
            prepared.env_resources.sampler
        };
    prepared.request.samplers[
        request_sampler_count++] = {
            14u,
            prepared.env_resources.sampler
        };
#endif
    prepared.request.sampler_count =
        request_sampler_count;

    draw_tx_mutation verify{};
    if (build_island_draw_mutation(
            prepared.request,
            verify) !=
        island_draw_adapter_result::ready) {
        release(prepared);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_mutation);
        log_prepare_stage_once(
            1u << 10u,
            "mutation_reject",
            material,
            decision,
            family);
        return false;
    }

    prepared.ready = true;
    effect_latch(effect_request_ready_);
    telemetry::hot_count(requests_);

    if (stable_envdiffuse_consumer_diag &&
        !g_envdiffuse_consumer_logged.exchange(
            true,
            std::memory_order_relaxed)) {
        char line[512]{};
        std::snprintf(
            line,
            sizeof(line),
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
            "[DSRRL PMETAL FULL PTDE HEMENV] mode=ptde_t11_rgba_div_c86 family=%u rx=%u route=%u captured_pA=%.9g,%.9g,%.9g beta=%.9g t11=%016llx",
#else
            "[DSRRL PMETAL ENVDIFFUSE CONSUMER DIAG] mode=zero_t11_contribution family=%u rx=%u route=%u captured_pA=%.9g,%.9g,%.9g beta=%.9g t11=%016llx",
#endif
            static_cast<unsigned>(family),
            static_cast<unsigned>(decision.receiver_id),
            static_cast<unsigned>(decision.route_index),
            static_cast<double>(source.envdiffuse_a[0]),
            static_cast<double>(source.envdiffuse_a[1]),
            static_cast<double>(source.envdiffuse_a[2]),
            static_cast<double>(source.beta),
            static_cast<unsigned long long>(
                reinterpret_cast<std::uintptr_t>(
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
                    prepared.envdiffuse_resources.ptde_a
#else
                    prepared.native_dsr_envdiffuse_a
#endif
                    )));
        reshade::log::message(
            reshade::log::level::info,
            line);
    }
    log_prepare_stage_once(
        1u << 11u,
        "request_ready",
        material,
        decision,
        family);

#if !defined(DSRRL_RELEASE_CLEANUP)
    // Pixel-fail diagnostic for the owner-reported white P_Metal phenotype.
    // Log exactly once per stable receiver and only after the entire island is
    // request-ready, so every value below belongs to an actually executable
    // replacement draw. No GPU readback and no renderer-state mutation.
    if (decision.receiver_id >= 33u &&
        decision.receiver_id <= 35u) {
        const auto bit =
            1u << (decision.receiver_id - 33u);
        const auto observed =
            g_value_cut_log_mask.load(
                std::memory_order_relaxed);
        if ((observed & bit) == 0u &&
            (g_value_cut_log_mask.fetch_or(
                 bit,
                 std::memory_order_relaxed) & bit) == 0u) {
            const auto spec_probe =
                material_resources_.
                    probe_exact_specular_companion(
                        context);

            std::uintptr_t t10 = 0u;
            std::uint32_t t10_count = 0u;
            for (std::uint32_t i = 0u;
                 i < prepared.material_resources.request_count;
                 ++i) {
                const auto &resource_request =
                    prepared.material_resources.requests[i];
                for (std::uint32_t j = 0u;
                     j < resource_request.srv_count;
                     ++j) {
                    if (resource_request.srvs[j].slot != 10u ||
                        resource_request.srvs[j].srv == nullptr)
                        continue;
                    t10 =
                        reinterpret_cast<std::uintptr_t>(
                            resource_request.srvs[j].srv);
                    ++t10_count;
                }
            }

            char line[1536]{};
            std::snprintf(
                line,
                sizeof(line),
                "[DSRRL PMETAL VALUE CUT] family=%u rx=%u route=%u env_slot=%u probe_a=%u probe_b=%u bank_a=%016llx row_a=%u bank_b=%016llx row_b=%u serial=%llu gen=%llu beta=%.9g pA=%.9g,%.9g,%.9g pB=%.9g,%.9g,%.9g c101=%.9g c100=%.9g,%.9g,%.9g spec_hash=%016llx spec_allowed=%u spec_ready=%u spec_req=%u t10_count=%u t10=%016llx t11=%016llx t12=%016llx t13=%016llx t14=%016llx",
                static_cast<unsigned>(family),
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(decision.route_index),
                static_cast<unsigned>(env_semantics.envspc_slot),
                static_cast<unsigned>(prepared.env_resources.probe_a),
                static_cast<unsigned>(prepared.env_resources.probe_b),
                static_cast<unsigned long long>(source.bank_signature_a),
                static_cast<unsigned>(source.row_id_a),
                static_cast<unsigned long long>(source.bank_signature_b),
                static_cast<unsigned>(source.row_id_b),
                static_cast<unsigned long long>(source.serial),
                static_cast<unsigned long long>(source.generation),
                static_cast<double>(source.beta),
                static_cast<double>(source.a[0]),
                static_cast<double>(source.a[1]),
                static_cast<double>(source.a[2]),
                static_cast<double>(source.b[0]),
                static_cast<double>(source.b[1]),
                static_cast<double>(source.b[2]),
                static_cast<double>(decision.c101),
                static_cast<double>(decision.c100[0]),
                static_cast<double>(decision.c100[1]),
                static_cast<double>(decision.c100[2]),
                static_cast<unsigned long long>(spec_probe.logical_hash),
                spec_probe.logical_hash_allowed ? 1u : 0u,
                spec_probe.companion_ready ? 1u : 0u,
                prepared.material_resources.spec_rgb ? 1u : 0u,
                static_cast<unsigned>(t10_count),
                static_cast<unsigned long long>(t10),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(
                        prepared.native_dsr_envdiffuse_a)),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(
                        prepared.env_resources.ptde_a)),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(
                        prepared.native_dsr_envdiffuse_b)),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(
                        prepared.env_resources.ptde_b)));
            reshade::log::message(
                reshade::log::level::info,
                line);
        }
    }
#endif

#if defined(DSRRL_PMETAL_SPEC_CUT_TRACE)
    trace_spec_cut(true);
#endif
    if (family ==
        pmetal_envspec_receiver_family::
            hemenvlerp)
        telemetry::hot_count(lerp_requests_);
    return true;
}

void pmetal_envspec_draw_runtime::release(
    prepared_pmetal_envspec_draw &prepared) noexcept
{
    material_resources_.
        release_prepared_draw(
            prepared.material_resources);

    env_resources_.release(
        prepared.env_resources);
    env_resources_.release(
        prepared.envdiffuse_resources);

    if (prepared.b12 != nullptr)
        prepared.b12->Release();

    if (prepared.shader != nullptr)
        prepared.shader->Release();

    if (prepared.shadow_sampler != nullptr)
        prepared.shadow_sampler->Release();

    if (prepared.native_dsr_envdiffuse_a != nullptr)
        prepared.native_dsr_envdiffuse_a->Release();
    if (prepared.native_dsr_envdiffuse_b != nullptr)
        prepared.native_dsr_envdiffuse_b->Release();

    prepared = {};
}

pmetal_envspec_telemetry
pmetal_envspec_draw_runtime::telemetry() const noexcept
{
    return {
        replacement_register_ok_.load(),
        replacement_register_fail_.load(),
        lerp_replacement_register_ok_.load(),
        lerp_replacement_register_fail_.load(),
        candidates_.load(),
        lerp_candidates_.load(),
        material_rejects_.load(),
        semantic_rejects_.load(),
        source_rejects_.load(),
        blended_receiver_hold_.load(),
        probe_rejects_.load(),
        spec_rgb_rejects_.load(),
        requests_.load(),
        lerp_requests_.load(),
        b12_uploads_.load(),
        b12_reuses_.load(),
        srv_shadow_hits_.load(),
        srv_shadow_fallbacks_.load(),
        effect_entry_seen_.load(),
        effect_feature_ready_.load(),
        effect_material_ready_.load(),
        effect_semantic_ready_.load(),
        effect_source_ready_.load(),
        effect_receiver_source_ready_.load(),
        effect_replacement_ready_.load(),
        effect_probe_ready_.load(),
        effect_spec_rgb_ready_.load(),
        effect_b12_ready_.load(),
        effect_request_ready_.load(),
        effect_fail_mask_.load(),
        quarantined_.load()
    };
}

void pmetal_envspec_draw_runtime::reset() noexcept
{
    release_resources();

    replacement_register_ok_.store(0u);
    replacement_register_fail_.store(0u);
    lerp_replacement_register_ok_.store(0u);
    lerp_replacement_register_fail_.store(0u);
    candidates_.store(0u);
    lerp_candidates_.store(0u);
    material_rejects_.store(0u);
    semantic_rejects_.store(0u);
    source_rejects_.store(0u);
    blended_receiver_hold_.store(0u);
    probe_rejects_.store(0u);
    spec_rgb_rejects_.store(0u);
    requests_.store(0u);
    lerp_requests_.store(0u);
    b12_uploads_.store(0u);
    b12_reuses_.store(0u);
    srv_shadow_hits_.store(0u);
    srv_shadow_fallbacks_.store(0u);
    effect_entry_seen_.store(false);
    effect_feature_ready_.store(false);
    effect_material_ready_.store(false);
    effect_semantic_ready_.store(false);
    effect_source_ready_.store(false);
    effect_receiver_source_ready_.store(false);
    effect_replacement_ready_.store(false);
    effect_probe_ready_.store(false);
    effect_spec_rgb_ready_.store(false);
    effect_b12_ready_.store(false);
    effect_request_ready_.store(false);
    effect_fail_mask_.store(0u);
    g_source_cut_logged.store(false);
    g_resource_mode_logged.store(false);
    g_native_envdiffuse_logged.store(false);
    g_envdiffuse_consumer_logged.store(false);
    g_source_frontier_logged.store(false);
    g_shadow_r7_logged.store(false);
#if !defined(DSRRL_RELEASE_CLEANUP)
    g_prepare_stage_log_mask.store(0u);
    g_value_cut_log_mask.store(0u);
#endif
    quarantined_.store(false);
}

} // namespace dsrrl::runtime
