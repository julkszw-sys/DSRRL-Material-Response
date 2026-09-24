#include "dsrrl/operators/resource_bridges/spec_rgb_bridge.hpp"

namespace dsrrl::operators::resource_bridges {
namespace {

spec_rgb_receiver_family classify_receiver(std::uint32_t receiver_id) noexcept
{
    if (receiver_id >= 24u && receiver_id <= 35u)
        return spec_rgb_receiver_family::dif_spc_bmp;
    if (receiver_id >= 36u && receiver_id <= 47u)
        return spec_rgb_receiver_family::dif_spc;
    return spec_rgb_receiver_family::unsupported;
}

spec_rgb_decision fail(
    const spec_rgb_context &context,
    spec_rgb_receiver_family family,
    spec_rgb_reason reason) noexcept
{
    return {
        spec_rgb_action::preserve_host,
        reason,
        family,
        context.receiver_id,
        10u,
        true
    };
}

} // namespace

spec_rgb_decision evaluate_spec_rgb_route(
    const spec_rgb_context &context) noexcept
{
    const spec_rgb_receiver_family family = classify_receiver(context.receiver_id);
    if (family == spec_rgb_receiver_family::unsupported)
        return fail(context, family, spec_rgb_reason::unsupported_receiver);

    if (!context.actual_material_verified)
        return fail(context, family, spec_rgb_reason::material_not_verified);

    if (!context.material_specular_consumer_verified)
        return fail(context, family, spec_rgb_reason::no_specular_consumer);

    if (!context.exact_name_ptde_companion_verified)
        return fail(context, family, spec_rgb_reason::ptde_companion_not_verified);

    if (!context.ptde_sidecar_ready)
        return fail(context, family, spec_rgb_reason::sidecar_not_ready);

    if (!context.native_t10_transport_ready)
        return fail(context, family, spec_rgb_reason::t10_transport_not_ready);

    if (!context.stock_t1_preserved)
        return fail(context, family, spec_rgb_reason::stock_t1_not_preserved);

    return {
        spec_rgb_action::bind_ptde_t10_rgb,
        spec_rgb_reason::active,
        family,
        context.receiver_id,
        10u,
        true
    };
}

} // namespace dsrrl::operators::resource_bridges
