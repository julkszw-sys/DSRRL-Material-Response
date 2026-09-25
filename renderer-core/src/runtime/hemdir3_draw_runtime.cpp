#include "dsrrl/runtime/hemdir3_draw_runtime.hpp"
#include "dsrrl/operators/lightbank/hemdir3.hpp"
#include "dsrrl/runtime/hemdir3_mode_transport.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>

namespace dsrrl::runtime {

hemdir3_draw_runtime::hemdir3_draw_runtime(
    core::renderer_core &core,
    upper_lower_draw_runtime &lightbank) noexcept
    : core_(core),
      lightbank_(lightbank)
{
}

hemdir3_draw_runtime::~hemdir3_draw_runtime()
{
    release_resources();
}

void hemdir3_draw_runtime::release_resources() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto &entry : replacements_) {
        if (entry.second.shader != nullptr)
            entry.second.shader->Release();
    }
    replacements_.clear();

    if (device_ != nullptr) {
        device_->Release();
        device_ = nullptr;
    }
}

void hemdir3_draw_runtime::on_init_device(
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

    std::lock_guard<std::mutex> lock(mutex_);

    if (device_ == nullptr) {
        native->AddRef();
        device_ = native;
        return;
    }

    if (device_ != native)
        quarantined_.store(true);
}

void hemdir3_draw_runtime::on_destroy_device(
    reshade::api::device *device) noexcept
{
    if (device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11)
        return;

    auto *native =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());

    std::lock_guard<std::mutex> lock(mutex_);

    if (native != device_)
        return;

    for (auto &entry : replacements_) {
        if (entry.second.shader != nullptr)
            entry.second.shader->Release();
    }
    replacements_.clear();

    if (device_ != nullptr) {
        device_->Release();
        device_ = nullptr;
    }
}

