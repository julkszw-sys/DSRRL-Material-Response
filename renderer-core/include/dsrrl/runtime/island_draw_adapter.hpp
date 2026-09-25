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

struct island_draw_dispatch_result {
    island_draw_adapter_result adapter =
        island_draw_adapter_result::wrong_transaction_mode;
    draw_tx_result transaction =
        draw_tx_result::not_issued;
};

enum class island_draw_batch_result : std::uint8_t {
    ready = 0,
    adapter_rejected,
    shader_conflict,
    constant_buffer_conflict,
    srv_conflict,
    sampler_conflict,
    capacity_exceeded
};

struct island_draw_batch {
    draw_tx_mutation mutation{};
    std::uint32_t island_count = 0;
};

island_draw_batch_result append_island_draw_request(
    island_draw_batch &batch,
    const island_draw_adapter_request &request) noexcept;

island_draw_dispatch_result dispatch_island_draw_batch(
    draw_state_transaction_runtime &transactions,
    reshade::api::command_list *cmd_list,
    const island_draw_batch &batch,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance) noexcept;

island_draw_dispatch_result dispatch_island_draw_indexed_batch(
    draw_state_transaction_runtime &transactions,
    reshade::api::command_list *cmd_list,
    const island_draw_batch &batch,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept;

island_draw_dispatch_result dispatch_island_draw(
    draw_state_transaction_runtime &transactions,
    reshade::api::command_list *cmd_list,
    const island_draw_adapter_request &request,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance) noexcept;

island_draw_dispatch_result dispatch_island_draw_indexed(
    draw_state_transaction_runtime &transactions,
    reshade::api::command_list *cmd_list,
    const island_draw_adapter_request &request,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept;

} // namespace dsrrl::runtime
