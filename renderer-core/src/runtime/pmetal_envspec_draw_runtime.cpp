#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"

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

    for (auto &[_,buffer] :
         b12_by_context_) {
        if (buffer != nullptr)
            buffer->Release();
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

    for (auto &[_,buffer] :
         b12_by_context_) {
        if (buffer != nullptr)
            buffer->Release();
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

bool pmetal_envspec_draw_runtime::prepare(
    reshade::api::command_list *cmd_list,
    const mr::material_identity &material,
    const mr::decision &decision,
    bool upper_lower_receiver_verified,
    prepared_pmetal_envspec_draw &prepared) noexcept
{
    prepared = {};

    if (cmd_list == nullptr ||
        quarantined_.load() ||
        !core_.features().enabled(
            core::operator_id::env_spec))
        return false;

    ++candidates_;

    if (!exact_pmetal_material(
            material) ||
        !exact_pmetal_decision(
            decision)) {
        ++material_rejects_;
        return false;
    }

    const auto query =
        make_query(
            material,
            decision.receiver_id);

    const auto env_semantics =
        mr::classify_mtd_envspec_semantics(
            query);

    const auto env_decision =
        mr::classify_mtd_semantic(
            query,
            mr::mtd_semantic_operator::
                env_spec);

    if (!env_semantics.exact_identity_match ||
        env_semantics.presence !=
            mr::ptde_envspec_presence::
                present ||
        env_semantics.router_state !=
            mr::mtd_envspec_router_state::
                present ||
        !env_semantics.envspc_slot_valid ||
        env_semantics.envspc_slot != 2u ||
        env_decision.state !=
            mr::mtd_semantic_state::use) {
        ++semantic_rejects_;
        return false;
    }

    pmetal_env_source source{};
    if (!lightbank_.
            selected_pmetal_env_source(
                source) ||
        !std::isfinite(source.beta)) {
        ++source_rejects_;
        return false;
    }

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());

    if (context == nullptr) {
        ++source_rejects_;
        return false;
    }

    replacement_pair pair{};
    {
        std::lock_guard<std::mutex> lock(
            mutex_);

        const auto found =
            replacements_.find(
                decision.receiver_id);

        if (found ==
            replacements_.end()) {
            ++replacement_register_fail_;
            return false;
        }

        pair =
            found->second;

        if (pair.base != nullptr)
            pair.base->AddRef();
        if (pair.upper_lower != nullptr)
            pair.upper_lower->AddRef();
    }

    if (pair.base == nullptr) {
        if (pair.upper_lower != nullptr)
            pair.upper_lower->Release();
        ++replacement_register_fail_;
        return false;
    }

    bool use_upper_lower = false;

    if (upper_lower_receiver_verified &&
        pair.upper_lower != nullptr &&
        lightbank_.
            prepare_upper_lower_carrier(
                context,
                prepared.upper_lower)) {
        use_upper_lower = true;
        ++upper_lower_ready_;
    } else {
        ++upper_lower_fallback_;
    }

    ID3D11PixelShader *shader =
        use_upper_lower
            ? pair.upper_lower
            : pair.base;

    core::operator_mask composed_owners =
        use_upper_lower
            ? pair.upper_lower_owners
            : pair.base_owners;

    if (use_upper_lower) {
        pair.base->Release();
        pair.base = nullptr;
    } else {
        if (pair.upper_lower != nullptr) {
            pair.upper_lower->Release();
            pair.upper_lower = nullptr;
        }
    }

    const bool probe_b_required =
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
        ++probe_rejects_;
        return false;
    }

    if (!material_resources_.
            prepare_draw_requests(
                context,
                decision.receiver_id,
                query,
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
        ++spec_rgb_rejects_;
        return false;
    }

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
        ++source_rejects_;
        return false;
    }

    ID3D11Buffer *b12 = nullptr;

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
            return false;
        }

        const auto key =
            reinterpret_cast<std::uintptr_t>(
                context);

        const auto found =
            b12_by_context_.find(key);

        if (found !=
                b12_by_context_.end() &&
            found->second != nullptr) {
            b12 =
                found->second;
            b12->AddRef();
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
                ++source_rejects_;
                return false;
            }

            b12_by_context_.emplace(
                key,
                b12);
            b12->AddRef();
        }
    }

    device->Release();

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
        ++source_rejects_;
        return false;
    }

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

    std::memcpy(
        mapped.pData,
        payload.data(),
        sizeof(payload));

    context->Unmap(
        b12,
        0u);

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
            core::operator_id::
                spec_rgb);
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

    prepared.request.additional_shader_owners =
        prepared.request.additional_owners;

    if (use_upper_lower) {
        prepared.request.additional_owners |=
            ul_owner;
        prepared.request.additional_shader_owners |=
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
        b12
    };
    prepared.request.constant_buffer_count =
        1u;

    if (use_upper_lower) {
        prepared.request.constant_buffers[
            prepared.request.constant_buffer_count++] = {
                13u,
                prepared.upper_lower.b13
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
        return false;
    }

    prepared.ready = true;
    ++requests_;
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
        candidates_.load(),
        material_rejects_.load(),
        semantic_rejects_.load(),
        source_rejects_.load(),
        probe_rejects_.load(),
        spec_rgb_rejects_.load(),
        upper_lower_ready_.load(),
        upper_lower_fallback_.load(),
        requests_.load(),
        quarantined_.load()
    };
}

void pmetal_envspec_draw_runtime::reset() noexcept
{
    release_resources();

    replacement_register_ok_.store(0u);
    replacement_register_fail_.store(0u);
    candidates_.store(0u);
    material_rejects_.store(0u);
    semantic_rejects_.store(0u);
    source_rejects_.store(0u);
    probe_rejects_.store(0u);
    spec_rgb_rejects_.store(0u);
    upper_lower_ready_.store(0u);
    upper_lower_fallback_.store(0u);
    requests_.store(0u);
    quarantined_.store(false);
}

} // namespace dsrrl::runtime
