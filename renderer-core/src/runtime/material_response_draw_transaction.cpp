#include "dsrrl/runtime/material_response_draw_transaction.hpp"
#include "dsrrl/runtime/island_draw_adapter.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>

#include <array>

namespace dsrrl::runtime {
namespace {

bool full_material_response_decision(
    const operators::material_response::decision &decision) noexcept
{
    using namespace operators::material_response;

    constexpr std::uint32_t required =
        diffuse_material_domain_linear |
        specular_factor_c101;

    return
        decision.active &&
        (decision.certified_operations & required) == required;
}

} // namespace

material_response_draw_runtime::material_response_draw_runtime(
    draw_state_transaction_runtime &transactions) noexcept
    : transactions_(transactions)
{
}

material_response_draw_runtime::~material_response_draw_runtime()
{
    release_resources();
}

void material_response_draw_runtime::release_resources() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto &entry : replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    replacements_.clear();

    for (auto &entry : b12_by_route_) {
        auto *buffer = entry.second;
        if (buffer != nullptr)
            buffer->Release();
    }
    b12_by_route_.clear();

    if (device_ != nullptr) {
        device_->Release();
        device_ = nullptr;
    }
}

void material_response_draw_runtime::on_init_device(
    reshade::api::device *device) noexcept
{
    if (device == nullptr ||
        device->get_api() != reshade::api::device_api::d3d11)
        return;

    auto *native =
        reinterpret_cast<ID3D11Device *>(device->get_native());
    if (native == nullptr)
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (device_ == nullptr) {
        native->AddRef();
        device_ = native;
        return;
    }

    if (device_ != native)
        local_quarantine_.store(true);
}

void material_response_draw_runtime::on_destroy_device(
    reshade::api::device *device) noexcept
{
    if (device == nullptr ||
        device->get_api() != reshade::api::device_api::d3d11)
        return;

    auto *native =
        reinterpret_cast<ID3D11Device *>(device->get_native());

    std::lock_guard<std::mutex> lock(mutex_);
    if (native != device_)
        return;

    for (auto &entry : replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    replacements_.clear();

    for (auto &entry : b12_by_route_) {
        auto *buffer = entry.second;
        if (buffer != nullptr)
            buffer->Release();
    }
    b12_by_route_.clear();

    if (device_ != nullptr) {
        device_->Release();
        device_ = nullptr;
    }
}

bool material_response_draw_runtime::register_receiver_replacement(
    std::uint32_t receiver_id,
    const void *dxbc,
    std::size_t dxbc_size,
    core::operator_mask composed_owners) noexcept
{
    const core::operator_mask forbidden_owners =
        core::operator_bit(core::operator_id::material_response) |
        core::operator_bit(core::operator_id::diffuse_material_domain);

    if (receiver_id == 0u ||
        dxbc == nullptr ||
        dxbc_size == 0u ||
        (composed_owners & ~core::all_operator_bits) != 0u ||
        (composed_owners & forbidden_owners) != 0u ||
        local_quarantine_.load() ||
        transactions_.quarantined()) {
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
        composed_owners
    };

    const auto found = replacements_.find(receiver_id);
    if (found != replacements_.end()) {
        if (found->second.shader != nullptr)
            found->second.shader->Release();
        found->second = record;
    } else {
        replacements_.emplace(
            receiver_id,
            record);
    }

    ++replacement_register_ok_;
    return true;
}

bool material_response_draw_runtime::has_receiver_replacement(
    std::uint32_t receiver_id) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = replacements_.find(receiver_id);
    return
        found != replacements_.end() &&
        found->second.shader != nullptr;
}

bool material_response_draw_runtime::prepare_draw_request(
    const operators::material_response::decision &decision,
    prepared_material_response_draw &prepared) noexcept
{
    prepared = {};
    ++eligible_draws_;

    if (!full_material_response_decision(decision) ||
        local_quarantine_.load() ||
        transactions_.quarantined())
        return false;

    replacement_record replacement{};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found =
            replacements_.find(decision.receiver_id);

        if (found != replacements_.end() &&
            found->second.shader != nullptr) {
            replacement = found->second;
            replacement.shader->AddRef();
        }
    }

    if (replacement.shader == nullptr) {
        ++replacement_miss_;
        return false;
    }

    auto *b12 = realize_b12(decision);
    if (b12 == nullptr) {
        ++b12_bind_fail_;
        replacement.shader->Release();
        return false;
    }

    prepared.shader = replacement.shader;
    prepared.b12 = b12;
    prepared.request.primary =
        core::operator_id::material_response;
    prepared.request.additional_owners =
        core::operator_bit(
            core::operator_id::diffuse_material_domain) |
        replacement.composed_owners;
    prepared.request.receiver_verified = true;
    prepared.request.material_verified = true;
    prepared.request.pixel_shader =
        replacement.shader;
    prepared.request.replace_pixel_shader = true;
    prepared.request.constant_buffers[0] = {
        12u,
        b12
    };
    prepared.request.constant_buffer_count = 1u;

    draw_tx_mutation verify{};
    if (build_island_draw_mutation(
            prepared.request,
            verify) !=
        island_draw_adapter_result::ready) {
        ++b12_bind_fail_;
        release_prepared_draw(prepared);
        return false;
    }

    prepared.ready = true;
    return true;
}

