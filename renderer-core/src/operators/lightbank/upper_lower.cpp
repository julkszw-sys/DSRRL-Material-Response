#include "dsrrl/operators/lightbank/upper_lower.hpp"

#include <cmath>

namespace dsrrl::operators::lightbank {
namespace {

bool finite3(const ul_rgb3 &v) noexcept
{
    return std::isfinite(v.r) &&
           std::isfinite(v.g) &&
           std::isfinite(v.b);
}

bool finite_endpoint(const ul_raw_endpoint &e) noexcept
{
    return finite3(e.rgb_255) && std::isfinite(e.multiplier_percent);
}

ul_rgb3 decode(const ul_raw_endpoint &e) noexcept
{
    const float scale = (1.0f / 255.0f) * (e.multiplier_percent / 100.0f);
    return {
        e.rgb_255.r * scale,
        e.rgb_255.g * scale,
        e.rgb_255.b * scale
    };
}

ul_rgb3 blend(const ul_rgb3 &a, const ul_rgb3 &b, float beta) noexcept
{
    const float inv = 1.0f - beta;
    return {
        inv * a.r + beta * b.r,
        inv * a.g + beta * b.g,
        inv * a.b + beta * b.b
    };
}

ul_rgb3 hemisphere(const ul_rgb3 &lower, const ul_rgb3 &upper, float t) noexcept
{
    return {
        lower.r + t * (upper.r - lower.r),
        lower.g + t * (upper.g - lower.g),
        lower.b + t * (upper.b - lower.b)
    };
}

} // namespace

upper_lower_sample evaluate_upper_lower(
    const ul_raw_endpoint &upper_a,
    const ul_raw_endpoint &lower_a,
    const ul_raw_endpoint &upper_b,
    const ul_raw_endpoint &lower_b,
    float beta,
    float n_final_y) noexcept
{
    if (!finite_endpoint(upper_a) ||
        !finite_endpoint(lower_a) ||
        !finite_endpoint(upper_b) ||
        !finite_endpoint(lower_b) ||
        !std::isfinite(beta) ||
        !std::isfinite(n_final_y))
        return {upper_lower_result::fail_open_nonfinite_input};

    const ul_rgb3 upper = blend(decode(upper_a), decode(upper_b), beta);
    const ul_rgb3 lower = blend(decode(lower_a), decode(lower_b), beta);
    const float t = 0.5f * n_final_y + 0.5f;

    return {
        upper_lower_result::exact,
        upper,
        lower,
        t,
        hemisphere(lower, upper, t)
    };
}

} // namespace dsrrl::operators::lightbank
