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

    for (std::uint32_t i = 0; i < request_count; ++i) {
        const auto &request = requests[i];
        if (!features_.enabled(request.op))
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
