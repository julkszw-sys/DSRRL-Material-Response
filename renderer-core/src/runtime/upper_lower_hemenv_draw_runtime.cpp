#include "dsrrl/runtime/upper_lower_hemenv_draw_runtime.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>

namespace dsrrl::runtime {

upper_lower_hemenv_draw_runtime::
upper_lower_hemenv_draw_runtime(
    core::renderer_core &core,
    upper_lower_draw_runtime &lightbank) noexcept
    : core_(core),
      lightbank_(lightbank)
{
}

upper_lower_hemenv_draw_runtime::
~upper_lower_hemenv_draw_runtime()
{
    release_resources();
}

void upper_lower_hemenv_draw_runtime::
release_resources() noexcept
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

void upper_lower_hemenv_draw_runtime::on_init_device(
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

void upper_lower_hemenv_draw_runtime::on_destroy_device(
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

bool upper_lower_hemenv_draw_runtime::register_replacement(
    const operators::lightbank::
        upper_lower_hemenv_materialize_outcome &outcome,
    const void *dxbc,
    std::size_t dxbc_size) noexcept
{
    using result =
        operators::lightbank::
            upper_lower_hemenv_materialize_result;

    if (outcome.result != result::applied ||
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

    replacement_record record{};
    record.shader = shader;
    record.stratum = outcome.stratum;
    record.family = outcome.family;
    record.stable_receiver_id =
        outcome.stable_receiver_id;
    record.composed_owners =
        outcome.composed_owners;

    const auto found =
        replacements_.find(
            outcome.plan_index);

    if (found != replacements_.end()) {
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

bool upper_lower_hemenv_draw_runtime::prepare_draw_request(
    reshade::api::command_list *cmd_list,
    const upper_lower_receiver_identity &identity,
    bool material_response_active,
    prepared_upper_lower_hemenv_draw &prepared) noexcept
{
    prepared = {};
    ++candidates_;

    if (cmd_list == nullptr ||
        !identity.valid() ||
        quarantined_.load() ||
        !core_.features().enabled(
            core::operator_id::upper_lower))
        return false;

    const bool spc =
        identity.stratum ==
        operators::lightbank::
            upper_lower_hemenv_stratum::spc;

    if (!upper_lower_identity_runtime_shape_valid(
            identity)) {
        ++identity_rejects_;
        return false;
    }

    // Until the combined MR+U/L replacement is materialized, never allow
    // two replacement pixel shaders to compete in one draw batch.
    if (spc && material_response_active) {
        ++spc_mr_hold_;
        return false;
    }

    replacement_record replacement{};
    {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto found =
            replacements_.find(
                identity.plan_index);

        if (found != replacements_.end() &&
            found->second.shader != nullptr &&
            found->second.stratum ==
                identity.stratum &&
            found->second.family ==
                identity.family &&
            found->second.stable_receiver_id ==
                identity.stable_receiver_id) {
            replacement = found->second;
            replacement.shader->AddRef();
        }
    }

    if (replacement.shader == nullptr)
        return false;

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());

    if (context == nullptr ||
        !lightbank_.prepare_upper_lower_carrier(
            context,
            prepared.carrier)) {
        replacement.shader->Release();
        ++carrier_rejects_;
        return false;
    }

    ++carrier_ready_;

    prepared.shader =
        replacement.shader;
    prepared.identity =
        identity;

    prepared.request.primary =
        core::operator_id::upper_lower;
    prepared.request.additional_owners =
        replacement.composed_owners;
    prepared.request.additional_shader_owners =
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
        return false;
    }

    prepared.ready = true;
    ++requests_;

    if (spc)
        ++spc_ready_;
    else
        ++nospc_ready_;

    switch (identity.family) {
    case operators::lightbank::upper_lower_hemenv_family::hemenv:
    case operators::lightbank::upper_lower_hemenv_family::hemenvlerp:
    case operators::lightbank::upper_lower_hemenv_family::hemenv_parallax:
    case operators::lightbank::upper_lower_hemenv_family::hemenvlerp_parallax:
    case operators::lightbank::upper_lower_hemenv_family::phn_pnts:
    case operators::lightbank::upper_lower_hemenv_family::phn_faceeye:
    case operators::lightbank::upper_lower_hemenv_family::phn_subsurf:
        ++phn_ready_;
        break;
    case operators::lightbank::upper_lower_hemenv_family::gst:
    case operators::lightbank::upper_lower_hemenv_family::gst_faceeye:
        ++gst_ready_;
        break;
    case operators::lightbank::upper_lower_hemenv_family::sfx:
        ++sfx_ready_;
        break;
    case operators::lightbank::upper_lower_hemenv_family::snow:
        ++snow_ready_;
        break;
    case operators::lightbank::upper_lower_hemenv_family::ntoa:
        ++ntoa_ready_;
        break;
    }

    return true;
}

void upper_lower_hemenv_draw_runtime::
release_prepared_draw(
    prepared_upper_lower_hemenv_draw &prepared) noexcept
{
    lightbank_.release_prepared_draw(
        prepared.carrier);

    if (prepared.shader != nullptr)
        prepared.shader->Release();

    prepared = {};
}

upper_lower_hemenv_draw_telemetry
upper_lower_hemenv_draw_runtime::telemetry() const noexcept
{
    return {
        replacement_register_ok_.load(),
        replacement_register_fail_.load(),
        candidates_.load(),
        carrier_ready_.load(),
        carrier_rejects_.load(),
        identity_rejects_.load(),
        nospc_ready_.load(),
        spc_ready_.load(),
        spc_mr_hold_.load(),
        phn_ready_.load(),
        gst_ready_.load(),
        sfx_ready_.load(),
        snow_ready_.load(),
        ntoa_ready_.load(),
        requests_.load(),
        quarantined_.load()
    };
}

void upper_lower_hemenv_draw_runtime::reset() noexcept
{
    release_resources();

    replacement_register_ok_.store(0);
    replacement_register_fail_.store(0);
    candidates_.store(0);
    carrier_ready_.store(0);
    carrier_rejects_.store(0);
    identity_rejects_.store(0);
    nospc_ready_.store(0);
    spc_ready_.store(0);
    spc_mr_hold_.store(0);
    phn_ready_.store(0);
    gst_ready_.store(0);
    sfx_ready_.store(0);
    snow_ready_.store(0);
    ntoa_ready_.store(0);
    requests_.store(0);
    quarantined_.store(false);
}

} // namespace dsrrl::runtime
