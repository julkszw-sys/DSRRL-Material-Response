#include "dsrrl/operators/point_light/pnts_attenuation.hpp"

#include <cmath>

namespace dsrrl::operators::point_light {
namespace {

float saturate(float value) noexcept
{
    if (value <= 0.0f)
        return 0.0f;
    if (value >= 1.0f)
        return 1.0f;
    return value;
}

} // namespace

pnts_attenuation_sample evaluate_pnts_attenuation(
    float distance,
    float begin,
    float end) noexcept
{
    if (!std::isfinite(distance) ||
        !std::isfinite(begin) ||
        !std::isfinite(end))
        return {pnts_attenuation_result::fail_open_nonfinite_input};

    if (!(end > begin))
        return {pnts_attenuation_result::fail_open_invalid_range};

    const float x = (end - distance) / (end - begin);
    const float dsr = saturate(x * x * x);
    const float ptde = saturate(x);

    return {
        pnts_attenuation_result::exact,
        x,
        dsr,
        ptde
    };
}

} // namespace dsrrl::operators::point_light
