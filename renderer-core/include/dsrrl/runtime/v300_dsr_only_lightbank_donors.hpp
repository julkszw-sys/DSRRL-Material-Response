#pragma once
// V300 — independently decoded from unmodified vanilla DSR DrawParam ZIP
// SHA256 70a90b51e471e8330b55ef17a9b8b86e18f7c0a1cfdb7384f3f51cfe165b8372
//
// Rows 0..(donor_count-1) remain original PTDE donors. DSR-only row 64
// exists ONLY in these three exact, structurally identified DSR LightBanks.
// Do not alias row 64 to PTDE row 63, to another bank, or to unity.
// For a DSR-only source draw, the ENTIRE EnvSpec+EnvDiffuse operator must
// remain stock DSR until pixel/receiver equivalence is independently proven.
#include <array>
#include <cstdint>

namespace dsrrl::runtime::v300 {

enum class donor_provenance : std::uint8_t {
    unknown = 0,
    ptde_exact = 1,
    dsr_only = 2
};

struct dsr_lightbank_row {
    std::uint64_t signature;
    std::uint64_t layout_signature;
    std::uint32_t live_count;
    std::uint32_t ptde_donor_count;
    std::uint32_t row_id;
    std::array<std::uint16_t,4> dsr_envdiffuse;
    std::array<std::uint16_t,4> dsr_envspec;
};

// Raw LIGHT_BANK row has 46 int16 entries; v300 independently extracted
// indices 32..35 EnvDiffuse and 36..39 EnvSpec from DSR BND3 PARAM record.
// The 0..63 original PTDE row table is not rewritten.
inline constexpr std::array<dsr_lightbank_row,3> k_dsr_only = {{
    // a14/m14, new row 64: DSR EnvDiffuse=white x15; EnvSpec=white x2.
    {0xc9b879a8c5e624d0ULL,0xac99bb1bdf5a164aULL,65u,64u,64u,
     {{255u,255u,255u,1500u}},{{255u,255u,255u,200u}}},
    // a14/s14, new row 64: EnvDiffuse white x5, EnvSpec white x2.
    {0xe447e0e74af8d1d3ULL,0x884d28629c29a7f3ULL,65u,64u,64u,
     {{255u,255u,255u,500u}},{{255u,255u,255u,200u}}},
    // a18/m18, new row 64: EnvDiffuse white x1.5, EnvSpec white x1.5.
    {0x1ecfd1e617c59071ULL,0x4233f14d61b764beULL,65u,64u,64u,
     {{255u,255u,255u,150u}},{{255u,255u,255u,150u}}}
}};

struct selection {
    donor_provenance provenance = donor_provenance::unknown;
    const dsr_lightbank_row *dsr_values = nullptr;
};

constexpr selection classify_dsr_only(
    std::uint64_t signature,
    std::uint64_t layout,
    std::uint32_t live_count,
    std::uint32_t ptde_count,
    std::uint32_t row_id) noexcept
{
    for (const auto &e : k_dsr_only) {
        if (e.signature != signature ||
            e.layout_signature != layout ||
            e.live_count != live_count ||
            e.ptde_donor_count != ptde_count)
            continue;
        if (row_id == e.row_id)
            return {donor_provenance::dsr_only, &e};
        return {};
    }
    return {};
}

// Runtime bridge policy: a DSR-only source is native, not a missing PTDE
// donor. It must NEVER be synthesized with previous/PTDE data.
constexpr bool requires_stock_dsr(const selection &s) noexcept {
    return s.provenance != donor_provenance::ptde_exact;
}

constexpr std::array<float,3> decode_linear(
    const std::array<std::uint16_t,4> &v) noexcept
{
    const float scale = static_cast<float>(v[3]) / 100.0f;
    return {{v[0] / 255.0f * scale,
             v[1] / 255.0f * scale,
             v[2] / 255.0f * scale}};
}

} // namespace dsrrl::runtime::v300
