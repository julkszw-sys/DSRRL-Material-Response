#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include "dsrrl/runtime/pixel_srv_shadow.hpp"
#include "dsrrl/runtime/ptde_metal_envspec_authority.hpp"

#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <Windows.h>
#include <d3d11.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace dsrrl::runtime {
namespace {

namespace mr = operators::material_response;
namespace hashing = operators::legacy_plan::hashing;

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
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
// Explicit source-ready and request-ready frontiers for each newly admitted
// MTD. Does NOT label a prepared draw as executed or as PTDE pixel PASS.
constexpr std::uint32_t k_experimental_spc_count = static_cast<std::uint32_t>(ptde_metal_envspec_profile::spc_route_359) - static_cast<std::uint32_t>(ptde_metal_envspec_profile::pmetal_alp) + 1u;
static_assert(k_experimental_spc_count <= 32u);
std::atomic<std::uint32_t> g_other_metal_stage_log_masks[2]{};

void log_other_metal_stage_once(
    const char *stage,
    std::uint32_t stage_ordinal,
    const mr::material_identity &material,
    const mr::decision &decision) noexcept
{
    const auto *identity = match_ptde_metal_envspec_material(material);
    if (identity == nullptr ||
        identity->profile == ptde_metal_envspec_profile::pmetal_baseline ||
        stage_ordinal > 1u)
        return;
    const auto index =
        static_cast<std::uint32_t>(identity->profile) -
        static_cast<std::uint32_t>(ptde_metal_envspec_profile::pmetal_alp);
    if (index >= k_experimental_spc_count)
        return;
    const auto bit = 1u << index;
    if ((g_other_metal_stage_log_masks[stage_ordinal].fetch_or(
             bit, std::memory_order_relaxed) & bit) != 0u)
        return;
    char message[320]{};
    std::snprintf(
        message, sizeof(message),
        "[DSRRL OTHER METAL PTDE] stage=%s profile=%u route=%u rx=%u slot=%u source_key=EXACT_MATERIAL no_unkeyed_hook_fallback=1 pixel=OPEN",
        stage,
        static_cast<unsigned>(identity->profile),
        static_cast<unsigned>(decision.route_index),
        static_cast<unsigned>(decision.receiver_id),
        static_cast<unsigned>(material.material_slot));
    reshade::log::message(reshade::log::level::info, message);
}
#endif
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
// This is a diagnostic-only receiver-stage probe, not a shader/CB/SRV
// override. It survives RELEASE_CLEANUP=ON and is bounded to one
// fail-open line per exact metal profile and downstream stage.
std::atomic<std::uint64_t> g_other_metal_downstream_reject_mask{0u};
class other_metal_downstream_gate final {
public:
    other_metal_downstream_gate(
        const mr::material_identity &material,
        const mr::decision &decision,
        bool experimental) noexcept
        : material_(material),
          decision_(decision),
          experimental_(experimental) {}

    other_metal_downstream_gate(
        const other_metal_downstream_gate &) = delete;
    other_metal_downstream_gate &operator=(
        const other_metal_downstream_gate &) = delete;

    ~other_metal_downstream_gate() noexcept
    {
        if (!experimental_ || completed_ || gate_ >= 16u)
            return;
        const auto *authority =
            match_ptde_metal_envspec_material(material_);
        if (authority == nullptr ||
            authority->profile == ptde_metal_envspec_profile::pmetal_baseline)
            return;
        const std::uint32_t profile_index =
            static_cast<std::uint32_t>(authority->profile) -
            static_cast<std::uint32_t>(
                ptde_metal_envspec_profile::pmetal_alp);
        if (profile_index >= 3u)
            return;
        const std::uint64_t bit =
            std::uint64_t{1u} << (profile_index * 16u + gate_);
        if ((g_other_metal_downstream_reject_mask.fetch_or(
                 bit, std::memory_order_relaxed) & bit) != 0u)
            return;
        char line[400]{};
        std::snprintf(
            line, sizeof(line),
            "[DSRRL OTHER METAL PTDE] stage=downstream_fail_open gate=%s profile=%u route=%u rx=%u slot=%u source_ready=1 request_ready=0 pixel=OPEN",
            gate_name_,
            static_cast<unsigned>(authority->profile),
            static_cast<unsigned>(decision_.route_index),
            static_cast<unsigned>(decision_.receiver_id),
            static_cast<unsigned>(material_.material_slot));
        reshade::log::message(reshade::log::level::info, line);
    }

    void next(std::uint32_t gate, const char *name) noexcept
    {
        gate_ = gate;
        gate_name_ = name;
    }

    void complete() noexcept { completed_ = true; }
private:
    const mr::material_identity &material_;
    const mr::decision &decision_;
    bool experimental_ = false;
    bool completed_ = false;
    std::uint32_t gate_ = 0u;
    const char *gate_name_ = "receiver_source";
};
#endif
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

// Draw-local identity and compiled receiver are checked independently.
// The additional three MTDs have the exact PTDE/DSR SPC shader family
// and EnvSpec slot 2; no shared-shader create-time visible patch is used.
bool exact_metal_material_and_decision(
    const mr::material_identity &material,
    const mr::decision &decision) noexcept
{
    const auto *authority = match_ptde_metal_envspec_material(material);
    if (authority == nullptr ||
        !decision.active ||
        decision.route_index != authority->route_index ||
        decision.receiver_id < 33u ||
        decision.receiver_id > 35u ||
        !std::isfinite(decision.c101) ||
        decision.c101 < 0.0f)
        return false;

#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    // The experimental variants are an exact c101=2.5 subgroup.
    // The independent MTD semantic classifier and exact live t1+t10
    // resource/receiver gates are still mandatory in prepare().
    if (authority->profile != ptde_metal_envspec_profile::pmetal_baseline &&
        decision.c101 != authority->c101)
        return false;
#endif
    return true;
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
    return exact_metal_material_and_decision(material, decision);
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
        env_semantics.envspc_slot != match_ptde_metal_envspec_material(material)->envspc_slot) {
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
    // Never let an unkeyed latest-hook LightBank fallback authorize a
    // different material. Extended profiles require an exact
    // FLVER+slot+MTD-keyed selector publication, even if the original
    // P_Metal baseline can retain its historical validated fallback.
    const bool experimental_material =
        is_experimental_ptde_metal_envspec_material(material);
    const bool exact_source_ready = experimental_material
        ? source_.latest_exact_material(material, source)
        : source_.latest(material, source);
    if (!exact_source_ready || !std::isfinite(source.beta)) {
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
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    other_metal_downstream_gate downstream_gate(
        material, decision, experimental_material);
    if (experimental_material)
        log_other_metal_stage_once(
            "source_ready", 0u, material, decision);
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
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    downstream_gate.next(1u, "d3d_context");
#endif

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

#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    downstream_gate.next(2u, "replacement_shader");
#endif
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
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    downstream_gate.next(3u, "probe_resource");
#endif

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
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    downstream_gate.next(4u, "ptde_spec_rgb_sidecar");
#endif

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

    if (!material_ready ||
        !prepared.material_resources.spec_rgb) {
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
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    downstream_gate.next(5u, "b12_carrier");
#endif
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
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    downstream_gate.next(6u, "envdiffuse_resource");
#endif

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
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    downstream_gate.next(7u, "draw_mutation");
#endif

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
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    downstream_gate.complete();
#endif
    effect_latch(effect_request_ready_);
    telemetry::hot_count(requests_);
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    if (experimental_material)
        log_other_metal_stage_once(
            "request_ready", 1u, material, decision);
#endif

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
