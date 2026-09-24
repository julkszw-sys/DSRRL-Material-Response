#pragma once

#include "dsrrl/core/island_policy.hpp"

#include <array>
#include <cstdint>

namespace dsrrl::operators::surface {

struct faceeye_vec3 {
    float x=0.0f;
    float y=0.0f;
    float z=0.0f;
};

enum class faceeye_shadow_math_result : std::uint8_t {
    exact=0,
    fail_open_nonfinite_input
};

struct faceeye_shadow_input {
    float pcf16=0.0f;
    faceeye_vec3 normal{};
    faceeye_vec3 c175_direction{};
    float c121_bias=0.0f;
    float c121_normal_scale=0.0f;
    float c121_fade_start=0.0f;
    float c121_fade_scale=0.0f;
    float view_length=0.0f;
    faceeye_vec3 c122_shadow_rgb{};
};

struct faceeye_shadow_sample {
    faceeye_shadow_math_result result=
        faceeye_shadow_math_result::fail_open_nonfinite_input;
    float normal_term=0.0f;
    float shadow_s=0.0f;
    float fade=0.0f;
    faceeye_vec3 shadow_rgb{1.0f,1.0f,1.0f};
};

inline constexpr std::array<float,4> k_faceeye_pcf_offsets={{
    -1.5f,-0.5f,0.5f,1.5f
}};
inline constexpr float k_faceeye_shadow_texel_scale=1.0f/2048.0f;

float decode_faceeye_packed_depth(const faceeye_vec3 &sample_rgb) noexcept;

faceeye_shadow_sample evaluate_faceeye_shadow_response(
    const faceeye_shadow_input &input) noexcept;

enum class faceeye_receiver_variant : std::uint8_t {
    unsupported=0,
    sdw_no_point,
    csd_no_point,
    sdw_pnts,
    csd_pnts,
    sdw_pntss,
    csd_pntss,
    sdw_pntssss,
    csd_pntssss
};

enum class faceeye_runtime_reason : std::uint8_t {
    ready=0,
    core_gate_not_active,
    receiver_not_verified,
    unsupported_receiver,
    ptde_kernel_shader_not_ready,
    runtime_t7_identity_not_verified,
    drawparam_roles_not_verified,
    csd_matrix_region_not_ready,
    stock_regular_s7_not_verified,
    regular_s7_sampler_not_ready,
    regular_s7_descriptor_not_verified,
    sampler_override_transaction_not_ready,
    legacy_env_probe_routes_not_ready,
    independent_lighting_exclusion_not_verified,
    draw_transaction_not_ready
};

struct faceeye_runtime_context {
    bool receiver_verified=false;
    faceeye_receiver_variant variant=faceeye_receiver_variant::unsupported;
    bool ptde_kernel_shader_ready=false;
    bool runtime_t7_identity_verified=false;
    bool drawparam_roles_verified=false;
    bool csd_matrix_region_ready=false;

    bool stock_regular_s7_verified=false;
    bool regular_s7_sampler_ready=false;
    bool regular_s7_descriptor_verified=false;
    bool sampler_override_transaction_ready=false;

    bool legacy_env_probe_routes_ready=false;
    bool independent_lighting_exclusion_verified=false;
    bool draw_transaction_ready=false;
};

struct faceeye_runtime_plan {
    bool ready=false;
    faceeye_runtime_reason reason=faceeye_runtime_reason::core_gate_not_active;
    bool keep_live_t7=true;
    bool override_s7_with_regular_sampler=false;
    bool apply_shadow_only_to_envdiffuse_envspec=true;
    bool preserve_upper_lower=true;
    bool preserve_local_pointlight=true;
};

faceeye_runtime_plan evaluate_faceeye_runtime_readiness(
    const core::feature_registry &features,
    const core::activation_context &activation,
    const faceeye_runtime_context &context) noexcept;

} // namespace dsrrl::operators::surface