void material_response_draw_runtime::release_prepared_draw(
    prepared_material_response_draw &prepared) noexcept
{
    if (prepared.b12 != nullptr)
        prepared.b12->Release();
    if (prepared.shader != nullptr)
        prepared.shader->Release();
    prepared = {};
}

ID3D11Buffer *material_response_draw_runtime::realize_b12(
    const operators::material_response::decision &decision) noexcept
{
    if (!full_material_response_decision(decision) ||
        local_quarantine_.load() ||
        transactions_.quarantined())
        return nullptr;

    std::lock_guard<std::mutex> lock(mutex_);
    if (device_ == nullptr)
        return nullptr;

    const auto found =
        b12_by_route_.find(decision.route_index);

    if (found != b12_by_route_.end() &&
        found->second != nullptr) {
        found->second->AddRef();
        ++b12_hit_;
        return found->second;
    }

    struct alignas(16) f4 {
        float x;
        float y;
        float z;
        float w;
    };

    const std::array<f4, 4> payload{{
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
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f}
    }};

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth =
        static_cast<UINT>(sizeof(payload));
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags =
        D3D11_BIND_CONSTANT_BUFFER;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = payload.data();

    ID3D11Buffer *buffer = nullptr;
    if (FAILED(device_->CreateBuffer(
            &desc,
            &init,
            &buffer)) ||
        buffer == nullptr)
        return nullptr;

    b12_by_route_.emplace(
        decision.route_index,
        buffer);

    buffer->AddRef();
    ++b12_create_;
    return buffer;
}

bool material_response_draw_runtime::replay_draw(
    reshade::api::command_list *cmd_list,
    const operators::material_response::decision &decision,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance) noexcept
{
    prepared_material_response_draw prepared{};
    if (!prepare_draw_request(
            decision,
            prepared))
        return false;

    const auto dispatch =
        dispatch_island_draw(
            transactions_,
            cmd_list,
            prepared.request,
            vertex_count,
            instance_count,
            first_vertex,
            first_instance);

    release_prepared_draw(prepared);

    if (dispatch.adapter !=
        island_draw_adapter_result::ready)
        return false;

    if (dispatch.transaction ==
        draw_tx_result::issued_restored) {
        ++replay_ok_;
    } else if (
        dispatch.transaction ==
        draw_tx_result::issued_restore_failed) {
        ++replay_restore_fail_;
    }

    return draw_tx_issued(
        dispatch.transaction);
}

bool material_response_draw_runtime::replay_draw_indexed(
    reshade::api::command_list *cmd_list,
    const operators::material_response::decision &decision,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept
{
    prepared_material_response_draw prepared{};
    if (!prepare_draw_request(
            decision,
            prepared))
        return false;

    const auto dispatch =
        dispatch_island_draw_indexed(
            transactions_,
            cmd_list,
            prepared.request,
            index_count,
            instance_count,
            first_index,
            vertex_offset,
            first_instance);

    release_prepared_draw(prepared);

    if (dispatch.adapter !=
        island_draw_adapter_result::ready)
        return false;

    if (dispatch.transaction ==
        draw_tx_result::issued_restored) {
        ++replay_ok_;
    } else if (
        dispatch.transaction ==
        draw_tx_result::issued_restore_failed) {
        ++replay_restore_fail_;
    }

    return draw_tx_issued(
        dispatch.transaction);
}

material_response_draw_telemetry
material_response_draw_runtime::telemetry() const noexcept
{
    return {
        replacement_register_ok_.load(),
        replacement_register_fail_.load(),
        b12_create_.load(),
        b12_hit_.load(),
        b12_bind_fail_.load(),
        eligible_draws_.load(),
        replacement_miss_.load(),
        replay_ok_.load(),
        replay_restore_fail_.load(),
        local_quarantine_.load() ||
            transactions_.quarantined()
    };
}

void material_response_draw_runtime::reset() noexcept
{
    release_resources();

    replacement_register_ok_.store(0);
    replacement_register_fail_.store(0);
    b12_create_.store(0);
    b12_hit_.store(0);
    b12_bind_fail_.store(0);
    eligible_draws_.store(0);
    replacement_miss_.store(0);
    replay_ok_.store(0);
    replay_restore_fail_.store(0);
    local_quarantine_.store(false);
}

} // namespace dsrrl::runtime
