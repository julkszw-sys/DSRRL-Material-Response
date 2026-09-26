#include "dsrrl/operators/point_light/fixed_local_specular_reflect_lowering.hpp"

namespace dsrrl::operators::point_light {
namespace {

constexpr std::uint32_t k_dp3 = 0x07002010u;
constexpr std::uint32_t k_mul = 0x07000038u;
constexpr std::uint32_t k_mad = 0x09000032u;
constexpr std::uint32_t k_imm_scalar = 0x00004001u;
constexpr std::uint32_t k_two_bits = 0x40000000u;
constexpr std::uint32_t k_minus_one_bits = 0xbf800000u;

bool temp_scalar_destination(
    const fixed_local_specular_operand_pair &pair,
    std::uint32_t &reg,
    std::uint8_t &component) noexcept
{
    const auto token = pair.token[0];
    const auto type = (token >> 12u) & 0xffu;
    const auto dimension = (token >> 20u) & 0x3u;
    const auto representation = (token >> 22u) & 0x3u;
    const auto mask = (token >> 4u) & 0xfu;
    if (type != 0u || dimension != 1u || representation != 0u)
        return false;

    switch (mask) {
    case 0x1u: component = 0u; break;
    case 0x2u: component = 1u; break;
    case 0x4u: component = 2u; break;
    case 0x8u: component = 3u; break;
    default: return false;
    }

    reg = pair.token[1];
    return reg <= 4095u;
}

std::uint32_t scalar_source_token(std::uint8_t component) noexcept
{
    return 0x0010000au +
        (static_cast<std::uint32_t>(component) << 4u);
}

void append_pair(
    std::array<std::uint32_t,44> &dst,
    std::size_t &at,
    const fixed_local_specular_operand_pair &pair) noexcept
{
    dst[at++] = pair.token[0];
    dst[at++] = pair.token[1];
}

void append_scalar_source(
    std::array<std::uint32_t,44> &dst,
    std::size_t &at,
    std::uint32_t reg,
    std::uint8_t component) noexcept
{
    dst[at++] = scalar_source_token(component);
    dst[at++] = reg;
}

void append_dp3(
    std::array<std::uint32_t,44> &dst,
    std::size_t &at,
    const fixed_local_specular_operand_pair &scalar_dst,
    const fixed_local_specular_operand_pair &a,
    const fixed_local_specular_operand_pair &b) noexcept
{
    dst[at++] = k_dp3;
    append_pair(dst,at,scalar_dst);
    append_pair(dst,at,a);
    append_pair(dst,at,b);
}

} // namespace

fixed_local_specular_reflect_emit_result
emit_fixed_local_specular_reflect_rdotl(
    const fixed_local_specular_light_operands &operands,
    fixed_local_specular_reflect_lowering &out) noexcept
{
    out = {};

    std::uint32_t primary_reg=0u;
    std::uint32_t auxiliary_reg=0u;
    std::uint32_t ndotl_reg=0u;
    std::uint8_t primary_component=0u;
    std::uint8_t auxiliary_component=0u;
    std::uint8_t ndotl_component=0u;

    if (!temp_scalar_destination(
            operands.primary_scalar_dst,
            primary_reg,
            primary_component) ||
        !temp_scalar_destination(
            operands.auxiliary_scalar_dst,
            auxiliary_reg,
            auxiliary_component) ||
        !temp_scalar_destination(
            operands.ndotl_scalar_dst,
            ndotl_reg,
            ndotl_component))
        return fixed_local_specular_reflect_emit_result::
            fail_non_scalar_destination;

    if ((primary_reg == auxiliary_reg &&
         primary_component == auxiliary_component) ||
        (primary_reg == ndotl_reg &&
         primary_component == ndotl_component) ||
        (auxiliary_reg == ndotl_reg &&
         auxiliary_component == ndotl_component))
        return fixed_local_specular_reflect_emit_result::
            fail_aliasing_destination;

    for (const auto *pair :
         {&operands.normal,&operands.view,&operands.light}) {
        const auto token = pair->token[0];
        if (((token >> 12u) & 0xffu) != 0u ||
            ((token >> 20u) & 0x3u) != 1u ||
            pair->token[1] > 4095u)
            return fixed_local_specular_reflect_emit_result::
                fail_invalid_operand;
    }

    std::size_t at=0u;

    append_dp3(
        out.words,at,
        operands.primary_scalar_dst,
        operands.normal,
        operands.view);

    append_dp3(
        out.words,at,
        operands.auxiliary_scalar_dst,
        operands.view,
        operands.light);

    append_dp3(
        out.words,at,
        operands.ndotl_scalar_dst,
        operands.normal,
        operands.light);

    out.words[at++] = k_mul;
    append_pair(out.words,at,operands.primary_scalar_dst);
    append_scalar_source(
        out.words,at,primary_reg,primary_component);
    append_scalar_source(
        out.words,at,ndotl_reg,ndotl_component);

    out.words[at++] = k_mul;
    append_pair(out.words,at,operands.primary_scalar_dst);
    append_scalar_source(
        out.words,at,primary_reg,primary_component);
    out.words[at++] = k_imm_scalar;
    out.words[at++] = k_two_bits;

    out.words[at++] = k_mad;
    append_pair(out.words,at,operands.primary_scalar_dst);
    append_scalar_source(
        out.words,at,auxiliary_reg,auxiliary_component);
    out.words[at++] = k_imm_scalar;
    out.words[at++] = k_minus_one_bits;
    append_scalar_source(
        out.words,at,primary_reg,primary_component);

    if (at != out.words.size())
        return fixed_local_specular_reflect_emit_result::
            fail_invalid_operand;

    out.result_temp_register = primary_reg;
    out.result_component = primary_component;
    return fixed_local_specular_reflect_emit_result::exact;
}

} // namespace dsrrl::operators::point_light
