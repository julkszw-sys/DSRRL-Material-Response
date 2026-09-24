#include "dsrrl/core/renderer_core.hpp"

namespace dsrrl::core {

render_patch_plan renderer_core::build_plan(
    const draw_context &context,
    const island_request *requests,
    std::uint32_t request_count) const
{
    render_patch_plan plan;

    if (requests == nullptr || request_count == 0)
        return plan;

    const auto receiver = receivers_.resolve(
        context.shader_fast_hash,
        context.shader_sha256);

    if (!receiver.has_value())
        return plan;

    // Exact receiver resolution is owned by Core. All remaining operator-local
    // requirements must still pass the canonical activation policy before an
    // island can enter a patch plan. This prevents a future caller from
    // bypassing BLOCKED/REJECTED/diagnostic/material/resource/producer/
    // consumer/context/graph gates by calling build_plan directly.
    auto activation = context.activation;
    activation.receiver_verified = true;
    activation.immediate_context =
        context.context == context_kind::immediate;

    for (std::uint32_t i = 0; i < request_count; ++i) {
        const auto &request = requests[i];

        const auto gate =
            evaluate_operator_activation(
                features_,
                request.op,
                activation);

        if (gate.state != island_state::active)
            continue;

        if ((receiver->capabilities & operator_bit(request.op)) == 0)
            continue;

        if (plan.patch_count >= plan.patches.size())
            break;

        if ((plan.carrier_write_mask & request.carrier_write_mask) != 0)
            continue;

        plan.patches[plan.patch_count++] = island_patch{
            request.op,
            request.carrier_write_mask,
            request.shader_replacement,
            request.resource_replacement
        };
        plan.carrier_write_mask |= request.carrier_write_mask;
    }

    return plan;
}

bool renderer_core::phase0_pass_through() const noexcept
{
    return features_.all_disabled() &&
           hooks_.empty() &&
           transactions_.empty();
}

} // namespace dsrrl::core
