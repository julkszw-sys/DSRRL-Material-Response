#pragma once
#include <array>
#include <cstdint>

namespace dsrrl::runtime::pointlight_bank_structure_authority_v1 {

// Generated from Vanilla DSR DrawParam reference
// SHA256 70a90b51e471e8330b55ef17a9b8b86e18f7c0a1cfdb7384f3f51cfe165b8372.
// Signature = FNV-1a64(count LE16, then for each row: id LE32,
// raw Shift-JIS row-name bytes including the terminating NUL).
// All 12 DSR PointLightBank structures are unique. Only the ten banks with an
// exact PTDE donor are authorized here; default and m99 intentionally fail open.

inline constexpr std::array<std::uint64_t,10> k_signatures = {{
    0xc164f8066f3005c1ULL, // m10_PointLightBank.param
    0x5b13ff330dabde07ULL, // m11_PointLightBank.param
    0x327176cc06141ba1ULL, // m12_PointLightBank.param
    0xa6279dd5ab4a14ddULL, // m13_PointLightBank.param
    0xb13feee8842fbaecULL, // m14_PointLightBank.param
    0x073f641c590d8c5aULL, // m15_1_PointLightBank.param
    0x60ffd87880bf3025ULL, // m15_PointLightBank.param
    0x9ac9aaa4c0bf7398ULL, // m16_PointLightBank.param
    0x6d66c6d77d48e520ULL, // m17_PointLightBank.param
    0xbdb2721537da531eULL, // m18_PointLightBank.param
}};

// Exact PTDE-homolog row coverage of the DSRLL special DrawParam materialized
// from the 2026-10-06 PTDE/DSR references. Bit i authorizes row i for the
// PARAM-backed PTDE source path. m15_1 and m15 are distinct banks.
// Total covered PointLight rows: 281 / 640. Uncovered rows remain vanilla DSR.
inline constexpr std::array<std::uint64_t,10>
    k_ptde_drawparam_homolog_row_masks = {{
        0xf044070781309f9fULL, // m10
        0x00001f0000007c07ULL, // m11
        0xe1ed86004ff6180fULL, // m12
        0xf7ff1d0e5ffc7439ULL, // m13
        0xffc0647b800191eeULL, // m14
        0xff83e700000f9c27ULL, // m15_1
        0xffffe700000f9c27ULL, // m15
        0x0c001f0000007c07ULL, // m16
        0x00000073c00001cfULL, // m17
        0xfffcff80c3f3fe3fULL, // m18
    }};

static_assert(
    k_signatures.size() ==
    k_ptde_drawparam_homolog_row_masks.size());

inline constexpr std::array<std::uint64_t,2> k_known_non_donor_signatures = {{
    0xa98a983a732e81c8ULL, // default_PointLightBank.param
    0x00c4273b98549132ULL, // m99_PointLightBank.param
}};

} // namespace dsrrl::runtime::pointlight_bank_structure_authority_v1
