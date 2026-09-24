#pragma once

#include "dsrrl/core/island_policy.hpp"

#include <cstdint>

namespace dsrrl::operators::point_light {

struct pointlight_vec3 { float x=0.0f; float y=0.0f; float z=0.0f; };

enum class legacy_specular_math_result : std::uint8_t {
    exact=0, fail_open_nonfinite_input, fail_open_invalid_exponent,
    fail_open_invalid_vector
};

struct legacy_specular_input {
    pointlight_vec3 source_rgb{}; float attenuation=0.0f; float r_dot_l=0.0f;
    float exponent_c102=0.0f; pointlight_vec3 spec_texture_blend{};
    float c101=1.0f; pointlight_vec3 color0{};
};

struct legacy_specular_vector_input {
    pointlight_vec3 source_rgb{}; float attenuation=0.0f;
    pointlight_vec3 normal{}; pointlight_vec3 view_direction{};
    pointlight_vec3 light_direction{}; float exponent_c102=0.0f;
    pointlight_vec3 spec_texture_blend{}; float c101=1.0f;
    pointlight_vec3 color0{};
};

struct legacy_specular_sample {
    legacy_specular_math_result result=legacy_specular_math_result::fail_open_nonfinite_input;
    pointlight_vec3 material_specular{}; pointlight_vec3 reflection{};
    float r_dot_l=0.0f; float angular=0.0f; pointlight_vec3 specular{};
};

// Fixture seam for already-proven RdotL values.
legacy_specular_sample evaluate_legacy_local_specular(const legacy_specular_input &input) noexcept;

// Production replacement-window evaluator. Owns R=2*dot(N,V)*N-V and RdotL;
// no DSR GGX/Schlick, SpecTex-alpha roughness or common-NdotL specular tail.
legacy_specular_sample evaluate_legacy_local_specular(const legacy_specular_vector_input &input) noexcept;

enum class local_specular_receiver_class : std::uint8_t {
    unsupported=0, fixed_spc_pntss, fixed_spc_pntssss, clustered_pnts
};

enum class local_specular_runtime_reason : std::uint8_t {
    ready=0, core_gate_not_active, receiver_not_verified, unsupported_receiver,
    material_terms_not_verified, spec_rgb_route_not_ready, source_amplitude_not_ready,
    attenuation_not_ready, fixed_membership_not_verified,
    clustered_membership_sidecar_not_ready, clustered_four_slot_shader_not_ready,
    stock_cluster_membership_not_bypassed, microfacet_window_not_owned,
    roughness_tail_not_bypassed, common_ndotl_tail_not_bypassed,
    diffuse_path_not_preserved, separate_output_cut_not_ready,
    draw_transaction_not_ready
};

struct local_specular_runtime_context {
    bool receiver_verified=false;
    local_specular_receiver_class receiver_class=local_specular_receiver_class::unsupported;
    bool material_c101_c102_verified=false; bool spec_rgb_route_ready=false;
    bool source_amplitude_category_ready=false; bool attenuation_ready=false;
    bool fixed_membership_verified=false; bool clustered_membership_sidecar_ready=false;
    bool clustered_four_slot_shader_ready=false; bool stock_cluster_membership_bypassed=false;
    bool complete_microfacet_window_owned=false;
    bool stock_roughness_tail_bypassed=false;
    bool stock_common_ndotl_specular_bypassed=false;
    bool diffuse_path_preserved=false; bool separate_specular_output_cut_ready=false;
    bool draw_transaction_ready=false;
};

struct local_specular_runtime_plan {
    bool ready=false;
    local_specular_runtime_reason reason=local_specular_runtime_reason::core_gate_not_active;
    bool preserve_dsr_diffuse=true; bool use_ptde_legacy_specular=true;
    bool bypass_dsr_microfacet=true; bool bypass_dsr_roughness_tail=true;
    bool common_ndotl_on_specular=false;
};

local_specular_runtime_plan evaluate_local_specular_runtime_readiness(
    const core::feature_registry &features,
    const core::activation_context &activation,
    const local_specular_runtime_context &context) noexcept;

} // namespace dsrrl::operators::point_light
