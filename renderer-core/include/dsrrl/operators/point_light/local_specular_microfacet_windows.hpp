#pragma once

#include "dsrrl/operators/point_light/legacy_specular.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::point_light {

enum class local_specular_window_result : std::uint8_t {
    exact = 0,
    fail_open_invalid_input,
    fail_open_dxbc_container,
    fail_open_instruction_stream,
    fail_open_anchor_count,
    fail_open_anchor_shape,
    fail_open_window_shape
};

struct local_specular_microfacet_window {
    std::uint32_t start_word = 0u;
    std::uint32_t schlick_word = 0u;
    std::uint32_t end_word_exclusive = 0u;
    std::uint16_t instruction_count = 0u;
    std::uint8_t light_ordinal = 0u;
};

struct local_specular_microfacet_window_scan {
    local_specular_window_result result =
        local_specular_window_result::fail_open_invalid_input;
    std::array<local_specular_microfacet_window,4> windows{};
    std::uint8_t window_count = 0u;
};

// Token-level structural guard for the exact-hash local PointLight receiver
// surface. This does NOT patch shader bytes. It proves the per-light DSR
// microfacet island boundary needed by the later PTDE replacement.
//
// Expected cardinality:
//   clustered Spc PntS   -> 1 window
//   fixed Spc PntSS     -> 2 windows
//   fixed Spc PntSSSS   -> 4 windows
//
// Each window must be the nearest IF/ENDIF region containing exactly one
// canonical DSR Schlick anchor:
//   MAD(..., -5.55473, -6.98316)
// with the surrounding DP3/DP3/DP3 -> MAD -> MUL -> EXP sequence.
//
// The function fails open on any malformed container, count mismatch,
// nesting mismatch, or unexpected structural shape.
local_specular_microfacet_window_scan
scan_local_specular_microfacet_windows(
    const void *pixel_shader_code,
    std::size_t code_size,
    local_specular_receiver_class receiver_class) noexcept;

// Exposed for deterministic unit tests and offline provenance generators.
// 'shex_words' is the complete SHEX/SHDR DWORD payload including the two-word
// shader header.
local_specular_microfacet_window_scan
scan_local_specular_microfacet_shex_words(
    const std::uint32_t *shex_words,
    std::size_t word_count,
    local_specular_receiver_class receiver_class) noexcept;

} // namespace dsrrl::operators::point_light
