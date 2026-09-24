#include "dsrrl/operators/surface/fixed_postfog_identity.hpp"

#include <cmath>

namespace dsrrl::operators::surface {

fixed_postfog_sample evaluate_fixed_postfog(
    float postfog_rgb_component,
    float selector) noexcept
{
    if (!std::isfinite(postfog_rgb_component) ||
        !std::isfinite(selector))
        return {fixed_postfog_result::fail_open_nonfinite_input};

    const float stock =
        selector > 0.5f
        ? std::pow(std::fabs(postfog_rgb_component), 1.0f / 2.2f)
        : postfog_rgb_component;

    return {
        fixed_postfog_result::exact,
        postfog_rgb_component,
        selector,
        stock,
        postfog_rgb_component
    };
}

} // namespace dsrrl::operators::surface
