#pragma once
// PROVISIONAL, DSR-ONLY ROWS. Exact values extracted from user-provided
// Vanilla DSR DrawParam(1).zip, verified by the native DSR row format.
// The 1246 homologous LightBank rows remain authoritative PTDE donors.
// This table is NOT a claim that PTDE has no scene-semantic equivalent.
// Source zip SHA256:
// 70a90b51e471e8330b55ef17a9b8b86e18f7c0a1cfdb7384f3f51cfe165b8372
// Offsets from 92-byte LIGHT_BANK_PARAM_ST row (u16x4):
// EnvDiffuse RGBM +0x40; EnvSpec RGBM +0x48.
// DSR row bytes SHA256 m14: 8e2a231c22cbf13fb2f68b316ecc17921acbf6148ade7018f318c75a2a5e17b2
// DSR row bytes SHA256 s14: b5007735f3695286b4560412100a2b3a7e6806fd0a0df1ff58788158b97cc381
// DSR row bytes SHA256 m18: be9e096eb2c43971ba9189f87fabff9f1854e58b85acd8cbf0b51baa261dec93

#include <array>
#include <cstdint>

namespace dsrrl::runtime::pmetal_dsr_only_lightbank_fallback {

struct unmatched_row {
    std::uint64_t ptde_bank_signature = 0u;
    std::uint32_t row_id = 0u;
    std::array<std::uint16_t,4> dsr_envdiffuse_rgbm{};
    std::array<std::uint16_t,4> dsr_envspec_rgbm{};
};

inline constexpr std::array<unmatched_row,3> k_rows{{
    // a14/m14_LightBank.param ID64: 0200_0006.
    {0xc9b879a8c5e624d0ULL,64u,{255u,255u,255u,1500u},{255u,255u,255u,200u}},
    // a14/s14_LightBank.param ID64: 0200_0006.
    {0xe447e0e74af8d1d3ULL,64u,{255u,255u,255u,500u},{255u,255u,255u,200u}},
    // a18/m18_LightBank.param ID64: character_entrance.
    {0x1ecfd1e617c59071ULL,64u,{255u,255u,255u,150u},{255u,255u,255u,150u}}
}};

constexpr const unmatched_row *find(
    std::uint64_t signature,
    std::uint32_t row_id) noexcept
{
    if (row_id != 64u)
        return nullptr;
    for (const auto &entry : k_rows)
        if (entry.ptde_bank_signature == signature &&
            entry.row_id == row_id)
            return &entry;
    return nullptr;
}

} // namespace dsrrl::runtime::pmetal_dsr_only_lightbank_fallback