bool hemdir3_draw_runtime::register_replacement(
    const operators::lightbank::
        hemdir3_b13_materialize_outcome &outcome,
    const void *dxbc,
    std::size_t dxbc_size) noexcept
{
    if (outcome.result !=
            operators::lightbank::
                hemdir3_b13_materialize_result::applied ||
        outcome.plan_index == 0xFFFFu ||
        dxbc == nullptr ||
        dxbc_size == 0u ||
        quarantined_.load()) {
        ++replacement_register_fail_;
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (device_ == nullptr) {
        ++replacement_register_fail_;
        return false;
    }

    ID3D11PixelShader *shader = nullptr;

    if (FAILED(device_->CreatePixelShader(
            dxbc,
            dxbc_size,
            nullptr,
            &shader)) ||
        shader == nullptr) {
        ++replacement_register_fail_;
        return false;
    }

    const replacement_record record{
        shader,
        outcome.stratum,
        outcome.paired_stable_receiver_id,
        outcome.composed_owners
    };

    const auto found =
        replacements_.find(
            outcome.plan_index);

    if (found != replacements_.end()) {
        const bool metadata_match =
            found->second.stratum ==
                record.stratum &&
            found->second.paired_stable_receiver_id ==
                record.paired_stable_receiver_id &&
            found->second.composed_owners ==
                record.composed_owners;

        if (!metadata_match) {
            shader->Release();
            quarantined_.store(true);
            ++replacement_register_fail_;
            return false;
        }

        if (found->second.shader != nullptr)
            found->second.shader->Release();

        found->second = record;
    } else {
        replacements_.emplace(
            outcome.plan_index,
            record);
    }

    ++replacement_register_ok_;
    return true;
}

bool hemdir3_draw_runtime::prepare_draw_request(
    reshade::api::command_list *cmd_list,
    const hemdir3_receiver_identity &identity,
    prepared_hemdir3_draw &prepared) noexcept
{
    prepared = {};

    if (cmd_list == nullptr ||
        !identity.valid() ||
        quarantined_.load() ||
        !core_.features().enabled(
            core::operator_id::hemdir3))
        return false;

    ++candidates_;

    std::uint32_t effective_mode = 0u;

    if (!hemdir3_mode_transport::snapshot(
            effective_mode) ||
        effective_mode != 2u) {
        ++mode_rejects_;
        return false;
    }

    ++mode2_hits_;

    replacement_record replacement{};

    {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto found =
            replacements_.find(
                identity.plan_index);

        if (found == replacements_.end() ||
            found->second.shader == nullptr ||
            found->second.stratum !=
                identity.stratum ||
            found->second.paired_stable_receiver_id !=
                identity.paired_stable_receiver_id) {
            ++readiness_rejects_;
            return false;
        }

        replacement = found->second;
        replacement.shader->AddRef();
    }

    if (identity.stratum ==
        operators::lightbank::
            hemdir3_native_stratum::spc) {
        replacement.shader->Release();
        ++spc_b12_hold_;
        return false;
    }

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());

    if (context == nullptr) {
        replacement.shader->Release();
        ++carrier_rejects_;
        return false;
    }

    if (!lightbank_.prepare_hemdir3_carrier(
            context,
            static_cast<std::uint32_t>(
                identity.plan_index) + 1u,
            prepared.carrier)) {
        replacement.shader->Release();
        ++carrier_rejects_;
        return false;
    }

    ++carrier_ready_;

    core::activation_context activation{};
    activation.receiver_verified = true;
    activation.producer_ready = true;
    activation.consumer_verified = true;

    operators::lightbank::
        hemdir3_runtime_context runtime{};

    runtime.semantic_mode = 2u;
    runtime.semantic_mode_provenance =
        operators::lightbank::
            hemdir3_semantic_mode_provenance::
                exact_effective_mode2;

    runtime.producer_snapshot =
        prepared.carrier.fingerprint;
    runtime.draw_snapshot =
        prepared.carrier.fingerprint;

    runtime.upper_lower_source_ready =
        prepared.carrier.upper_lower_ready;
    runtime.d123_source_ready =
        prepared.carrier.d123_ready;
    runtime.b13_carrier_ready =
        prepared.carrier.ready;

    runtime.receiver_verified = true;
    runtime.receiver_stratum =
        operators::lightbank::
            hemdir3_receiver_stratum::nospc;

    // Exact native HemDir3 is the consumer family for semantic mode2.
    // The b13 replacement changes only c92..c99 operand routing; the native
    // material/post-fog continuation is preserved byte-for-byte. Native
    // HemDir3 has no EnvDiffuse cube-sampler source branch.
    runtime.host_envdiffuse_source_suppressed = true;
    runtime.material_continuation_ready = true;
    runtime.downstream_material_domain_ready = true;
    runtime.downstream_postfog_ready = true;
    runtime.atmosphere_route_verified = true;
    runtime.draw_transaction_ready = true;

    const auto plan =
        operators::lightbank::
            evaluate_hemdir3_runtime_readiness(
                core_.features(),
                activation,
                runtime);

    if (!plan.ready ||
        !plan.suppress_host_envdiffuse ||
        !plan.use_ptde_linear_d123 ||
        plan.apply_source_gamma_compensation ||
        plan.require_b12_material_donor ||
        plan.require_directional_legacy_specular) {
        replacement.shader->Release();
        lightbank_.release_hemdir3_carrier(
            prepared.carrier);
        ++readiness_rejects_;
        return false;
    }

    prepared.shader =
        replacement.shader;
    prepared.identity =
        identity;

    prepared.request.primary =
        core::operator_id::hemdir3;

    prepared.request.additional_owners =
        core::operator_bit(
            core::operator_id::upper_lower) |
        replacement.composed_owners;

    prepared.request.receiver_verified = true;
    prepared.request.material_verified = false;

    prepared.request.pixel_shader =
        replacement.shader;
    prepared.request.replace_pixel_shader = true;

    prepared.request.constant_buffers[0] = {
        13u,
        prepared.carrier.b13
    };
    prepared.request.constant_buffer_count = 1u;

    draw_tx_mutation verify{};

    if (build_island_draw_mutation(
            prepared.request,
            verify) !=
        island_draw_adapter_result::ready) {
        release_prepared_draw(prepared);
        ++readiness_rejects_;
        return false;
    }

    prepared.ready = true;
    ++nospc_ready_;
    ++requests_;
    return true;
}

void hemdir3_draw_runtime::release_prepared_draw(
    prepared_hemdir3_draw &prepared) noexcept
{
    if (prepared.shader != nullptr)
        prepared.shader->Release();

    lightbank_.release_hemdir3_carrier(
        prepared.carrier);

    prepared = {};
}

hemdir3_draw_telemetry
hemdir3_draw_runtime::telemetry() const noexcept
{
    return {
        replacement_register_ok_.load(),
        replacement_register_fail_.load(),
        candidates_.load(),
        mode2_hits_.load(),
        mode_rejects_.load(),
        carrier_ready_.load(),
        carrier_rejects_.load(),
        nospc_ready_.load(),
        spc_b12_hold_.load(),
        readiness_rejects_.load(),
        requests_.load(),
        quarantined_.load()
    };
}

void hemdir3_draw_runtime::reset() noexcept
{
    release_resources();

    replacement_register_ok_.store(0);
    replacement_register_fail_.store(0);
    candidates_.store(0);
    mode2_hits_.store(0);
    mode_rejects_.store(0);
    carrier_ready_.store(0);
    carrier_rejects_.store(0);
    nospc_ready_.store(0);
    spc_b12_hold_.store(0);
    readiness_rejects_.store(0);
    requests_.store(0);
    quarantined_.store(false);
}

} // namespace dsrrl::runtime
