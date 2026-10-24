#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"
#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)
#include "dsrrl/runtime/v203_equipment_envspec36_gate.hpp"
#include "dsrrl/runtime/v287_nonmetal_trace.hpp"
#endif
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include "dsrrl/runtime/pixel_srv_shadow.hpp"
#if defined(DSRRL_V336_WANDERER_SRV_AUDIT)
#include "dsrrl/runtime/v336_wanderer_srv_witness.hpp"
#endif

#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <Windows.h>
#include <d3d11.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

#if defined(DSRRL_V346_LERP_REJECT_STAGE)
namespace dsrrl::operators::env_spec {
std::uint32_t v346_lerp_reject_stage() noexcept;
}
#endif

namespace dsrrl::runtime {
namespace {

namespace mr = operators::material_response;
namespace hashing = operators::legacy_plan::hashing;

// The materialized PS is not an immutable file. Record its final DXBC
// digest AFTER the exact receiver/materializer and optional Lerp composition,
// once per successfully-created PS. This is a create-time identity witness;
// it neither changes shader bytes nor grants the draw any authority.
void log_final_pmetal_shader_digest(
    const char *family,
    std::uint32_t receiver,
    const hashing::sha256_digest &digest,
    std::size_t byte_count) noexcept
{
    constexpr char alphabet[] = "0123456789abcdef";
    char hex[65]{};
    for (std::size_t i = 0u; i < digest.size(); ++i) {
        hex[i * 2u] = alphabet[digest[i] >> 4u];
        hex[i * 2u + 1u] = alphabet[digest[i] & 0x0fu];
    }
    char line[256]{};
    std::snprintf(
        line, sizeof(line),
        "[DSRRL PS PAYLOAD AUTHORITY] family=%s rx=%u bytes=%zu final_dxbc_sha256=%s created=1 pixel_pass=0",
        family,
        static_cast<unsigned>(receiver),
        byte_count,
        hex);
    reshade::log::message(
        reshade::log::level::warning, line);
}

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

// Read-only exact-owner RX34 t11/s11/b12 witness. This is diagnostic
// resource-domain evidence, never authority to bind a PTDE probe or modify
// a shader. The live texture dimensions/format separate PackedGI candidates
// from legacy resources without inventing logical-name mappings.
std::atomic<std::uint32_t> g_rx34_t11_witness_source_ready{0u};
std::atomic<std::uint32_t> g_rx34_t11_witness_source_reject{0u};

void witness_rx34_native_envdiffuse(
    reshade::api::command_list *cmd_list,
    ID3D11DeviceContext *context,
    const pmetal_envspec_source &source,
    bool source_ready) noexcept
{
    auto &counter = source_ready ?
        g_rx34_t11_witness_source_ready :
        g_rx34_t11_witness_source_reject;
    auto sample = counter.load(std::memory_order_relaxed);
    if (sample >= 6u ||
        !counter.compare_exchange_strong(
            sample, sample + 1u, std::memory_order_relaxed))
        return;

    ID3D11ShaderResourceView *t11 = nullptr;
    ID3D11SamplerState *s11 = nullptr;
    ID3D11Buffer *b12 = nullptr;
    context->PSGetShaderResources(11u, 1u, &t11);
    context->PSGetSamplers(11u, 1u, &s11);
    context->PSGetConstantBuffers(12u, 1u, &b12);
    ID3D11ShaderResourceView *shadow_views[1]{};
    const bool shadow_ready =
        pixel_srv_shadow_snapshot(cmd_list, 11u, 1u, shadow_views);

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    D3D11_TEXTURE2D_DESC texture_desc{};
    D3D11_SAMPLER_DESC sampler_desc{};
    D3D11_BUFFER_DESC cb_desc{};
    bool texture2d = false;
    ID3D11Resource *resource = nullptr;
    if (t11 != nullptr) {
        t11->GetDesc(&srv_desc);
        t11->GetResource(&resource);
    }
    if (resource != nullptr) {
        ID3D11Texture2D *texture = nullptr;
        if (SUCCEEDED(resource->QueryInterface(
                __uuidof(ID3D11Texture2D),
                reinterpret_cast<void **>(&texture))) &&
            texture != nullptr) {
            texture->GetDesc(&texture_desc);
            texture2d = true;
            texture->Release();
        }
        resource->Release();
    }
    if (s11 != nullptr) {
        s11->GetDesc(&sampler_desc);
        s11->Release();
    }
    if (b12 != nullptr) {
        b12->GetDesc(&cb_desc);
        b12->Release();
    }

    char line[768]{};
    std::snprintf(
        line, sizeof(line),
        "[DSRRL RX34 T11 DOMAIN WITNESS] sample=%u route=345 source_gate=%u t11=%p shadow=%u shadow_t11=%p same=%u srv_fmt=%u srv_dim=%u tex2d=%u tex_fmt=%u width=%u height=%u array=%u mips=%u misc=%08x s11=%u sampler_filter=%u b12=%u cb_bytes=%u ptde_c86_donor=%u beta=%.9g",
        static_cast<unsigned>(sample + 1u),
        source_ready ? 1u : 0u,
        static_cast<void *>(t11),
        shadow_ready ? 1u : 0u,
        static_cast<void *>(shadow_views[0]),
        shadow_ready && shadow_views[0] == t11 ? 1u : 0u,
        static_cast<unsigned>(srv_desc.Format),
        static_cast<unsigned>(srv_desc.ViewDimension),
        texture2d ? 1u : 0u,
        static_cast<unsigned>(texture_desc.Format),
        texture_desc.Width,
        texture_desc.Height,
        texture_desc.ArraySize,
        texture_desc.MipLevels,
        texture_desc.MiscFlags,
        s11 != nullptr ? 1u : 0u,
        static_cast<unsigned>(sampler_desc.Filter),
        b12 != nullptr ? 1u : 0u,
        cb_desc.ByteWidth,
        source.envdiffuse_linear_valid ? 1u : 0u,
        static_cast<double>(source.beta));
    reshade::log::message(
        reshade::log::level::warning, line);
    if (t11 != nullptr)
        t11->Release();
}

std::atomic_bool g_source_cut_logged{false};
std::atomic_bool g_resource_mode_logged{false};
std::atomic_bool g_native_envdiffuse_logged{false};
std::atomic_bool g_envdiffuse_consumer_logged{false};
std::atomic_bool g_source_frontier_logged{false};
std::atomic_bool g_shadow_r7_logged{false};
std::atomic<std::uint32_t> g_prepare_stage_log_mask{0u};
std::atomic<std::uint32_t> g_value_cut_log_mask{0u};

void log_prepare_stage_once(
    std::uint32_t bit,
    const char *stage,
    const mr::material_identity &material,
    const mr::decision &decision,
    pmetal_envspec_receiver_family family) noexcept
{
#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)
    if (material.route_index != 345u) {
        // Previous global stage bit masked Lerp family1 after Stable family0
        // and also collapsed different FLVER/MTD failure sources. V287
        // records a bounded per-owner+receiver+family+stage observation.
        v287_nonmetal_trace::log(
            stage, material, decision, static_cast<std::uint32_t>(family));
        return;
    }
#endif
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
#if defined(DSRRL_V230_RUNTIME_TELEMETRY)
    // Fail-open cause: the normal V230 release strips INFO logs. Keep this
    // already-bounded exact-family stage witness, but no extra GPU queries.
    reshade::log::message(
        reshade::log::level::warning, line);
#else
    reshade::log::message(
        reshade::log::level::info, line);
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
    if (exact_pmetal_material(material) &&
        exact_pmetal_decision(decision))
        return true;
#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)
    // Experimental nonmetal island: independent exact FLVER/slot/MTD gate.
    // Never promote by shader family alone or use a generic MR/DSR PBL tail.
#if defined(DSRRL_V293_PARSER_EXACT_FLVER_ALIAS)
    return v203_equipment::exact_envspec36_draw(material, decision) ||
        v203_equipment::exact_spx_roughcloth_draw(material, decision);
#else
    return v203_equipment::exact_envspec36_draw(material, decision);
#endif
#else
    return false;
#endif
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
#if defined(DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB)
    for (auto &[_, shader] : lerp_nonmetal_r8_replacements_) {
        if (shader != nullptr) shader->Release();
    }
    lerp_nonmetal_r8_replacements_.clear();
#endif
    lerp_replacement_sha256_.clear();
    lerp_replacement_size_.clear();

    for (auto &[_,entry] :
         b12_by_context_) {
        if (entry.buffer != nullptr)
            entry.buffer->Release();
    }
    b12_by_context_.clear();
#if defined(DSRRL_V370_ISOLATED_EXACT_ENVDIFFUSE)
    for(auto &[_,shader] : independent_envdiffuse_shaders_)
        if(shader!=nullptr)shader->Release();
    independent_envdiffuse_shaders_.clear();
    for(auto &[_,cb] : independent_envdiffuse_cb_by_context_)
        if(cb.buffer!=nullptr)cb.buffer->Release();
    independent_envdiffuse_cb_by_context_.clear();
#endif

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
#if defined(DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB)
    for (auto &[_, shader] : lerp_nonmetal_r8_replacements_) {
        if (shader != nullptr) shader->Release();
    }
    lerp_nonmetal_r8_replacements_.clear();
#endif
    lerp_replacement_sha256_.clear();
    lerp_replacement_size_.clear();

    for (auto &[_,entry] :
         b12_by_context_) {
        if (entry.buffer != nullptr)
            entry.buffer->Release();
    }
    b12_by_context_.clear();
#if defined(DSRRL_V370_ISOLATED_EXACT_ENVDIFFUSE)
    for(auto &[_,shader] : independent_envdiffuse_shaders_)
        if(shader!=nullptr)shader->Release();
    independent_envdiffuse_shaders_.clear();
    for(auto &[_,cb] : independent_envdiffuse_cb_by_context_)
        if(cb.buffer!=nullptr)cb.buffer->Release();
    independent_envdiffuse_cb_by_context_.clear();
#endif

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
    log_final_pmetal_shader_digest(
        "StableHemEnv", outcome.receiver_id,
        payload_sha256, dxbc_size);

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

#if defined(DSRRL_V345_UNIFIED_HEMENVLERP)
    // Exactly ONE D3D11 HemEnvLerp pixel shader per RX33-35, identical
    // for P_Metal and nonmetal. Rebuild after R19 A/B source preparation
    // with a single PTDE post-EnvSpec material consumer.
    std::vector<std::uint8_t> unified;
    if (!operators::env_spec::
            materialize_nonmetal_lerp_r8_spec_linear_candidate(
                static_cast<const std::uint8_t *>(dxbc),
                dxbc_size, unified) ||
        unified.empty()) {
        ++lerp_replacement_register_fail_;
        char log[256]{};
#if defined(DSRRL_V346_LERP_REJECT_STAGE)
        std::snprintf(log,sizeof(log),
            #if defined(DSRRL_V347_LERP_ENVDIFFUSE_REBASE)
            "[DSRRL V347 LERP REJECT] stage=create rx=%u candidate=REJECTED gate=%u fail_open=stock",
#else
            "[DSRRL V346 LERP REJECT] stage=create rx=%u candidate=REJECTED gate=%u fail_open=stock",
#endif
            static_cast<unsigned>(outcome.semantic_receiver_id),
            static_cast<unsigned>(
                operators::env_spec::v346_lerp_reject_stage()));
#else
        std::snprintf(log,sizeof(log),
            "[DSRRL V345 UNIFIED LERP] stage=create rx=%u candidate=REJECTED fail_open=stock",
            static_cast<unsigned>(outcome.semantic_receiver_id));
#endif
        reshade::log::message(reshade::log::level::warning,log);
        return false;
    }
    dxbc=unified.data();
    dxbc_size=unified.size();
#endif
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
    log_final_pmetal_shader_digest(
        "HemEnvLerp", outcome.semantic_receiver_id,
        payload_sha256, dxbc_size);
#if defined(DSRRL_V345_UNIFIED_HEMENVLERP)
    char log[256]{};
    std::snprintf(log,sizeof(log),
#if defined(DSRRL_V348_PTDE_LERP_SPECRGB_RGB_FIX)
#if defined(DSRRL_V350_PTDE_LERP_RGB_COMPONENT_SWIZZLE)
        "[DSRRL V350 PTDE LERP RGB] stage=create rx=%u candidate=CREATED bytes=%zu variant_count=1 t10=XYZW c101=scalar_x dst=r1.yzw src1=XYZW src2=XXYZ",
#elif defined(DSRRL_V349_PTDE_LERP_ENVSPEC_YZW_FIX)
        "[DSRRL V349 PTDE RGB LIVEOUT] stage=create rx=%u candidate=CREATED bytes=%zu variant_count=1 t10=XYZW c101=scalar_x envspec=r1.yzw",
#else
        "[DSRRL V348 SPEC RGB] stage=create rx=%u candidate=CREATED bytes=%zu variant_count=1 swizzle=XYZW c101=scalar_x",
#endif
#else
        "[DSRRL V345 UNIFIED LERP] stage=create rx=%u candidate=CREATED bytes=%zu variant_count=1",
#endif
        static_cast<unsigned>(outcome.semantic_receiver_id),dxbc_size);
    reshade::log::message(reshade::log::level::info,log);
#endif

    ++lerp_replacement_register_ok_;
    return true;
}


#if defined(DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB)
bool pmetal_envspec_draw_runtime::
register_lerp_nonmetal_r8_candidate(
    const operators::env_spec::
        pmetal_rgba_lerp_materialize_outcome &outcome,
    const void *r19_dxbc,
    std::size_t r19_size) noexcept
{
    if (outcome.result != operators::env_spec::
            pmetal_rgba_lerp_materialize_result::applied ||
        outcome.semantic_receiver_id < 33u ||
        outcome.semantic_receiver_id > 35u ||
        !outcome.spec_rgb_consumer ||
        r19_dxbc == nullptr || r19_size==0u)
        return false;
#if defined(DSRRL_V345_UNIFIED_HEMENVLERP)
    // Canonical PS already built once at register_lerp_replacement.
    // No second R8 shader is ever created.
    return true;
#else
    std::vector<std::uint8_t> candidate;
    if (!operators::env_spec::
            materialize_nonmetal_lerp_r8_spec_linear_candidate(
                static_cast<const std::uint8_t *>(r19_dxbc),
                r19_size,
                candidate) ||
        candidate.empty())
        return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if (device_==nullptr || quarantined_.load())
        return false;
    if (lerp_replacements_.find(outcome.semantic_receiver_id) ==
        lerp_replacements_.end())
        return false;
    if (lerp_nonmetal_r8_replacements_.find(
            outcome.semantic_receiver_id) !=
        lerp_nonmetal_r8_replacements_.end())
        return true;
    ID3D11PixelShader *shader=nullptr;
    if (FAILED(device_->CreatePixelShader(
            candidate.data(),candidate.size(),nullptr,&shader)) ||
        shader==nullptr)
        return false;
    lerp_nonmetal_r8_replacements_[
        outcome.semantic_receiver_id]=shader;
    return true;
#endif
}
#endif

#if defined(DSRRL_V370_ISOLATED_EXACT_ENVDIFFUSE)
// Independent private receiver: never publishes a patch to stock pipeline,
// and never mutates/aliases the combined EnvSpec replacement bank.
bool pmetal_envspec_draw_runtime::register_independent_envdiffuse(
    const void *dxbc,std::size_t n,std::uint32_t rx) noexcept
{
    if(dxbc==nullptr || n==0u || rx<33u || rx>35u)return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if(device_==nullptr || quarantined_.load())return false;
    if(independent_envdiffuse_shaders_.count(rx)!=0u)return true;
    ID3D11PixelShader *shader=nullptr;
    if(FAILED(device_->CreatePixelShader(dxbc,n,nullptr,&shader)) ||
       shader==nullptr)return false;
    independent_envdiffuse_shaders_.emplace(rx,shader);
    char line[170]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V370 ED] PRIVATE_PTDE_C86_SHADER rx=%u bytes=%zu"
        " original_29_dword=1 t12=STOCK pixel=OPEN",rx,n);
    reshade::log::message(reshade::log::level::info,line);
    return true;
}

