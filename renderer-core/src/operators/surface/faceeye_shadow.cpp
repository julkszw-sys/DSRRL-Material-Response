#include "dsrrl/operators/surface/faceeye_shadow.hpp"

#include <algorithm>
#include <cmath>

namespace dsrrl::operators::surface {
namespace {

bool finite(const faceeye_vec3 &v) noexcept
{
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
}

float dot(const faceeye_vec3 &a,const faceeye_vec3 &b) noexcept
{
    return a.x*b.x+a.y*b.y+a.z*b.z;
}

float sat(float x) noexcept
{
    return std::max(0.0f,std::min(1.0f,x));
}

bool is_csd(faceeye_receiver_variant v) noexcept
{
    return v==faceeye_receiver_variant::csd_no_point ||
           v==faceeye_receiver_variant::csd_pnts ||
           v==faceeye_receiver_variant::csd_pntss ||
           v==faceeye_receiver_variant::csd_pntssss;
}

bool stock_regular_sampler_family(faceeye_receiver_variant v) noexcept
{
    return v==faceeye_receiver_variant::sdw_pntss ||
           v==faceeye_receiver_variant::csd_pntss ||
           v==faceeye_receiver_variant::sdw_pntssss ||
           v==faceeye_receiver_variant::csd_pntssss;
}

} // namespace

float decode_faceeye_packed_depth(const faceeye_vec3 &sample_rgb) noexcept
{
    if(!finite(sample_rgb))
        return 0.0f;
    return sample_rgb.x*(255.0f/256.0f)+
           sample_rgb.y*(255.0f/65536.0f)+
           sample_rgb.z*(255.0f/16777216.0f);
}

faceeye_shadow_sample evaluate_faceeye_shadow_response(
    const faceeye_shadow_input &input) noexcept
{
    faceeye_shadow_sample out;
    if(!std::isfinite(input.pcf16) ||
       !finite(input.normal) ||
       !finite(input.c175_direction) ||
       !std::isfinite(input.c121_bias) ||
       !std::isfinite(input.c121_normal_scale) ||
       !std::isfinite(input.c121_fade_start) ||
       !std::isfinite(input.c121_fade_scale) ||
       !std::isfinite(input.view_length) ||
       !finite(input.c122_shadow_rgb))
        return out;

    out.normal_term=sat(
        (dot(input.c175_direction,input.normal)+input.c121_bias)*
        input.c121_normal_scale);
    out.shadow_s=sat(input.pcf16+out.normal_term);
    out.fade=sat(
        (input.c121_fade_start-input.view_length)*
        input.c121_fade_scale);
    const float k=out.shadow_s*out.fade;
    out.shadow_rgb={
        1.0f-k*input.c122_shadow_rgb.x,
        1.0f-k*input.c122_shadow_rgb.y,
        1.0f-k*input.c122_shadow_rgb.z
    };
    out.result=faceeye_shadow_math_result::exact;
    return out;
}

faceeye_runtime_plan evaluate_faceeye_runtime_readiness(
    const core::feature_registry &features,
    const core::activation_context &activation,
    const faceeye_runtime_context &context) noexcept
{
    faceeye_runtime_plan out;
    const auto gate=core::evaluate_operator_activation(
        features,core::operator_id::faceeye_shadow_legacy,activation);
    if(gate.state!=core::island_state::active){
        out.reason=faceeye_runtime_reason::core_gate_not_active;
        return out;
    }
    if(!context.receiver_verified){
        out.reason=faceeye_runtime_reason::receiver_not_verified;
        return out;
    }
    if(context.variant==faceeye_receiver_variant::unsupported){
        out.reason=faceeye_runtime_reason::unsupported_receiver;
        return out;
    }
    if(!context.runtime_t7_identity_verified){
        out.reason=faceeye_runtime_reason::runtime_t7_identity_not_verified;
        return out;
    }
    if(!faceeye_auxiliary_snapshot_complete(
           context.auxiliary_dirlight_snapshot)){
        out.reason=faceeye_runtime_reason::auxiliary_dirlight_snapshot_not_ready;
        return out;
    }
    if(!context.auxiliary_dirlight_value_homology_verified){
        out.reason=
            faceeye_runtime_reason::
                auxiliary_dirlight_value_homology_not_verified;
        return out;
    }
    if(is_csd(context.variant) && !context.csd_matrix_region_ready){
        out.reason=faceeye_runtime_reason::csd_matrix_region_not_ready;
        return out;
    }

    const bool native_regular=stock_regular_sampler_family(context.variant);
    if(native_regular){
        out.use_stock_ptde_style_kernel=true;
        if(!context.stock_ptde_kernel_identity_verified){
            out.reason=
                faceeye_runtime_reason::stock_ptde_kernel_identity_not_verified;
            return out;
        }
        if(!context.stock_regular_s7_verified){
            out.reason=faceeye_runtime_reason::stock_regular_s7_not_verified;
            return out;
        }
    }else{
        out.replace_comparison_kernel=true;
        if(!context.replacement_ptde_kernel_shader_ready){
            out.reason=
                faceeye_runtime_reason::
                    replacement_ptde_kernel_shader_not_ready;
            return out;
        }
        out.override_s7_with_regular_sampler=true;
        if(!context.regular_s7_sampler_ready){
            out.reason=faceeye_runtime_reason::regular_s7_sampler_not_ready;
            return out;
        }
        if(!context.regular_s7_descriptor_verified){
            out.reason=faceeye_runtime_reason::regular_s7_descriptor_not_verified;
            return out;
        }
        if(!context.sampler_override_transaction_ready){
            out.reason=
                faceeye_runtime_reason::sampler_override_transaction_not_ready;
            return out;
        }
    }

    if(!context.legacy_env_probe_routes_ready){
        out.reason=faceeye_runtime_reason::legacy_env_probe_routes_not_ready;
        return out;
    }
    if(!context.independent_lighting_exclusion_verified){
        out.reason=
            faceeye_runtime_reason::independent_lighting_exclusion_not_verified;
        return out;
    }
    if(!context.draw_transaction_ready){
        out.reason=faceeye_runtime_reason::draw_transaction_not_ready;
        return out;
    }

    out.ready=true;
    out.reason=faceeye_runtime_reason::ready;
    return out;
}

} // namespace dsrrl::operators::surface
