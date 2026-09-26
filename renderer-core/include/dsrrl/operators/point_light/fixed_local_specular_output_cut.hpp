#pragma once

#include "dsrrl/operators/point_light/fixed_local_specular_operand_contract.hpp"

#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::point_light {

enum class fixed_local_specular_output_cut_result : std::uint8_t {
    exact = 0,
    pass_not_fixed_local_specular,
    fail_invalid_dxbc,
    fail_operand_contract,
    fail_fog_anchor,
    fail_join_shape,
    fail_local_operand_shape
};

struct fixed_local_specular_output_cut {
    fixed_local_specular_output_cut_result result =
        fixed_local_specular_output_cut_result::pass_not_fixed_local_specular;

    fixed_local_specular_operand_contract operands{};

    // Exact fixed-Spc census: 48/48 unique bodies place the local-light join
    // as the first instruction immediately after the final PointLight ENDIF,
    // and that join is always a 7-DWORD ADD. Some bodies have an unrelated
    // continuation MAD after this ADD; it is not part of the PointLight cut.
    std::uint32_t join_word = 0u;
    std::uint16_t join_opcode = 0u;

    // The last ADD source operand is the owned additive PointLight term.
    // The materializer may redirect only this temp index to the PTDE island
    // result, leaving the stock block executable but dead at the owned cut.
    std::uint32_t local_operand_token_word = 0u;
    std::uint32_t local_operand_index_word = 0u;
    std::uint32_t stock_local_temp = 0u;

    std::uint32_t bridge_mov_word = 0u;
    std::uint32_t first_fog_word = 0u;
};

// Exact fixed PntSS/PntSSSS Spc output-cut locator. Positive activation first
// requires the complete receiver/window/operand contract. The cut is accepted
// only when the first instruction after the final fixed-light ENDIF is the
// exact 7-DWORD ADD join. The downstream bridge/Fog path is independently
// attested but may contain family-specific continuation instructions.
// Any structural drift fails open.
fixed_local_specular_output_cut
locate_fixed_local_specular_output_cut(
    const void *pixel_shader_code,
    std::size_t code_size) noexcept;

} // namespace dsrrl::operators::point_light
