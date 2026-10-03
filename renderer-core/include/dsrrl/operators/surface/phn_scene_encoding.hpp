#pragma once

#include <cstdint>
#include <vector>

namespace dsrrl::operators::surface {

// PTDE PHN scene-encoding carrier.
//
// PTDE legacy PHN terminates as:
//   C_out.rgb = sat((c135.x / c135.y) * C_preterminal.rgb)
//
// The bridge transports the already-resolved scalar k135 through an existing
// draw-local constant-buffer lane. Carrier resolution is deliberately kept
// outside this shader transform so material families can provide their own
// verified producer without duplicating the terminal operator.
struct phn_scene_encoding_carrier {
    std::uint32_t cb_slot = 0u;
    std::uint32_t cb_index = 0u;
    std::uint8_t cb_component = 0u;
};

enum class phn_scene_encoding_result : std::uint8_t {
    applied = 0,
    already_encoded,
    fail_invalid_carrier,
    fail_invalid_token_stream,
    fail_unverified_terminal_shape,
    fail_ambiguous_terminal,
    fail_unexpected_terminal_opcode
};

// Replace the unique separate RGB terminal MOV/MOV_SAT:
//
//   mov[_sat] o0.xyz, S
//
// with the PTDE terminal operator:
//
//   mul_sat o0.xyz, S, cb<slot>[index].component
//
// Alpha remains untouched. Combined RGBA writes and ambiguous terminal shapes
// fail open. The function updates the SM4/5 token count in words[1].
phn_scene_encoding_result apply_unique_phn_scene_encoding_words(
    std::vector<std::uint32_t> &words,
    const phn_scene_encoding_carrier &carrier) noexcept;

// Exact postcondition for the transformed terminal operator. This checks both
// the PTDE scene-encoding multiply and the terminal SAT in one semantic cut.
bool unique_phn_scene_encoding_exact(
    const std::vector<std::uint32_t> &words,
    const phn_scene_encoding_carrier &carrier) noexcept;

} // namespace dsrrl::operators::surface
