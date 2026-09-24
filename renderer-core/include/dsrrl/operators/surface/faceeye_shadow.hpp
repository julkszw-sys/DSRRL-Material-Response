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

// Exact PTDE FaceEye auxiliary ShaderConstant_DirLightEntity register set.
// This is a carrier/layout contract only; CPU-authored field meanings and
// PTDE<->DSR value homology remain an independent readiness gate.
inline constexpr std::array<std::uint16_t,26> k_faceeye_auxiliary_registers={{
    121,122,123,
    140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,
    157,158,159,160,
    174,175,182
}};

struct faceeye_auxiliary_snapshot_descriptor {
    bool immutable_draw_local=false;
    std::array<bool,k_faceeye_auxiliary_registers.size()> register_present{};
};

inline bool faceeye_auxiliary_snapshot_complete(
    const faceeye_auxiliary_snapshot_descriptor &snapshot) noexcept
{
    if(!snapshot.immutable_draw_local)
        return false;
    for(const bool present : snapshot.register_present)
        if(!present)
            return false;
    return true;
}

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
    replacement_ptde_kernel_shader_not_ready,
    stock_ptde_kernel_identity_not_verified,
    runtime_t7_identity_not_verified,
    auxiliary_dirlight_snapshot_not_ready,
    auxiliary_dirlight_value_homology_not_verified,
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

    // DSR no-Point/PntS changed to a comparison-sampler 3x3 kernel and needs
    // an operator-local PTDE 4x4/manual-sample replacement. Fixed
    // PntSS/PntSSSS already retain the PTDE-style 16-tap kernel and should
    // preserve it rather than route through the replacement path.
    bool replacement_ptde_kernel_shader_ready=false;
    bool stock_ptde_kernel_identity_verified=false;

    bool runtime_t7_identity_verified=false;
    // PTDE FaceEye consumes exactly 26 auxiliary DirLightEntity registers.
    // Presence/immutability of the draw-local carrier does not prove that DSR
    // values have PTDE semantics, so homology stays a distinct hard gate.
    faceeye_auxiliary_snapshot_descriptor auxiliary_dirlight_snapshot{};
    bool auxiliary_dirlight_value_homology_verified=false;
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
    bool replace_comparison_kernel=false;
    bool use_stock_ptde_style_kernel=false;
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
