#pragma once

#include <cstdint>

namespace dsrrl::operators::point_light {

enum class pnts_attenuation_result : std::uint8_t {
    exact = 0,
    fail_open_nonfinite_input,
    fail_open_invalid_range
};

struct pnts_attenuation_sample {
    pnts_attenuation_result result = pnts_attenuation_result::fail_open_invalid_range;
    float normalized_x = 0.0f;
    float stock_dsr = 0.0f;
    float ptde = 0.0f;
};

// Exact local attenuation cut for the confirmed substantive DSR PBL PntS family.
//
// x = (End - distance) / (End - Begin)
// DSR stock: A_D = sat(x^3)
// PTDE/bridge: A_P = sat(x)
//
// This intentionally owns attenuation only. Source RGB/intensity, material response,
// local diffuse/specular algebra and downstream composition remain separate operators.
pnts_attenuation_sample evaluate_pnts_attenuation(
    float distance,
    float begin,
    float end) noexcept;

} // namespace dsrrl::operators::point_light
