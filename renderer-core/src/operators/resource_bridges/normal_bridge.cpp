#include "dsrrl/operators/resource_bridges/normal_bridge.hpp"

namespace dsrrl::operators::resource_bridges {
namespace {

bool receiver_supported(std::uint32_t receiver_id) noexcept
{
    return receiver_id >= 24u && receiver_id <= 35u;
}

normal_bridge_decision fail(
    const normal_bridge_context &context,
    normal_reason reason) noexcept
{
    return {
        normal_action::preserve_host,
        reason,
        context.receiver_id,
        2u,
        2u,
        true
    };
}

} // namespace

normal_bridge_decision evaluate_normal_route(
    const normal_bridge_context &context) noexcept
{
    if (!receiver_supported(context.receiver_id))
        return fail(context, normal_reason::unsupported_receiver);

    bool route_authorized = false;
    switch (context.authority) {
    case normal_route_authority::homologous_material_route:
        if (!context.homologous_bmp_material_route)
            return fail(context, normal_reason::nonhomologous_route);
        route_authorized = true;
        break;

    case normal_route_authority::safe_exact_resource_tuple:
        if (!context.safe_exact_t0_t1_t2_tuple)
            return fail(context, normal_reason::route_not_authorized);
        route_authorized = true;
        break;

    case normal_route_authority::none:
    default:
        break;
    }

    if (!route_authorized)
        return fail(context, normal_reason::route_not_authorized);

    if (!context.actual_bound_t2_verified)
        return fail(context, normal_reason::actual_t2_not_verified);

    if (!context.target_unambiguous)
        return fail(context, normal_reason::ambiguous_target);

    if (!context.exact_ptde_normal_sidecar_verified)
        return fail(context, normal_reason::ptde_sidecar_not_verified);

    if (!context.ptde_srv_ready)
        return fail(context, normal_reason::ptde_srv_not_ready);

    if (!context.preserve_stock_s2)
        return fail(context, normal_reason::sampler_contract_not_preserved);

    return {
        normal_action::bind_ptde_t2,
        normal_reason::active,
        context.receiver_id,
        2u,
        2u,
        true
    };
}

} // namespace dsrrl::operators::resource_bridges