bool pmetal_envspec_draw_runtime::prepare_independent_envdiffuse(
    reshade::api::command_list *cmd_list,
    const mr::material_identity &material,
    const mr::decision &decision,
    prepared_pmetal_envspec_draw &prepared) noexcept
{
    prepared={};
    if(cmd_list==nullptr || quarantined_.load() ||
       !material.valid || !material.owner_tuple_exact ||
       !material.material_slot_valid || !decision.active ||
       decision.receiver_id<33u || decision.receiver_id>35u ||
       !core_.features().enabled(core::operator_id::env_diffuse) ||
       // Own source owner, NOT EnvSpec operator permission: a disabled
       // EnvSpec island must not prevent an authenticated diffuse draw.
       !exact_pmetal_material(material))
        return false;
    pmetal_envspec_source source{};
    if(!source_.latest(material,source) ||
       !source.envdiffuse_linear_valid ||
       !std::isfinite(source.beta) || source.beta!=0.0f)
        return false;
    for(float c:source.envdiffuse_a)
        if(!std::isfinite(c) || c<0.0f)return false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(device_==nullptr)return false;
        const auto found=independent_envdiffuse_shaders_.find(
            decision.receiver_id);
        if(found==independent_envdiffuse_shaders_.end() ||
           found->second==nullptr)return false;
        found->second->AddRef();
        prepared.shader=found->second;
    }
    auto *ctx=reinterpret_cast<ID3D11DeviceContext *>(
        cmd_list->get_native());
    if(ctx==nullptr){release(prepared);return false;}
    ID3D11ShaderResourceView *t11[1]{};
    if(!pixel_srv_shadow_snapshot(cmd_list,11u,1u,t11)) {
        if(t11[0]!=nullptr){t11[0]->Release();t11[0]=nullptr;}
        ctx->PSGetShaderResources(11u,1u,t11);
    }
    // Independent DSR t11 SHA/producer registry; never reuse t12 EnvSpec
    // ordinal as a diffuse identity. Unknown or duplicate source fails open.
    const bool exact_resource=env_resources_.prepare_envdiffuse_bound(
        t11[0],nullptr,false,prepared.envdiffuse_resources);
    if(t11[0]!=nullptr)t11[0]->Release();
    if(!exact_resource){release(prepared);return false;}

    // Exact old coupled ABI b12[3] = PTDE LightBank c86.rgb. Rows 0-2
    // remain zero; this private shader only reads b12[3].xyz.
    std::array<float,16> payload{};
    payload[12]=source.envdiffuse_a[0];
    payload[13]=source.envdiffuse_a[1];
    payload[14]=source.envdiffuse_a[2];
    std::array<std::uint32_t,16> bits{};
    std::memcpy(bits.data(),payload.data(),sizeof(payload));
    ID3D11Device *ctx_device=nullptr;
    ctx->GetDevice(&ctx_device);
    if(ctx_device==nullptr){release(prepared);return false;}
    const auto key=reinterpret_cast<std::uintptr_t>(ctx);
    ID3D11Buffer *buffer=nullptr;
    bool upload=true;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(ctx_device!=device_ || quarantined_.load()) {
            ctx_device->Release();release(prepared);return false;
        }
        auto it=independent_envdiffuse_cb_by_context_.find(key);
        if(it!=independent_envdiffuse_cb_by_context_.end() &&
           it->second.buffer!=nullptr) {
            buffer=it->second.buffer;
            buffer->AddRef();
            upload=!it->second.valid || it->second.payload_bits!=bits;
        } else {
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth=64u;
            desc.Usage=D3D11_USAGE_DYNAMIC;
            desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            if(FAILED(device_->CreateBuffer(&desc,nullptr,&buffer)) ||
               buffer==nullptr) {
                ctx_device->Release();release(prepared);return false;
            }
            envdiffuse_cb_cache entry{};
            entry.buffer=buffer;
            independent_envdiffuse_cb_by_context_.emplace(key,entry);
            buffer->AddRef();
        }
    }
    ctx_device->Release();
    if(upload) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(ctx->Map(buffer,0u,D3D11_MAP_WRITE_DISCARD,0u,&mapped)) ||
           mapped.pData==nullptr) {
            buffer->Release();release(prepared);return false;
        }
        std::memcpy(mapped.pData,payload.data(),sizeof(payload));
        ctx->Unmap(buffer,0u);
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it=independent_envdiffuse_cb_by_context_.find(key);
        if(it!=independent_envdiffuse_cb_by_context_.end() &&
           it->second.buffer==buffer) {
            it->second.payload_bits=bits;
            it->second.valid=true;
        }
    }
    prepared.b12=buffer;
    const auto owner=core::operator_bit(core::operator_id::env_diffuse);
    auto &q=prepared.request;
    q.primary=core::operator_id::env_diffuse;
    q.receiver_verified=true;
    q.material_verified=true;
    q.pixel_shader=prepared.shader;
    q.replace_pixel_shader=true;
    q.constant_buffers[0]={12u,prepared.b12,owner};
    q.constant_buffer_count=1u;
    q.srvs[0]={11u,prepared.envdiffuse_resources.ptde_a};
    q.srv_count=1u;
    q.samplers[0]={11u,prepared.envdiffuse_resources.sampler};
    q.sampler_count=1u;
    prepared.ready=true;
    static std::atomic<std::uint32_t> observations{0u};
    const auto n=observations.fetch_add(1u,std::memory_order_relaxed)+1u;
    if(n<=16u || (n&(n-1u))==0u) {
        char line[445]{};
        std::snprintf(line,sizeof(line),
            "[DSRRL V370 ED REQUEST] n=%u rx=%u route=%u slot=%u"
            " flver=%016llx bank=%016llx row=%u serial=%llu"
            " c86=%.8g,%.8g,%.8g diff_probe=%u b12=1 t11=PTDE t12=STOCK"
            " beta=%.8g pixel=OPEN",
            n,decision.receiver_id,decision.route_index,
            material.material_slot,
            static_cast<unsigned long long>(material.flver_identity_hash),
            static_cast<unsigned long long>(source.bank_signature_a),
            source.row_id_a,
            static_cast<unsigned long long>(source.serial),
            static_cast<double>(source.envdiffuse_a[0]),
            static_cast<double>(source.envdiffuse_a[1]),
            static_cast<double>(source.envdiffuse_a[2]),
            static_cast<unsigned>(prepared.envdiffuse_resources.probe_a),
            static_cast<double>(source.beta));
        reshade::log::message(reshade::log::level::warning,line);
    }
    return true;
}
#endif

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

    // This private HemEnv shader composes PTDE cubemap, SpecRGB and
    // Material Response. An independent resource sidecar may be prepared
    // without these, but the *combined pixel shader* may not: no DSR PBL
    // tail is a substitute for the PTDE material-domain consumer.
    const bool material_chain_ready =
        core_.features().enabled(core::operator_id::material_response) &&
        core_.features().enabled(core::operator_id::diffuse_material_domain) &&
        core_.features().enabled(core::operator_id::spec_rgb);
    const bool lerp_terminal_ready =
        family != pmetal_envspec_receiver_family::hemenvlerp ||
        core_.features().enabled(core::operator_id::terminal_sat_rgb);
    if (!material_chain_ready || !lerp_terminal_ready) {
        telemetry::hot_count(semantic_rejects_);
        effect_fail(effect_fail_mask_,
            family == pmetal_envspec_receiver_family::hemenvlerp
                ? k_effect_fail_lerp_feature : k_effect_fail_feature);
        log_prepare_stage_once(1u << 2u,
            "incomplete_ptde_material_spec_chain", material, decision, family);
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
#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)
        ((exact_pmetal_material(material) &&
          env_semantics.envspc_slot != 2u) ||
         (!exact_pmetal_material(material) &&
          env_semantics.envspc_slot >= 4u))) {
#else
        env_semantics.envspc_slot != 2u) {
#endif
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
        // Keep the native t11 source-vs-resource cut visible even if the
        // exact LightBank producer has not authorized its semantic donor.
        // This is read-only; the draw still fails open below.
        if (decision.receiver_id == 34u &&
            material.route_index == 345u &&
            cmd_list != nullptr) {
            auto *native_context =
                reinterpret_cast<ID3D11DeviceContext *>(
                    cmd_list->get_native());
            if (native_context != nullptr)
                witness_rx34_native_envdiffuse(
                    cmd_list, native_context, source, false);
        }
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

    // Exact P_Metal RX34 post-source, pre-transaction witness. No PS/CB/SRV
    // set calls, no shadow resource cache mutation and no authorization.
    if (decision.receiver_id == 34u &&
        material.route_index == 345u)
        witness_rx34_native_envdiffuse(cmd_list, context, source, true);

    ID3D11PixelShader *shader = nullptr;
    core::operator_mask composed_owners = 0u;
#if defined(DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB) && !defined(DSRRL_V345_UNIFIED_HEMENVLERP)
    bool r8_selected = false;
#endif

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
#if defined(DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB) && !defined(DSRRL_V345_UNIFIED_HEMENVLERP)
            // Dedicated Lerp variant only; stock R19 remains P_Metal's.
            if (material.route_index != 345u &&
                (
                    v203_equipment::exact_envspec36_material(material)
#if defined(DSRRL_V293_PARSER_EXACT_FLVER_ALIAS)
                    || v203_equipment::exact_spx_roughcloth_material(material)
#endif
                )) {
                const auto alternative =
                    lerp_nonmetal_r8_replacements_.find(
                        decision.receiver_id);
                if (alternative !=
                    lerp_nonmetal_r8_replacements_.end() &&
                    alternative->second!=nullptr)
                    { shader=alternative->second; r8_selected=true; }
            }
#endif
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

#if defined(DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB) && !defined(DSRRL_V345_UNIFIED_HEMENVLERP)
    if (r8_selected)
        v287_nonmetal_trace::log("v289_r8_ps_selected",material,decision,1u);
#endif
    effect_latch(effect_replacement_ready_);

    // For beta == 0, lerp(A, B, beta) == A. Bind B=A (including t14)
    // instead of rejecting the draw because the unconsumed B probe is absent.
    // The shader still receives a valid second SRV; nonzero blends retain
    // exact two-endpoint assignment and content validation.
    const bool probe_b_required = source.beta != 0.0f;

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
#if defined(DSRRL_V230_RUNTIME_TELEMETRY)
        // An EnvSpec probe reject is not a SpecRGB sidecar reject.
        // Record source A/B and the current verified pack/sampler state
        // without asking a similar texture/probe to stand in. No mutation.
        if (decision.receiver_id >= 33u &&
            decision.receiver_id <= 35u &&
            material.route_index == 345u) {
            static std::atomic<std::uint32_t> seen{0u};
            const auto sample = seen.fetch_add(
                1u,std::memory_order_relaxed);
            if (sample < 32u) {
                const auto env_t =
                    env_resources_.telemetry();
                char line[460]{};
                std::snprintf(line,sizeof(line),
                    "[DSRRL V230 PROBE FAIL CUT] sample=%u rx=%u "
                    "family=%u env_slot=%u beta=%.9g "
                    "b_required=%u native_feed=%u shadow_ready=%u "
                    "shadow_a=%p shadow_b=%p "
                    "pack_ready=%u sampler_ready=%u "
                    "native_candidates=%llu native_matches=%llu "
                    "view_matches=%llu pixel_pass=0",
                    sample+1u,
                    static_cast<unsigned>(decision.receiver_id),
                    static_cast<unsigned>(family),
                    static_cast<unsigned>(env_semantics.envspc_slot),
                    static_cast<double>(source.beta),
                    probe_b_required?1u:0u,
                    k_native_dsr_cubemap_feed?1u:0u,
                    shadow_env_ready?1u:0u,
                    static_cast<void *>(shadow_env[0]),
                    static_cast<void *>(shadow_env[2]),
                    env_t.pack_ready?1u:0u,
                    env_t.sampler_ready?1u:0u,
                    static_cast<unsigned long long>(
                        env_t.native_candidates),
                    static_cast<unsigned long long>(
                        env_t.native_matches),
                    static_cast<unsigned long long>(
                        env_t.view_matches));
                reshade::log::message(
                    reshade::log::level::warning,line);
            }
        }
#endif
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

    const bool material_ready =
        shadow_material_ready
            ? material_resources_.
                  prepare_draw_requests_bound(
                      shadow_material,
                      decision.receiver_id,
                      query,
                      material_chain_ready,
                      true,
                      prepared.material_resources)
            : material_resources_.
                  prepare_draw_requests(
                      context,
                      decision.receiver_id,
                      query,
                      material_chain_ready,
                      true,
                      prepared.material_resources);

    // Source-censused chr-bound stock t1 is not a missing player equipment
    // sidecar. Preserve the whole native character draw without trying the
    // PTDE equipment island or recording a false SpecRGB fail-open.
    if (prepared.material_resources.verified_character_not_equipment) {
        if (shader != nullptr)
            shader->Release();
        env_resources_.release(prepared.env_resources);
        material_resources_.release_prepared_draw(
            prepared.material_resources);
        log_prepare_stage_once(
            1u << 29u, "character_not_equipment_stock",
            material, decision, family);
        return false;
    }

    if (!material_ready ||
        !prepared.material_resources.spec_rgb) {
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
        family == pmetal_envspec_receiver_family::hemenvlerp &&
        source.beta != 0.0f;
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

    // The complete operator has four possible carrier slots:
    //   t11 = PTDE EnvDiffuse A (or native DSR A in legacy diagnostic),
    //   t12 = exact EnvSpec A cubemap,
    //   t13 = PTDE EnvDiffuse B for verified Lerp only,
    //   t14 = exact EnvSpec B cubemap (B=A when beta is exactly zero).
    // EnvSpec slots are common to every mode, including native-DSR
    // falsifiers. Independent t0/t2/t10 sidecars are appended elsewhere.
    std::uint32_t request_srv_count = 0u;
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    prepared.request.srvs[request_srv_count++] = {
        11u, prepared.envdiffuse_resources.ptde_a
    };
#elif !defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) && !defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
    prepared.request.srvs[request_srv_count++] = {
        11u, prepared.native_dsr_envdiffuse_a
    };
#endif

    prepared.request.srvs[request_srv_count++] = {
        12u, prepared.env_resources.ptde_a
    };

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
    if (family == pmetal_envspec_receiver_family::hemenvlerp)
        prepared.request.srvs[request_srv_count++] = {
            13u, prepared.envdiffuse_resources.ptde_b
        };
#endif
#elif !defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) && !defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
    if (native_envdiffuse_b_required)
        prepared.request.srvs[request_srv_count++] = {
            13u, prepared.native_dsr_envdiffuse_b
        };
#endif

    prepared.request.srvs[request_srv_count++] = {
        14u, prepared.env_resources.ptde_b
    };
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
    // The PTDE EnvDiffuse resource is consumed only by the complete PTDE
    // shader, and carries its own exact sampler; no s11/s13 on native
    // diagnostic paths. Lerp's B sampler remains bound even for B=A.
    prepared.request.samplers[request_sampler_count++] = {
        11u, prepared.envdiffuse_resources.sampler
    };
#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
    if (family == pmetal_envspec_receiver_family::hemenvlerp)
        prepared.request.samplers[request_sampler_count++] = {
            13u, prepared.envdiffuse_resources.sampler
        };
#endif
#endif
    // Both EnvSpec cubemaps always use the same exact PTDE sampler,
    // including the A-only/zero-beta path where t14 is an alias.
    prepared.request.samplers[request_sampler_count++] = {
        12u, prepared.env_resources.sampler
    };
    prepared.request.samplers[request_sampler_count++] = {
        14u, prepared.env_resources.sampler
    };
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

#if defined(DSRRL_V291_LERP_CUBEMAP_CENSUS)
    if (family == pmetal_envspec_receiver_family::hemenvlerp) {
        prepared.diagnostic_lerp = true;
        prepared.diagnostic_beta = source.beta;
        prepared.diagnostic_serial = source.serial;
        prepared.diagnostic_probe_a = prepared.env_resources.probe_a;
        prepared.diagnostic_probe_b = prepared.env_resources.probe_b;
        prepared.diagnostic_diff_a = prepared.envdiffuse_resources.probe_a;
        prepared.diagnostic_diff_b = prepared.envdiffuse_resources.probe_b;
    }
#endif
#if defined(DSRRL_V336_WANDERER_SRV_AUDIT) && defined(DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB)
    if (r8_selected &&
        family == pmetal_envspec_receiver_family::hemenvlerp)
        v336_wanderer_srv_witness(
            material, decision, env_resources_, prepared.env_resources,
            shadow_env_ready, shadow_material_ready,
            source.beta, source.serial);
#endif
#if defined(DSRRL_V345_UNIFIED_HEMENVLERP)
    if (family == pmetal_envspec_receiver_family::hemenvlerp) {
        static std::atomic<std::uint32_t> once{0u};
        const bool pmetal = material.route_index == 345u;
        const auto bit = 1u << (decision.receiver_id-33u+
                                 (pmetal ? 0u : 3u));
        if (!(once.fetch_or(bit,std::memory_order_relaxed)&bit)) {
            char log[320]{};
            std::snprintf(log,sizeof(log),
                "[DSRRL V345 UNIFIED LERP] stage=request_ready rx=%u route=%u class=%s single_ps=1 t10=post_env t1=native beta=%.9g",
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(material.route_index),
                pmetal ? "P_METAL" : "NONMETAL",
                static_cast<double>(source.beta));
            reshade::log::message(reshade::log::level::info,log);
        }
    }
#endif
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
    g_prepare_stage_log_mask.store(0u);
    g_value_cut_log_mask.store(0u);
    quarantined_.store(false);
}

} // namespace dsrrl::runtime
