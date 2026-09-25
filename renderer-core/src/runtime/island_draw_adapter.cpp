#include "dsrrl/runtime/island_draw_adapter.hpp"

namespace dsrrl::runtime {
namespace {

std::uint32_t request_mutation_mask(
    const island_draw_adapter_request &request) noexcept
{
    std::uint32_t mask = core::draw_mutation_none;

    if (request.replace_pixel_shader)
        mask |= core::draw_mutation_shader;
    if (request.constant_buffer_count != 0u)
        mask |= core::draw_mutation_constant_buffer;
    if (request.srv_count != 0u)
        mask |= core::draw_mutation_srv;
    if (request.sampler_count != 0u)
        mask |= core::draw_mutation_sampler;

    return mask;
}

} // namespace

island_draw_adapter_result build_island_draw_mutation(
    const island_draw_adapter_request &request,
    draw_tx_mutation &mutation) noexcept
{
    mutation = {};

    if (!core::valid_operator_id(request.primary))
        return island_draw_adapter_result::wrong_transaction_mode;

    const auto &policy =
        core::draw_policy(request.primary);

    if (policy.mode !=
        core::draw_transaction_mode::draw_required)
        return island_draw_adapter_result::wrong_transaction_mode;

    if (policy.exact_receiver_gate &&
        !request.receiver_verified)
        return island_draw_adapter_result::receiver_gate_missing;

    if (policy.exact_material_gate &&
        !request.material_verified)
        return island_draw_adapter_result::material_gate_missing;

    if ((request.additional_owners &
         ~core::all_operator_bits) != 0u ||
        (request.additional_owners &
         core::operator_bit(request.primary)) != 0u)
        return island_draw_adapter_result::invalid_additional_owner;

    if (request.constant_buffer_count > draw_tx_max_cb ||
        request.srv_count > draw_tx_max_srv ||
        request.sampler_count > draw_tx_max_sampler)
        return island_draw_adapter_result::mutation_not_allowed;

    const std::uint32_t actual =
        request_mutation_mask(request);

    if (actual == core::draw_mutation_none)
        return island_draw_adapter_result::empty_mutation;

    if ((actual & ~policy.allowed_mutation_mask) != 0u)
        return island_draw_adapter_result::mutation_not_allowed;

    if ((actual & policy.required_mutation_mask) !=
        policy.required_mutation_mask)
        return island_draw_adapter_result::required_mutation_missing;

    mutation.owners =
        core::operator_bit(request.primary) |
        request.additional_owners;

    mutation.pixel_shader =
        request.pixel_shader;
    mutation.replace_pixel_shader =
        request.replace_pixel_shader;

    mutation.constant_buffers =
        request.constant_buffers;
    mutation.constant_buffer_count =
        request.constant_buffer_count;

    mutation.srvs = request.srvs;
    mutation.srv_count = request.srv_count;

    mutation.samplers = request.samplers;
    mutation.sampler_count =
        request.sampler_count;

    return island_draw_adapter_result::ready;
}

island_draw_dispatch_result dispatch_island_draw(
    draw_state_transaction_runtime &transactions,
    reshade::api::command_list *cmd_list,
    const island_draw_adapter_request &request,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance) noexcept
{
    island_draw_dispatch_result out{};
    draw_tx_mutation mutation{};

    out.adapter =
        build_island_draw_mutation(
            request,
            mutation);

    if (out.adapter !=
        island_draw_adapter_result::ready)
        return out;

    out.transaction =
        transactions.replay_draw(
            cmd_list,
            mutation,
            vertex_count,
            instance_count,
            first_vertex,
            first_instance);

    return out;
}

island_draw_dispatch_result dispatch_island_draw_indexed(
    draw_state_transaction_runtime &transactions,
    reshade::api::command_list *cmd_list,
    const island_draw_adapter_request &request,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept
{
    island_draw_dispatch_result out{};
    draw_tx_mutation mutation{};

    out.adapter =
        build_island_draw_mutation(
            request,
            mutation);

    if (out.adapter !=
        island_draw_adapter_result::ready)
        return out;

    out.transaction =
        transactions.replay_draw_indexed(
            cmd_list,
            mutation,
            index_count,
            instance_count,
            first_index,
            vertex_offset,
            first_instance);

    return out;
}

} // namespace dsrrl::runtime
