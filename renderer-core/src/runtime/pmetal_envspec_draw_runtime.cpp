#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"

#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <Windows.h>
#include <d3d11.h>

#include <array>
#include <cmath>
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
constexpr std::uint32_t k_effect_fail_lerp_ul_conflict = 1u << 6u;
constexpr std::uint32_t k_effect_fail_context = 1u << 7u;
constexpr std::uint32_t k_effect_fail_replacement = 1u << 8u;
constexpr std::uint32_t k_effect_fail_ul = 1u << 9u;
constexpr std::uint32_t k_effect_fail_probe = 1u << 10u;
constexpr std::uint32_t k_effect_fail_spec_rgb = 1u << 11u;
constexpr std::uint32_t k_effect_fail_device = 1u << 12u;
constexpr std::uint32_t k_effect_fail_b12 = 1u << 13u;
constexpr std::uint32_t k_effect_fail_mutation = 1u << 14u;

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
    return
        decision.active &&
        decision.route_index ==
            k_pmetal_route_index &&
        decision.receiver_id >= 33u &&
        decision.receiver_id <= 35u &&
        (decision.certified_operations &
         mr::material_response_operation::
             specular_factor_c101) != 0u;
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

pmetal_envspec_draw_runtime::
pmetal_envspec_draw_runtime(
    core::renderer_core &core,
    upper_lower_draw_runtime &lightbank,
    envspec_resource_runtime &env_resources,
    material_resource_draw_runtime &material_resources) noexcept
    : core_(core),
      lightbank_(lightbank),
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

    for (auto &[_,pair] :
         replacements_) {
        if (pair.base != nullptr)
            pair.base->Release();
        if (pair.upper_lower != nullptr)
            pair.upper_lower->Release();
    }
    replacements_.clear();

    for (auto &[_,shader] :
         lerp_replacements_) {
        if (shader != nullptr)
            shader->Release();
    }
    lerp_replacements_.clear();

    for (auto &[_,entry] :
         b12_by_context_) {
        if (entry.buffer != nullptr)
            entry.buffer->Release();
    }
    b12_by_context_.clear();

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

    for (auto &[_,pair] :
         replacements_) {
        if (pair.base != nullptr)
            pair.base->Release();
        if (pair.upper_lower != nullptr)
            pair.upper_lower->Release();
    }
    replacements_.clear();

    for (auto &[_,shader] :
         lerp_replacements_) {
        if (shader != nullptr)
            shader->Release();
    }
    lerp_replacements_.clear();

    for (auto &[_,entry] :
         b12_by_context_) {
        if (entry.buffer != nullptr)
            entry.buffer->Release();
    }
    b12_by_context_.clear();

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

    if (outcome.result != result::applied ||
        outcome.receiver_id < 33u ||
        outcome.receiver_id > 35u ||
        !outcome.spec_rgb_consumer ||
        dxbc == nullptr ||
        dxbc_size == 0u ||
        quarantined_.load()) {
        ++replacement_register_fail_;
        return false;
    }

    std::lock_guard<std::mutex> lock(
        mutex_);

    if (device_ == nullptr) {
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

    auto &pair =
        replacements_[
            outcome.receiver_id];

    ID3D11PixelShader *&target =
        outcome.upper_lower_composed
            ? pair.upper_lower
            : pair.base;

    if (target != nullptr)
        target->Release();

    target = shader;

    auto &owners =
        outcome.upper_lower_composed
            ? pair.upper_lower_owners
            : pair.base_owners;

    owners =
        outcome.composed_owners;

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
        !outcome.envdiffuse_preserved ||
        !outcome.upper_lower_composed ||
        !outcome.terminal_sat_rgb_composed ||
        !outcome.spec_rgb_consumer ||
        dxbc == nullptr ||
        dxbc_size == 0u ||
        quarantined_.load()) {
        ++lerp_replacement_register_fail_;
        return false;
    }

    std::lock_guard<std::mutex> lock(
        mutex_);

    if (device_ == nullptr) {
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

    auto &target =
        lerp_replacements_[
            outcome.semantic_receiver_id];

    if (target != nullptr)
        target->Release();

    target = shader;

    ++lerp_replacement_register_ok_;
    return true;
}

