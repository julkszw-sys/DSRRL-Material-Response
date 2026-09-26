#include "dsrrl/operators/point_light/fixed_local_specular_patch_plan.hpp"

namespace dsrrl::operators::point_light {

fixed_local_specular_patch_plan
build_fixed_local_specular_patch_plan_from_attested(
    const local_specular_receiver_identity &identity,
    const local_specular_microfacet_window_scan &scan) noexcept
{
    fixed_local_specular_patch_plan out;
    out.identity = identity;

    if (identity.receiver_class ==
        local_specular_receiver_class::unsupported) {
        out.result =
            fixed_local_specular_plan_result::
                pass_not_local_specular_receiver;
        return out;
    }

    if (identity.receiver_class ==
        local_specular_receiver_class::clustered_spc_pnts) {
        out.result =
            fixed_local_specular_plan_result::
                pass_clustered_membership_not_owned;
        return out;
    }

    std::uint8_t expected = 0u;
    switch (identity.receiver_class) {
    case local_specular_receiver_class::fixed_spc_pntss:
        expected = 2u;
        break;
    case local_specular_receiver_class::fixed_spc_pntssss:
        expected = 4u;
        break;
    default:
        out.result =
            fixed_local_specular_plan_result::
                pass_not_local_specular_receiver;
        return out;
    }

    if (scan.result !=
            local_specular_window_result::exact) {
        out.result =
            fixed_local_specular_plan_result::
                fail_microfacet_window_scan;
        return out;
    }

    if (scan.window_count != expected) {
        out.result =
            fixed_local_specular_plan_result::
                fail_fixed_light_count;
        return out;
    }

    out.light_count = expected;
    for (std::uint8_t i = 0u; i < expected; ++i) {
        const auto &window = scan.windows[i];

        // Window ordinal is part of the fixed-light ABI proof. Never silently
        // reorder t19 or cb0 fixed slots to compensate for a scanner mismatch.
        if (window.light_ordinal != i) {
            out = {};
            out.identity = identity;
            out.result =
                fixed_local_specular_plan_result::
                    fail_fixed_light_count;
            return out;
        }

        auto &light = out.lights[i];
        light.microfacet_window = window;
        light.raw_q_t19_index = i;
        light.position_begin_cb =
            static_cast<std::uint16_t>(112u + i);
        light.color_end_cb =
            static_cast<std::uint16_t>(116u + i);
    }

    // This plan exists specifically to prevent a partial angular-kernel patch.
    // A materializer consuming it must replace the complete DSR local-specular
    // microfacet branch while preserving diffuse as an independently retained
    // path.
    out.replace_complete_microfacet_window = true;
    out.use_ptde_legacy_reflect_pow = true;
    out.consume_g_specular_power_as_exponent = true;
    out.bypass_stock_roughness_tail = true;
    out.bypass_stock_common_ndotl_specular = true;
    out.preserve_stock_diffuse = true;
    out.result =
        fixed_local_specular_plan_result::ready;
    return out;
}

fixed_local_specular_patch_plan
build_fixed_local_specular_patch_plan(
    const void *pixel_shader_code,
    std::size_t code_size) noexcept
{
    fixed_local_specular_patch_plan out;

    local_specular_receiver_identity identity{};
    if (!local_specular_receiver_for_shader(
            pixel_shader_code,
            code_size,
            identity)) {
        out.result =
            fixed_local_specular_plan_result::
                pass_not_local_specular_receiver;
        return out;
    }

    if (identity.receiver_class ==
        local_specular_receiver_class::clustered_spc_pnts) {
        out.identity = identity;
        out.result =
            fixed_local_specular_plan_result::
                pass_clustered_membership_not_owned;
        return out;
    }

    const auto scan =
        scan_local_specular_microfacet_windows(
            pixel_shader_code,
            code_size,
            identity.receiver_class);

    return build_fixed_local_specular_patch_plan_from_attested(
        identity,
        scan);
}

} // namespace dsrrl::operators::point_light
