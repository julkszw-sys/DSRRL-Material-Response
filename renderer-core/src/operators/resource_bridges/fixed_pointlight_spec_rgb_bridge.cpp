#include "dsrrl/operators/resource_bridges/fixed_pointlight_spec_rgb_bridge.hpp"

namespace dsrrl::operators::resource_bridges {
namespace {

fixed_pointlight_spec_rgb_decision fail(
    fixed_pointlight_spec_rgb_reason reason) noexcept
{
    fixed_pointlight_spec_rgb_decision out;
    out.reason = reason;
    return out;
}

} // namespace

fixed_pointlight_spec_rgb_decision
evaluate_fixed_pointlight_spec_rgb_route(
    const fixed_pointlight_spec_rgb_context &context,
    const material_response::mtd_semantic_query &query) noexcept
{
    if (!context.exact_fixed_receiver_verified)
        return fail(
            fixed_pointlight_spec_rgb_reason::
                receiver_not_verified);

    if (!context.actual_material_verified)
        return fail(
            fixed_pointlight_spec_rgb_reason::
                material_not_verified);

    const auto semantic =
        material_response::classify_mtd_semantic(
            query,
            material_response::mtd_semantic_operator::spec_rgb);

    if (semantic.state !=
        material_response::mtd_semantic_state::use)
        return fail(
            fixed_pointlight_spec_rgb_reason::
                mtd_census_not_authorized);

    if (!context.material_specular_consumer_verified)
        return fail(
            fixed_pointlight_spec_rgb_reason::
                no_specular_consumer);

    if (!context.endpoint_a_identity_verified)
        return fail(
            fixed_pointlight_spec_rgb_reason::
                endpoint_a_not_verified);

    if (!context.endpoint_a_sidecar_ready)
        return fail(
            fixed_pointlight_spec_rgb_reason::
                endpoint_a_sidecar_not_ready);

    if (context.blended_material) {
        if (!context.endpoint_b_identity_verified)
            return fail(
                fixed_pointlight_spec_rgb_reason::
                    endpoint_b_not_verified);
        if (!context.endpoint_b_sidecar_ready)
            return fail(
                fixed_pointlight_spec_rgb_reason::
                    endpoint_b_sidecar_not_ready);
    }

    if (!context.t10_t16_transport_ready)
        return fail(
            fixed_pointlight_spec_rgb_reason::
                transport_not_ready);

    if (!context.stock_t1_t4_preserved)
        return fail(
            fixed_pointlight_spec_rgb_reason::
                stock_endpoints_not_preserved);

    fixed_pointlight_spec_rgb_decision out;
    out.reason =
        fixed_pointlight_spec_rgb_reason::active;
    out.action =
        context.blended_material
            ? fixed_pointlight_spec_rgb_action::
                bind_blended_t10_t16
            : fixed_pointlight_spec_rgb_action::
                bind_single_t10;
    return out;
}

} // namespace dsrrl::operators::resource_bridges
