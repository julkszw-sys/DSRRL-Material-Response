#include "dsrrl/operators/resource_bridges/diffuse_bridge.hpp"

namespace dsrrl::operators::resource_bridges {
namespace {

bool receiver_supported(std::uint32_t receiver_id) noexcept
{
    return receiver_id >= 24u && receiver_id <= 35u;
}

diffuse_bridge_decision preserve(
    const diffuse_bridge_context &context,
    diffuse_reason reason) noexcept
{
    return {
        diffuse_action::preserve_existing_route,
        reason,
        context.receiver_id,
        0u,
        false,
        false
    };
}

} // namespace

diffuse_bridge_decision evaluate_diffuse_route(
    const diffuse_bridge_context &context) noexcept
{
    if (!receiver_supported(context.receiver_id))
        return preserve(context, diffuse_reason::unsupported_receiver);

    if (!context.actual_material_verified)
        return preserve(context, diffuse_reason::material_not_verified);

    if (!context.actual_bound_t0_verified)
        return preserve(context, diffuse_reason::actual_t0_not_verified);

    if (context.shared_material_route &&
        !context.exact_texture_identity_conjunction)
        return preserve(
            context,
            diffuse_reason::shared_material_texture_identity_missing);

    if (!context.exact_ptde_diffuse_companion_verified)
        return preserve(context, diffuse_reason::ptde_companion_not_verified);

    if (!context.ptde_srv_ready)
        return preserve(context, diffuse_reason::ptde_srv_not_ready);

    if (!context.ptde_c100_donor_verified)
        return preserve(context, diffuse_reason::c100_donor_not_verified);

    if (!context.diffuse_linear_receiver_ready)
        return preserve(
            context,
            diffuse_reason::diffuse_linear_receiver_not_ready);

    return {
        diffuse_action::bind_ptde_t0_and_full_material_response,
        diffuse_reason::active,
        context.receiver_id,
        0u,
        true,
        true
    };
}

} // namespace dsrrl::operators::resource_bridges
