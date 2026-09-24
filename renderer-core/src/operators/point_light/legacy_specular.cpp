#include "dsrrl/operators/point_light/legacy_specular.hpp"

#include <algorithm>
#include <cmath>

namespace dsrrl::operators::point_light {
namespace {

bool finite(const pointlight_vec3 &v) noexcept
{
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
}

pointlight_vec3 mul(
    const pointlight_vec3 &a,
    const pointlight_vec3 &b) noexcept
{
    return {a.x*b.x,a.y*b.y,a.z*b.z};
}

pointlight_vec3 scale(const pointlight_vec3 &a,float s) noexcept
{
    return {a.x*s,a.y*s,a.z*s};
}

pointlight_vec3 sub(const pointlight_vec3 &a,const pointlight_vec3 &b) noexcept
{
    return {a.x-b.x,a.y-b.y,a.z-b.z};
}

float dot(const pointlight_vec3 &a,const pointlight_vec3 &b) noexcept
{
    return a.x*b.x+a.y*b.y+a.z*b.z;
}

float length_sq(const pointlight_vec3 &v) noexcept
{
    return dot(v,v);
}

legacy_specular_sample evaluate_from_rdotl(
    const legacy_specular_input &input,
    const pointlight_vec3 &reflection) noexcept
{
    legacy_specular_sample out;
    if(!finite(input.source_rgb) ||
       !std::isfinite(input.attenuation) ||
       !std::isfinite(input.r_dot_l) ||
       !std::isfinite(input.exponent_c102) ||
       !finite(input.spec_texture_blend) ||
       !std::isfinite(input.c101) ||
       !finite(input.color0))
        return out;

    if(input.exponent_c102<=0.0f){
        out.result=legacy_specular_math_result::fail_open_invalid_exponent;
        return out;
    }

    out.material_specular=scale(
        mul(input.spec_texture_blend,input.color0),
        input.c101);
    out.reflection=reflection;
    out.r_dot_l=input.r_dot_l;

    const float r=std::max(input.r_dot_l,0.0f);
    out.angular=std::pow(r,input.exponent_c102);

    if(!std::isfinite(out.angular))
        return legacy_specular_sample{};

    out.specular=mul(
        scale(input.source_rgb,input.attenuation*out.angular),
        out.material_specular);
    out.result=legacy_specular_math_result::exact;
    return out;
}

} // namespace

legacy_specular_sample evaluate_legacy_local_specular(
    const legacy_specular_input &input) noexcept
{
    return evaluate_from_rdotl(input,{});
}

legacy_specular_sample evaluate_legacy_local_specular(
    const legacy_specular_vector_input &input) noexcept
{
    if(!finite(input.source_rgb) ||
       !std::isfinite(input.attenuation) ||
       !finite(input.normal) ||
       !finite(input.view_direction) ||
       !finite(input.light_direction) ||
       !std::isfinite(input.exponent_c102) ||
       !finite(input.spec_texture_blend) ||
       !std::isfinite(input.c101) ||
       !finite(input.color0))
        return {};

    // The replacement window requires normalized N/V/L. Do not silently
    // normalize arbitrary host intermediates: a bad carrier must fail open.
    constexpr float unit_tolerance=0.0025f;
    const float n2=length_sq(input.normal);
    const float v2=length_sq(input.view_direction);
    const float l2=length_sq(input.light_direction);
    if(std::fabs(n2-1.0f)>unit_tolerance ||
       std::fabs(v2-1.0f)>unit_tolerance ||
       std::fabs(l2-1.0f)>unit_tolerance){
        legacy_specular_sample out;
        out.result=legacy_specular_math_result::fail_open_invalid_vector;
        return out;
    }

    const float n_dot_v=dot(input.normal,input.view_direction);
    const pointlight_vec3 reflection=
        sub(scale(input.normal,2.0f*n_dot_v),input.view_direction);
    const float r_dot_l=dot(reflection,input.light_direction);

    legacy_specular_input scalar{};
    scalar.source_rgb=input.source_rgb;
    scalar.attenuation=input.attenuation;
    scalar.r_dot_l=r_dot_l;
    scalar.exponent_c102=input.exponent_c102;
    scalar.spec_texture_blend=input.spec_texture_blend;
    scalar.c101=input.c101;
    scalar.color0=input.color0;
    return evaluate_from_rdotl(scalar,reflection);
}

local_specular_runtime_plan evaluate_local_specular_runtime_readiness(
    const core::feature_registry &features,
    const core::activation_context &activation,
    const local_specular_runtime_context &context) noexcept
{
    local_specular_runtime_plan out;
    const auto gate=core::evaluate_operator_activation(
        features,core::operator_id::local_specular_legacy,activation);
    if(gate.state!=core::island_state::active){
        out.reason=local_specular_runtime_reason::core_gate_not_active;
        return out;
    }
    if(!context.receiver_verified){
        out.reason=local_specular_runtime_reason::receiver_not_verified;
        return out;
    }
    if(context.receiver_class==local_specular_receiver_class::unsupported){
        out.reason=local_specular_runtime_reason::unsupported_receiver;
        return out;
    }
    if(!context.material_c101_c102_verified){
        out.reason=local_specular_runtime_reason::material_terms_not_verified;
        return out;
    }
    if(!context.spec_rgb_route_ready){
        out.reason=local_specular_runtime_reason::spec_rgb_route_not_ready;
        return out;
    }
    if(!context.source_amplitude_category_ready){
        out.reason=local_specular_runtime_reason::source_amplitude_not_ready;
        return out;
    }
    if(!context.attenuation_ready){
        out.reason=local_specular_runtime_reason::attenuation_not_ready;
        return out;
    }

    if(context.receiver_class==local_specular_receiver_class::clustered_spc_pnts){
        if(!context.clustered_membership_sidecar_ready){
            out.reason=
                local_specular_runtime_reason::
                    clustered_membership_sidecar_not_ready;
            return out;
        }
        if(!context.clustered_four_slot_shader_ready){
            out.reason=
                local_specular_runtime_reason::
                    clustered_four_slot_shader_not_ready;
            return out;
        }
        if(!context.stock_cluster_membership_bypassed){
            out.reason=
                local_specular_runtime_reason::
                    stock_cluster_membership_not_bypassed;
            return out;
        }
    }else if(!context.fixed_membership_verified){
        out.reason=
            local_specular_runtime_reason::fixed_membership_not_verified;
        return out;
    }

    if(!context.complete_microfacet_window_owned){
        out.reason=local_specular_runtime_reason::microfacet_window_not_owned;
        return out;
    }
    if(!context.stock_roughness_tail_bypassed){
        out.reason=local_specular_runtime_reason::roughness_tail_not_bypassed;
        return out;
    }
    if(!context.stock_common_ndotl_specular_bypassed){
        out.reason=local_specular_runtime_reason::common_ndotl_tail_not_bypassed;
        return out;
    }
    if(!context.diffuse_path_preserved){
        out.reason=local_specular_runtime_reason::diffuse_path_not_preserved;
        return out;
    }
    if(!context.separate_specular_output_cut_ready){
        out.reason=
            local_specular_runtime_reason::separate_output_cut_not_ready;
        return out;
    }
    if(!context.draw_transaction_ready){
        out.reason=
            local_specular_runtime_reason::draw_transaction_not_ready;
        return out;
    }

    out.ready=true;
    out.reason=local_specular_runtime_reason::ready;
    return out;
}

} // namespace dsrrl::operators::point_light
