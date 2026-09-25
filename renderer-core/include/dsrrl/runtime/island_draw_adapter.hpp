#pragma once

#include "dsrrl/core/draw_transaction_policy.hpp"
#include "dsrrl/runtime/draw_state_transaction.hpp"

namespace dsrrl::runtime {

enum class island_draw_adapter_result : std::uint8_t {
    ready = 0,
    wrong_transaction_mode,
    receiver_gate_missing,
    material_gate_missing,
    empty_mutation,
    mutation_not_allowed,
    required_mutation_missing,
    invalid_additional_owner
};

struct island_draw_adapter_request {
    core::operator_id primary =
        core::operator_id::material_response;

    core::operator_mask additional_owners = 0;

    bool receiver_verified = false;
    bool material_verified = false;

    ID3D11PixelShader *pixel_shader = nullptr;
    bool replace_pixel_shader = false;

    std::array<draw_tx_cb_binding, draw_tx_max_cb> constant_buffers{};
    std::uint32_t constant_buffer_count = 0;

    std::array<draw_tx_srv_binding, draw_tx_max_srv> srvs{};
    std::uint32_t srv_count = 0;

    std::array<draw_tx_sampler_binding, draw_tx_max_sampler> samplers{};
    std::uint32_t sampler_count = 0;
};

island_draw_adapter_result build_island_draw_mutation(
    const island_draw_adapter_request &request,
    draw_tx_mutation &mutation) noexcept;

} // namespace dsrrl::runtime
