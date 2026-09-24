#include "dsrrl/core/island_policy.hpp"

namespace dsrrl::core {

activation_decision evaluate_operator_activation(
    const feature_registry &features,
    operator_id id,
    const activation_context &context) noexcept
{
    const auto contract = find_operator_contract(id);
    if (!contract.has_value())
        return {island_state::fail_open, activation_reason::blocked};

    if (contract->status == canonical_status::rejected)
        return {island_state::fail_open, activation_reason::blocked};

    if (contract->default_state == port_state::stock_host)
        return {island_state::fail_open, activation_reason::stock_host_preserved};

    if (contract->default_state == port_state::blocked)
        return {island_state::fail_open, activation_reason::blocked};

    if (contract->default_state == port_state::diagnostic && !context.allow_diagnostic)
        return {island_state::fail_open, activation_reason::diagnostic_not_allowed};

    if (!features.enabled(id))
        return {island_state::disabled, activation_reason::disabled};

    const std::uint32_t req = contract->requirements;
    if ((req & require_receiver) != 0 && !context.receiver_verified)
        return {island_state::fail_open, activation_reason::missing_receiver};
    if ((req & require_material) != 0 && !context.material_verified)
        return {island_state::fail_open, activation_reason::missing_material};
    if ((req & require_resource) != 0 && !context.resource_ready)
        return {island_state::fail_open, activation_reason::missing_resource};
    if ((req & require_producer) != 0 && !context.producer_ready)
        return {island_state::fail_open, activation_reason::missing_producer};
    if ((req & require_consumer) != 0 && !context.consumer_verified)
        return {island_state::fail_open, activation_reason::missing_consumer};
    if ((req & require_immediate_context) != 0 && !context.immediate_context)
        return {island_state::fail_open, activation_reason::wrong_context};
    if ((req & require_graph) != 0 && !context.graph_ready)
        return {island_state::fail_open, activation_reason::missing_graph};

    return {island_state::active, activation_reason::active};
}

} // namespace dsrrl::core
