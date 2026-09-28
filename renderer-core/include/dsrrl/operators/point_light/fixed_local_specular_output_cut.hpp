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
    fail_local_operand_shape,
    fail_local_component_liveness
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

    // The last ADD source operand is the owned additive PointLight RGB term.
    // The materializer redirects only this temp index to the PTDE island
    // result. Component liveness is attested separately: an unrelated lane
    // may survive only when the owned PointLight windows never touch it.
    std::uint32_t local_operand_token_word = 0u;
    std::uint32_t local_operand_index_word = 0u;
    std::uint32_t stock_local_temp = 0u;
    std::uint8_t stock_local_join_component_mask = 0u;
    std::uint8_t stock_local_owned_window_component_mask = 0u;
    std::uint8_t stock_local_downstream_component_mask = 0u;
    std::uint32_t stock_local_owned_window_use_count = 0u;
    std::uint32_t stock_local_downstream_use_count = 0u;
    bool stock_local_owned_components_dead_after_join = false;

    std::uint32_t bridge_mov_word = 0u;
    std::uint32_t first_fog_word = 0u;
};

// Exact fixed PntSS/PntSSSS Spc output-cut locator. Positive activation first
// requires the complete receiver/window/operand contract. The cut is accepted
// only when the first instruction after the final fixed-light ENDIF is the
// exact 7-DWORD ADD join. The owned local term must be xyz, and component
// liveness is checked through the remainder of the shader: no owned RGB lane
// may be consumed after the join, and any surviving lane must be disjoint from
// the components touched by the PointLight windows. The downstream bridge/Fog
// path may contain family-specific continuation instructions. Any structural
// drift fails open.
fixed_local_specular_output_cut
locate_fixed_local_specular_output_cut(
    const void *pixel_shader_code,
    std::size_t code_size) noexcept;

} // namespace dsrrl::operators::point_light
