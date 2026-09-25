#pragma once

#include "dsrrl/core/draw_transaction_policy.hpp"

#include <cstdint>

namespace dsrrl::operators::lightbank {

static_assert(
    core::requires_draw_transaction(core::operator_id::upper_lower),
    "Upper/Lower draw carrier must use the shared transaction layer.");

struct ul_rgb3 {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

struct ul_raw_endpoint {
    ul_rgb3 rgb_255{};
    float multiplier_percent = 0.0f;
};

enum class upper_lower_result : std::uint8_t {
    exact = 0,
    fail_open_nonfinite_input
};

struct upper_lower_sample {
    upper_lower_result result = upper_lower_result::fail_open_nonfinite_input;
    ul_rgb3 upper_ptde{};
    ul_rgb3 lower_ptde{};
    float hemisphere_t = 0.0f;
    ul_rgb3 hemisphere{};
};

// Exact PTDE-authored U/L reconstruction.
//
// Endpoint decode:
//   P(RGB,M) = (RGB / 255) * (M / 100)
//
// LightBank endpoint blend:
//   U = (1-beta) U_A + beta U_B
//   L = (1-beta) L_A + beta L_B
//
// Shader island:
//   t = 0.5 * N_final.y + 0.5
//   H = L + t * (U - L)
//
// The reconstructed U/L vectors are the b13[6].xyz / b13[7].xyz semantic
// payload. This function does not apply the DSR x1.5, pow(2.2), endpoint
// inverse/root, or compensate the common downstream pre-Fog root.
upper_lower_sample evaluate_upper_lower(
    const ul_raw_endpoint &upper_a,
    const ul_raw_endpoint &lower_a,
    const ul_raw_endpoint &upper_b,
    const ul_raw_endpoint &lower_b,
    float beta,
    float n_final_y) noexcept;

} // namespace dsrrl::operators::lightbank
