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

    // Final stock local-light join immediately before the bridge MOV/Fog path.
    // Audited fixed Spc surface: ADD for 24 bodies, MAD for 24 bodies.
    std::uint32_t join_word = 0u;
    std::uint16_t join_opcode = 0u;

    // Last source operand of the join is the owned additive PointLight term:
    //   ADD -> src1
    //   MAD -> src2 (addend)
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
// only when, after the final fixed-light ENDIF, there is exactly one ADD/MAD
// join before a MOV bridge and the next instruction begins the c103 Fog path.
// Any structural drift fails open.
fixed_local_specular_output_cut
locate_fixed_local_specular_output_cut(
    const void *pixel_shader_code,
    std::size_t code_size) noexcept;

} // namespace dsrrl::operators::point_light
