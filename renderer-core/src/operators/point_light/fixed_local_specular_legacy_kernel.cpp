#include "dsrrl/operators/point_light/fixed_local_specular_legacy_kernel.hpp"
#include "dsrrl/operators/point_light/fixed_local_specular_pow_lowering.hpp"
#include "dsrrl/operators/point_light/fixed_local_specular_reflect_lowering.hpp"
#include <algorithm>
namespace dsrrl::operators::point_light {
fixed_local_specular_legacy_kernel_result emit_fixed_local_specular_legacy_kernel(
    const fixed_local_specular_light_operands &operands,
    fixed_local_specular_legacy_kernel &out) noexcept
{
    out = {};
    fixed_local_specular_reflect_lowering reflect{};
    if (emit_fixed_local_specular_reflect_rdotl(operands, reflect) !=
        fixed_local_specular_reflect_emit_result::exact)
        return fixed_local_specular_legacy_kernel_result::fail_reflect_lowering;
    fixed_local_specular_pow_lowering power{};
    if (emit_fixed_local_specular_ptde_pow(
            reflect.result_temp_register,
            reflect.result_component,
            power) != fixed_local_specular_pow_emit_result::exact)
        return fixed_local_specular_legacy_kernel_result::fail_pow_lowering;
    std::copy(reflect.words.begin(), reflect.words.end(), out.words.begin());
    std::copy(power.words.begin(), power.words.end(),
              out.words.begin() + reflect.words.size());
    out.result_temp_register = reflect.result_temp_register;
    out.result_component = reflect.result_component;
    return fixed_local_specular_legacy_kernel_result::exact;
}
} // namespace dsrrl::operators::point_light
