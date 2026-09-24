#pragma once

#include "dsrrl/core/island_policy.hpp"

#include <array>
#include <cstdint>

namespace dsrrl::operators::lightbank {

struct hemdir3_vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct hemdir3_lobe {
    hemdir3_vec3 direction{};
    hemdir3_vec3 color{};
};

enum class hemdir3_math_result : std::uint8_t {
    exact = 0,
    fail_open_nonfinite_input
};

struct hemdir3_sample {
    hemdir3_math_result result =
        hemdir3_math_result::fail_open_nonfinite_input;
    hemdir3_vec3 hemisphere{};
    std::array<float, 3> weights{};
    hemdir3_vec3 joined_source{};
};

// Exact PTDE local diffuse source join at the pre-material accumulator:
//   S = H + sum_i max(-dot(N_final, L_i), 0) * C_i
// HemDir3 Spc receivers additionally own a directional legacy-specular lobe;
// that dependency is gated separately by the b12 c101/c102 material donor.
hemdir3_sample evaluate_hemdir3_source_join(
    const hemdir3_vec3 &hemisphere,
    const hemdir3_vec3 &n_final,
    const std::array<hemdir3_lobe, 3> &lobes) noexcept;

enum class hemdir3_receiver_class : std::uint8_t {
    unsupported = 0,
    no_spc,
    spc
};

enum class hemdir3_runtime_reason : std::uint8_t {
    ready = 0,
    core_gate_not_active,
    semantic_mode_not_hemdir3,
    upper_lower_source_not_ready,
    d123_source_not_ready,
    b13_carrier_not_ready,
    receiver_not_verified,
    unsupported_receiver,
    material_specular_b12_not_ready,
    directional_specular_continuation_not_ready,
    host_envdiffuse_not_suppressed,
    material_continuation_not_ready,
    downstream_material_domain_not_ready,
    downstream_postfog_not_ready,
    atmosphere_route_not_verified,
    draw_transaction_not_ready
};

struct hemdir3_runtime_context {
    std::uint32_t semantic_mode = 0;
    bool upper_lower_source_ready = false;
    bool d123_source_ready = false;
    bool b13_carrier_ready = false;
    bool receiver_verified = false;
    hemdir3_receiver_class receiver_class = hemdir3_receiver_class::unsupported;

    // Family-wide PTDE RE: no-Spc consumes neither c101 nor c102. Spc owns
    // exactly one additional directional c102 lobe relative to paired HemEnv,
    // so it requires the immutable b12 {c101.rgb,c102} material donor.
    bool material_specular_b12_ready = false;
    bool directional_specular_continuation_ready = false;

    bool host_envdiffuse_source_suppressed = false;
    bool material_continuation_ready = false;
    bool downstream_material_domain_ready = false;
    bool downstream_postfog_ready = false;
    bool atmosphere_route_verified = false;
    bool draw_transaction_ready = false;
};

struct hemdir3_runtime_plan {
    bool ready = false;
    hemdir3_runtime_reason reason =
        hemdir3_runtime_reason::core_gate_not_active;
    bool suppress_host_envdiffuse = true;
    bool use_ptde_linear_d123 = true;
    bool require_directional_legacy_specular = false;
    bool require_material_specular_b12 = false;
    bool apply_source_gamma_compensation = false;
};

hemdir3_runtime_plan evaluate_hemdir3_runtime_readiness(
    const core::feature_registry &features,
    const core::activation_context &activation,
    const hemdir3_runtime_context &context) noexcept;

} // namespace dsrrl::operators::lightbank
