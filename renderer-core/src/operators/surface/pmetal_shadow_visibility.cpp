#include "dsrrl/operators/surface/pmetal_shadow_visibility.hpp"

namespace dsrrl::operators::surface {

bool pmetal_shadow_sampler_matches_ptde(
    const pmetal_shadow_sampler_descriptor &observed) noexcept
{
    return observed.comparison==k_ptde_pmetal_shadow_s7.comparison &&
           observed.min_filter==k_ptde_pmetal_shadow_s7.min_filter &&
           observed.mag_filter==k_ptde_pmetal_shadow_s7.mag_filter &&
           observed.mip_filter==k_ptde_pmetal_shadow_s7.mip_filter &&
           observed.address_u==k_ptde_pmetal_shadow_s7.address_u &&
           observed.address_v==k_ptde_pmetal_shadow_s7.address_v &&
           observed.address_w==k_ptde_pmetal_shadow_s7.address_w &&
           observed.border_color==k_ptde_pmetal_shadow_s7.border_color &&
           observed.max_anisotropy==
               k_ptde_pmetal_shadow_s7.max_anisotropy &&
           observed.ptde_d3d9_max_mip_level==
               k_ptde_pmetal_shadow_s7.ptde_d3d9_max_mip_level;
}

pmetal_shadow_receiver_contract pmetal_shadow_contract_for_shader(
    std::uint32_t shader_index) noexcept
{
    pmetal_shadow_receiver_contract out;

    switch(shader_index){
    case 932u:
        out.variant=pmetal_shadow_receiver_variant::plain_no_shadow;
        return out;

    case 894u:
        out.variant=pmetal_shadow_receiver_variant::csd_no_point;
        break;
    case 913u:
        out.variant=pmetal_shadow_receiver_variant::sdw_no_point;
        break;
    case 904u:
        out.variant=pmetal_shadow_receiver_variant::csd_pnts;
        break;
    case 923u:
        out.variant=pmetal_shadow_receiver_variant::sdw_pnts;
        break;

    case 905u:
        out.variant=pmetal_shadow_receiver_variant::csd_pntss;
        out.stock_kernel=
            pmetal_shadow_kernel_mode::ptde_manual_packed_pcf16;
        out.shadow_operator_present=true;
        out.stock_regular_s7=true;
        return out;
    case 924u:
        out.variant=pmetal_shadow_receiver_variant::sdw_pntss;
        out.stock_kernel=
            pmetal_shadow_kernel_mode::ptde_manual_packed_pcf16;
        out.shadow_operator_present=true;
        out.stock_regular_s7=true;
        return out;
    case 906u:
        out.variant=pmetal_shadow_receiver_variant::csd_pntssss;
        out.stock_kernel=
            pmetal_shadow_kernel_mode::ptde_manual_packed_pcf16;
        out.shadow_operator_present=true;
        out.stock_regular_s7=true;
        return out;
    case 925u:
        out.variant=pmetal_shadow_receiver_variant::sdw_pntssss;
        out.stock_kernel=
            pmetal_shadow_kernel_mode::ptde_manual_packed_pcf16;
        out.shadow_operator_present=true;
        out.stock_regular_s7=true;
        return out;

    default:
        return out;
    }

    out.stock_kernel=pmetal_shadow_kernel_mode::dsr_comparison_pcf9;
    out.shadow_operator_present=true;
    out.needs_ptde_kernel_replacement=true;
    return out;
}

pmetal_shadow_runtime_plan evaluate_pmetal_shadow_runtime_readiness(
    const pmetal_shadow_runtime_context &context) noexcept
{
    pmetal_shadow_runtime_plan out;

    if(!context.receiver_verified){
        out.reason=pmetal_shadow_runtime_reason::receiver_not_verified;
        return out;
    }
    if(!context.material_pmetal_verified){
        out.reason=pmetal_shadow_runtime_reason::material_not_verified;
        return out;
    }

    const auto contract=
        pmetal_shadow_contract_for_shader(context.shader_index);
    if(contract.variant==pmetal_shadow_receiver_variant::unsupported){
        out.reason=pmetal_shadow_runtime_reason::unsupported_receiver;
        return out;
    }
    if(!contract.shadow_operator_present){
        out.reason=pmetal_shadow_runtime_reason::no_shadow_operator;
        return out;
    }
    if(!context.runtime_t7_identity_verified){
        out.reason=
            pmetal_shadow_runtime_reason::runtime_t7_identity_not_verified;
        return out;
    }
    if(!context.runtime_t7_raw_rgb_verified){
        out.reason=
            pmetal_shadow_runtime_reason::runtime_t7_raw_rgb_not_verified;
        return out;
    }
    if(!context.draw_transaction_ready){
        out.reason=pmetal_shadow_runtime_reason::draw_transaction_not_ready;
        return out;
    }

    if(contract.needs_ptde_kernel_replacement){
        if(!context.replacement_ptde_kernel_shader_ready){
            out.reason=
                pmetal_shadow_runtime_reason::replacement_shader_not_ready;
            return out;
        }
        if(!context.regular_s7_sampler_ready){
            out.reason=
                pmetal_shadow_runtime_reason::regular_s7_sampler_not_ready;
            return out;
        }
        if(!pmetal_shadow_sampler_matches_ptde(
               context.regular_s7_descriptor)){
            out.reason=
                pmetal_shadow_runtime_reason::regular_s7_descriptor_mismatch;
            return out;
        }
        if(!context.sampler_override_transaction_ready){
            out.reason=
                pmetal_shadow_runtime_reason::
                    sampler_override_transaction_not_ready;
            return out;
        }

        out.ready=true;
        out.reason=pmetal_shadow_runtime_reason::ready;
        out.replace_comparison_kernel=true;
        out.override_s7_with_ptde_regular_sampler=true;
        return out;
    }

    if(contract.stock_kernel==
       pmetal_shadow_kernel_mode::ptde_manual_packed_pcf16){
        if(!context.stock_ptde_kernel_identity_verified){
            out.reason=
                pmetal_shadow_runtime_reason::
                    stock_ptde_kernel_identity_not_verified;
            return out;
        }
        if(!context.stock_regular_s7_verified){
            out.reason=
                pmetal_shadow_runtime_reason::stock_regular_s7_not_verified;
            return out;
        }

        out.ready=true;
        out.reason=pmetal_shadow_runtime_reason::ready;
        out.preserve_stock_ptde_style_kernel=true;
        return out;
    }

    out.reason=pmetal_shadow_runtime_reason::unsupported_receiver;
    return out;
}

} // namespace dsrrl::operators::surface
