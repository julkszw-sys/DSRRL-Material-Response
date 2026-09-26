#include "dsrrl/operators/point_light/fixed_local_specular_pow_lowering.hpp"

namespace dsrrl::operators::point_light {
namespace {

constexpr std::uint32_t k_op_max = 0x08000034u;
constexpr std::uint32_t k_op_log = 0x0500002fu;
constexpr std::uint32_t k_op_mul = 0x08000038u;
constexpr std::uint32_t k_op_exp = 0x05000019u;

constexpr std::uint32_t k_imm_scalar = 0x00004001u;
constexpr std::uint32_t k_zero_bits = 0x00000000u;

// SM5 operand token helpers for a temporary scalar component.
// Destination uses component-mask mode; source uses select-one mode.
constexpr std::uint32_t temp_dst_token(std::uint8_t component) noexcept
{
    return component == 0u ? 0x00100012u :
           component == 1u ? 0x00100022u :
           component == 2u ? 0x00100042u :
                             0x00100082u;
}

constexpr std::uint32_t temp_src_token(std::uint8_t component) noexcept
{
    return 0x0010000au +
        (static_cast<std::uint32_t>(component) << 4u);
}

constexpr std::uint32_t cbuffer_scalar_token(
    std::uint8_t component) noexcept
{
    // Four-component operand, select-one component, constant-buffer type.
    return 0x0020800au +
        (static_cast<std::uint32_t>(component) << 4u);
}

fixed_local_specular_pow_emit_result emit(
    std::uint32_t temp_register,
    std::uint8_t component,
    std::uint32_t cb_slot,
    std::uint32_t cb_index,
    std::uint8_t cb_component,
    fixed_local_specular_pow_lowering &out) noexcept
{
    out = {};

    if (component > 3u || cb_component > 3u)
        return fixed_local_specular_pow_emit_result::
            fail_invalid_component;

    // SM5 immediate register index in the attested target corpus.
    if (temp_register > 4095u)
        return fixed_local_specular_pow_emit_result::
            fail_invalid_register;

    if (cb_slot > 4095u || cb_index > 4095u)
        return fixed_local_specular_pow_emit_result::
            fail_invalid_exponent_carrier;

    const auto dst = temp_dst_token(component);
    const auto src = temp_src_token(component);
    const auto cb = cbuffer_scalar_token(cb_component);

    out.words = {{
        // max rN.c, rN.c, l(0)
        k_op_max,
        dst, temp_register,
        src, temp_register,
        k_imm_scalar, k_zero_bits,

        // log rN.c, rN.c
        k_op_log,
        dst, temp_register,
        src, temp_register,

        // mul rN.c, rN.c, cbX[Y].c
        k_op_mul,
        dst, temp_register,
        src, temp_register,
        cb, cb_slot, cb_index,

        // exp rN.c, rN.c
        k_op_exp,
        dst, temp_register,
        src, temp_register
    }};

    return fixed_local_specular_pow_emit_result::exact;
}

} // namespace

fixed_local_specular_pow_emit_result
emit_fixed_local_specular_ptde_pow(
    std::uint32_t temp_register,
    std::uint8_t component,
    fixed_local_specular_pow_lowering &out) noexcept
{
    return emit(
        temp_register,
        component,
        12u,
        0u,
        3u,
        out);
}

fixed_local_specular_pow_emit_result
emit_stock_dsr_water_specular_pow(
    std::uint32_t temp_register,
    std::uint8_t component,
    fixed_local_specular_pow_lowering &out) noexcept
{
    return emit(
        temp_register,
        component,
        0u,
        11u,
        0u,
        out);
}

} // namespace dsrrl::operators::point_light