bool pmetal_envspec_draw_runtime::prepare(
    reshade::api::command_list *cmd_list,
    const mr::material_identity &material,
    const mr::decision &decision,
    bool upper_lower_receiver_verified,
    prepared_pmetal_envspec_draw &prepared) noexcept
{
    return prepare(
        cmd_list,
        material,
        decision,
        pmetal_envspec_receiver_family::
            stable_hemenv,
        upper_lower_receiver_verified,
        prepared);
}

bool pmetal_envspec_draw_runtime::prepare(
    reshade::api::command_list *cmd_list,
    const mr::material_identity &material,
    const mr::decision &decision,
    pmetal_envspec_receiver_family family,
    bool upper_lower_receiver_verified,
    prepared_pmetal_envspec_draw &prepared) noexcept
{
    prepared = {};
    effect_latch(effect_entry_seen_);

    if (cmd_list == nullptr ||
        quarantined_.load() ||
        !core_.features().enabled(
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
             core::operator_id::upper_lower) ||
         !core_.features().enabled(
             core::operator_id::terminal_sat_rgb))) {
        telemetry::hot_count(semantic_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_lerp_feature);
        return false;
    }

    telemetry::hot_count(candidates_);
    if (family ==
        pmetal_envspec_receiver_family::
            hemenvlerp)
        telemetry::hot_count(lerp_candidates_);

    if (!exact_pmetal_material(
            material) ||
        !exact_pmetal_decision(
            decision)) {
        telemetry::hot_count(material_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_material);
        return false;
    }
    effect_latch(effect_material_ready_);

    const auto query =
        make_query(
            material,
            decision.receiver_id);

    const auto env_semantics =
        mr::classify_mtd_envspec_semantics(
            query);

    // classify_mtd_semantic(...env_spec) begins by running the same exact
    // EnvSpec router lookup again. At this point P_Metal material + route are
    // already exact, so the exact router result is the authoritative semantic
    // gate. Avoid scanning the 325-record router twice per candidate draw.
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
        return false;
    }
    effect_latch(effect_semantic_ready_);

    pmetal_env_source source{};
    if (!lightbank_.
            selected_pmetal_env_source(
                source) ||
        !std::isfinite(source.beta)) {
        telemetry::hot_count(source_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_source);
        return false;
    }
    effect_latch(effect_source_ready_);

    // Stable HemEnv remains a one-endpoint consumer. HemEnvLerp is an
    // independently attested A/B+beta consumer and is the only family allowed
    // to carry finite blended source state through this island.
    if (family ==
            pmetal_envspec_receiver_family::
                stable_hemenv &&
        source.beta != 0.0f) {
        telemetry::hot_count(blended_receiver_hold_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_blend);
        return false;
    }

    if (family ==
            pmetal_envspec_receiver_family::
                hemenvlerp &&
        upper_lower_receiver_verified) {
        telemetry::hot_count(semantic_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_lerp_ul_conflict);
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
        return false;
    }

    replacement_pair pair{};
    ID3D11PixelShader *lerp_shader = nullptr;

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
                return false;
            }

            lerp_shader =
                found->second;
            lerp_shader->AddRef();
        } else {
            const auto found =
                replacements_.find(
                    decision.receiver_id);

            if (found ==
                replacements_.end()) {
                ++replacement_register_fail_;
                effect_fail(
                    effect_fail_mask_,
                    k_effect_fail_replacement);
                return false;
            }

            pair =
                found->second;

            if (pair.base != nullptr)
                pair.base->AddRef();
            if (pair.upper_lower != nullptr)
                pair.upper_lower->AddRef();
        }
    }

    if (family ==
            pmetal_envspec_receiver_family::
                stable_hemenv &&
        pair.base == nullptr) {
        if (pair.upper_lower != nullptr)
            pair.upper_lower->Release();
        ++replacement_register_fail_;
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_replacement);
        return false;
    }
    effect_latch(effect_replacement_ready_);

    bool use_upper_lower = false;

    if (family ==
            pmetal_envspec_receiver_family::
                hemenvlerp) {
        // Exact Lerp materialization already owns the verified b0[7]/b0[8]
        // -> b13[6]/b13[7] consumer remap. The matching PTDE carrier is
        // therefore mandatory; missing producer state fails open to stock.
        if (!lightbank_.
                prepare_upper_lower_carrier(
                    context,
                    prepared.upper_lower)) {
            if (lerp_shader != nullptr)
                lerp_shader->Release();
            telemetry::hot_count(upper_lower_fallback_);
            effect_fail(
                effect_fail_mask_,
                k_effect_fail_ul);
            return false;
        }
        use_upper_lower = true;
        telemetry::hot_count(upper_lower_ready_);
    } else if (
        upper_lower_receiver_verified &&
        pair.upper_lower != nullptr &&
        lightbank_.
            prepare_upper_lower_carrier(
                context,
                prepared.upper_lower)) {
        use_upper_lower = true;
        telemetry::hot_count(upper_lower_ready_);
    } else {
        telemetry::hot_count(upper_lower_fallback_);
    }

    ID3D11PixelShader *shader =
        family ==
            pmetal_envspec_receiver_family::
                hemenvlerp
            ? lerp_shader
            : (use_upper_lower
                ? pair.upper_lower
                : pair.base);

    core::operator_mask composed_owners =
        family ==
            pmetal_envspec_receiver_family::
                hemenvlerp
            ? 0u
            : (use_upper_lower
                ? pair.upper_lower_owners
                : pair.base_owners);

    if (family ==
        pmetal_envspec_receiver_family::
            stable_hemenv) {
        if (use_upper_lower) {
            pair.base->Release();
            pair.base = nullptr;
        } else if (pair.upper_lower != nullptr) {
            pair.upper_lower->Release();
            pair.upper_lower = nullptr;
        }
    }

    const bool probe_b_required =
        family ==
            pmetal_envspec_receiver_family::
                hemenvlerp ||
        source.beta != 0.0f;

    if (!env_resources_.prepare(
            context,
            env_semantics.envspc_slot,
            probe_b_required,
            prepared.env_resources)) {
        if (shader != nullptr)
            shader->Release();
        lightbank_.release_prepared_draw(
            prepared.upper_lower);
        telemetry::hot_count(probe_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_probe);
        return false;
    }
    effect_latch(effect_probe_ready_);

    if (!material_resources_.
            prepare_draw_requests(
                context,
                decision.receiver_id,
                query,
                true,
                true,
                prepared.material_resources) ||
        !prepared.material_resources.spec_rgb) {
        if (shader != nullptr)
            shader->Release();
        env_resources_.release(
            prepared.env_resources);
        lightbank_.release_prepared_draw(
            prepared.upper_lower);
        material_resources_.
            release_prepared_draw(
                prepared.material_resources);
        telemetry::hot_count(spec_rgb_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_spec_rgb);
        return false;
    }
    effect_latch(effect_spec_rgb_ready_);

    const std::array<f4,4> payload{{
        {
            decision.c101_f0q[0],
            decision.c101_f0q[1],
            decision.c101_f0q[2],
            1.0f
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
            source.b[0],
            source.b[1],
            source.b[2],
            source.beta
        }
    }};
    static_assert(sizeof(payload) == 64u);

    ID3D11Device *device = nullptr;
    context->GetDevice(&device);

    if (device == nullptr) {
        if (shader != nullptr)
            shader->Release();
        env_resources_.release(
            prepared.env_resources);
        lightbank_.release_prepared_draw(
            prepared.upper_lower);
        material_resources_.
            release_prepared_draw(
                prepared.material_resources);
        telemetry::hot_count(source_rejects_);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_device);
        return false;
    }

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
            lightbank_.release_prepared_draw(
                prepared.upper_lower);
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
                std::memcmp(
                    found->second.payload.data(),
                    payload.data(),
                    sizeof(payload)) != 0;
        } else {
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = 64u;
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
                lightbank_.release_prepared_draw(
                    prepared.upper_lower);
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
            lightbank_.release_prepared_draw(
                prepared.upper_lower);
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
                std::memcpy(
                    found->second.payload.data(),
                    payload.data(),
                    sizeof(payload));
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
    const auto sat_owner =
        core::operator_bit(
            core::operator_id::
                terminal_sat_rgb);
    const auto ul_owner =
        core::operator_bit(
            core::operator_id::
                upper_lower);

    prepared.shader = shader;
    prepared.b12 = b12;
    prepared.upper_lower_composed =
        use_upper_lower;

    prepared.request.primary =
        core::operator_id::env_spec;

    prepared.request.additional_owners =
        mr_owner |
        domain_owner |
        spec_owner |
        composed_owners;

    if (family ==
        pmetal_envspec_receiver_family::
            hemenvlerp)
        prepared.request.additional_owners |=
            sat_owner;

    prepared.request.additional_shader_owners =
        prepared.request.additional_owners;
    prepared.request.additional_constant_buffer_owners =
        mr_owner;

    if (use_upper_lower) {
        prepared.request.additional_owners |=
            ul_owner;
        prepared.request.additional_shader_owners |=
            ul_owner;
        prepared.request.additional_constant_buffer_owners |=
            ul_owner;
        prepared.request.additional_carrier_owners |=
            ul_owner;
    }

    prepared.request.receiver_verified = true;
    prepared.request.material_verified = true;
    prepared.request.pixel_shader =
        shader;
    prepared.request.replace_pixel_shader =
        true;

    prepared.request.constant_buffers[0] = {
        12u,
        b12,
        env_owner | mr_owner
    };
    prepared.request.constant_buffer_count =
        1u;

    if (use_upper_lower) {
        prepared.request.constant_buffers[
            prepared.request.constant_buffer_count++] = {
                13u,
                prepared.upper_lower.b13,
                ul_owner
            };
    }

    prepared.request.srvs[0] = {
        12u,
        prepared.env_resources.ptde_a
    };
    prepared.request.srvs[1] = {
        14u,
        prepared.env_resources.ptde_b
    };
    prepared.request.srv_count = 2u;

    prepared.request.samplers[0] = {
        12u,
        prepared.env_resources.sampler
    };
    prepared.request.samplers[1] = {
        14u,
        prepared.env_resources.sampler
    };
    prepared.request.sampler_count = 2u;

    draw_tx_mutation verify{};
    if (build_island_draw_mutation(
            prepared.request,
            verify) !=
        island_draw_adapter_result::ready) {
        release(prepared);
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_mutation);
        return false;
    }

    prepared.ready = true;
    effect_latch(effect_request_ready_);
    telemetry::hot_count(requests_);
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

    lightbank_.release_prepared_draw(
        prepared.upper_lower);

    if (prepared.b12 != nullptr)
        prepared.b12->Release();

    if (prepared.shader != nullptr)
        prepared.shader->Release();

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
        upper_lower_ready_.load(),
        upper_lower_fallback_.load(),
        requests_.load(),
        lerp_requests_.load(),
        b12_uploads_.load(),
        b12_reuses_.load(),
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
    upper_lower_ready_.store(0u);
    upper_lower_fallback_.store(0u);
    requests_.store(0u);
    lerp_requests_.store(0u);
    b12_uploads_.store(0u);
    b12_reuses_.store(0u);
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
    quarantined_.store(false);
}

} // namespace dsrrl::runtime
