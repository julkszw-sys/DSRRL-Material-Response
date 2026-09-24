#pragma once

#include <cstdint>

namespace dsrrl::operators::surface {

enum class fixed_postfog_result : std::uint8_t {
    exact = 0,
    fail_open_nonfinite_input
};

struct fixed_postfog_sample {
    fixed_postfog_result result = fixed_postfog_result::fail_open_nonfinite_input;
    float postfog_input = 0.0f;
    float selector = 0.0f;
    float stock_dsr = 0.0f;
    float ptde_bridge = 0.0f;
};

// Confirmed fixed-family post-Fog cut.
//
// DSR stock:
//   Y = selector > 0.5 ? abs(C)^(1/2.2) : C
//
// PTDE fixed homolog / bridge:
//   Y = C
//
// The bridge removes only this conditional post-Fog root. Fog itself,
// pre-Fog material/light composition, terminal SAT and later postprocess remain
// separate owners.
fixed_postfog_sample evaluate_fixed_postfog(
    float postfog_rgb_component,
    float selector) noexcept;

} // namespace dsrrl::operators::surface
