#include "dsrrl/operators/point_light/fixed_local_specular_island_plan.hpp"

namespace dsrrl::operators::point_light {

fixed_local_specular_island_plan
build_fixed_local_specular_island_plan(
    const void *pixel_shader_code,
    std::size_t code_size) noexcept
{
    fixed_local_specular_island_plan out;
    out.output_cut =
        locate_fixed_local_specular_output_cut(
            pixel_shader_code,code_size);

    if (out.output_cut.result ==
        fixed_local_specular_output_cut_result::
            pass_not_fixed_local_specular) {
        out.result =
            fixed_local_specular_island_plan_result::
                pass_not_fixed_local_specular;
        return out;
    }

    if (out.output_cut.result !=
        fixed_local_specular_output_cut_result::exact) {
        out.result =
            fixed_local_specular_island_plan_result::
                fail_output_cut;
        return out;
    }

    const auto &operands=out.output_cut.operands;
    if (operands.light_count != 2u &&
        operands.light_count != 4u) {
        out.result =
            fixed_local_specular_island_plan_result::
                fail_light_count;
        return out;
    }

    if (!operands.exponent_carrier_attested ||
        operands.ptde_specular_power_cb_slot != 12u ||
        operands.ptde_specular_power_cb_index != 0u ||
        operands.ptde_specular_power_component != 3u) {
        out.result =
            fixed_local_specular_island_plan_result::
                fail_exponent_carrier;
        return out;
    }

    out.light_count=operands.light_count;
    for(std::uint8_t i=0u;i<out.light_count;++i) {
        if (emit_fixed_local_specular_legacy_kernel(
                operands.lights[i],
                out.angular[i]) !=
            fixed_local_specular_legacy_kernel_result::exact) {
            out.result =
                fixed_local_specular_island_plan_result::
                    fail_angular_kernel;
            return out;
        }
    }

    out.raw_q_srv_slot=operands.plan.raw_q_srv_slot;
    out.material_cb_slot=operands.plan.material_cb_slot;
    out.ptde_specular_power_cb_index=
        operands.ptde_specular_power_cb_index;
    out.ptde_specular_power_component=
        operands.ptde_specular_power_component;
    out.replace_complete_local_specular=true;
    out.stock_microfacet_dead_at_output_cut=true;
    out.preserve_downstream_fog=true;
    out.result=fixed_local_specular_island_plan_result::ready;
    return out;
}

} // namespace dsrrl::operators::point_light
