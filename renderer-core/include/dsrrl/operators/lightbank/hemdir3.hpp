#pragma once

#include "dsrrl/core/island_policy.hpp"
#include "dsrrl/operators/lightbank/snapshot_freshness.hpp"

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

struct hemdir3_raw_direction {
    float x_degrees = 0.0f;
    float y_degrees = 0.0f;
};

struct hemdir3_raw_color {
    hemdir3_vec3 rgb_255{};
    float multiplier_percent = 0.0f;
};

struct hemdir3_raw_lobe_endpoint {
    hemdir3_raw_direction direction{};
    hemdir3_raw_color color{};
};

enum class hemdir3_profile_result : std::uint8_t {
    exact = 0,
    fail_open_nonfinite_input
};

struct hemdir3_profile_sample {
    hemdir3_profile_result result =
        hemdir3_profile_result::fail_open_nonfinite_input;
    std::array<hemdir3_lobe, 3> lobes{};
};

// Exact PTDE LightBank D123 producer operators. Directions are interpolated
// in angle space on the shortest 2*pi arc before vector construction; colors
// are decoded per endpoint and then linearly interpolated. Cartesian direction
// vectors must never be lerped directly.
hemdir3_profile_sample evaluate_hemdir3_profile(
    const std::array<hemdir3_raw_lobe_endpoint, 3> &endpoint_a,
    const std::array<hemdir3_raw_lobe_endpoint, 3> &endpoint_b,
    float beta) noexcept;

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

// Exact PTDE local source join at the pre-material diffuse accumulator:
//   S = H + sum_i max(-dot(N_final, L_i), 0) * C_i
// Inputs are already PTDE-linear semantic carrier values. This function does
// not apply gamma/root/gain compensation and does not normalize L_i.
hemdir3_sample evaluate_hemdir3_source_join(
    const hemdir3_vec3 &hemisphere,
    const hemdir3_vec3 &n_final,
    const std::array<hemdir3_lobe, 3> &lobes) noexcept;

enum class hemdir3_receiver_stratum : std::uint8_t {
    unsupported = 0,
    nospc,
    spc
};

enum class hemdir3_runtime_reason : std::uint8_t {
    ready = 0,
    core_gate_not_active,
    semantic_mode_not_hemdir3,
    semantic_mode_provenance_not_verified,
    snapshot_owner_not_verified,
    snapshot_assignment_tuple_not_fresh,
    upper_lower_source_not_ready,
    d123_source_not_ready,
    b13_carrier_not_ready,
    receiver_not_verified,
    unsupported_receiver,
    spc_b12_material_donor_not_ready,
    directional_specular_continuation_not_ready,
    host_envdiffuse_not_suppressed,
    material_continuation_not_ready,
    downstream_material_domain_not_ready,
    downstream_postfog_not_ready,
    atmosphere_route_not_verified,
    draw_transaction_not_ready
};

enum class hemdir3_semantic_mode_provenance : std::uint8_t {
    unknown = 0,
    ordinary_direct,
    exact_effective_mode2
};

struct hemdir3_runtime_context {
    std::uint32_t semantic_mode = 0;
    hemdir3_semantic_mode_provenance semantic_mode_provenance =
        hemdir3_semantic_mode_provenance::unknown;
    // Canonical freshness contract: exact owner identity and exact
    // {selector_A, selector_B, beta_bits} equality are independent gates.
    lightbank_snapshot_fingerprint producer_snapshot{};
    lightbank_snapshot_fingerprint draw_snapshot{};
    bool upper_lower_source_ready = false;
    bool d123_source_ready = false;
    bool b13_carrier_ready = false;
    bool receiver_verified = false;
    hemdir3_receiver_stratum receiver_stratum =
        hemdir3_receiver_stratum::unsupported;

    // The Spc HemDir3 stratum has one additional legacy directional-specular
    // continuation relative to no-Spc. It consumes the immutable PTDE
    // c101/c102 donor carried in b12. no-Spc must not require or consume it.
    bool spc_b12_material_donor_ready = false;
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
    bool apply_source_gamma_compensation = false;
    bool require_b12_material_donor = false;
    bool require_directional_legacy_specular = false;
};

hemdir3_runtime_plan evaluate_hemdir3_runtime_readiness(
    const core::feature_registry &features,
    const core::activation_context &activation,
    const hemdir3_runtime_context &context) noexcept;

} // namespace dsrrl::operators::lightbank
