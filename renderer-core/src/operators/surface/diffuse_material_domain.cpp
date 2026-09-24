#include "dsrrl/operators/surface/diffuse_material_domain.hpp"

#include <cmath>

namespace dsrrl::operators::surface {

diffuse_material_domain_sample evaluate_diffuse_material_domain_host(
    float dsr_pretransform) noexcept
{
    if (!std::isfinite(dsr_pretransform))
        return {diffuse_material_domain_result::fail_open_nonfinite_input};

    const float stock =
        std::pow(std::fabs(dsr_pretransform), 2.2f);

    return {
        diffuse_material_domain_result::exact_local_host_forward,
        dsr_pretransform,
        stock,
        dsr_pretransform
    };
}

} // namespace dsrrl::operators::surface
