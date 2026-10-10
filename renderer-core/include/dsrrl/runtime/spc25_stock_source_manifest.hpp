#pragma once
// EXACT owner-source DSR parts TPF/DDS compressed mip payloads.
// SHA256 source ZIP a76b13c909fb4a30a1a312cba7d594a851193bef55e578934829fd873f872c97
// Only 10 logical P_Metal stock specular names, 13 exact FLVER slot source
// rows. No C_Metal/C_RoughCloth texture is claimed to exist in this corpus.
// SOURCE SHA match is NOT a live FLVER owner/stock SRV proof.
#include <array>
#include <cstdint>
#include <string_view>
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

namespace dsrrl::runtime::spc25_stock_source_manifest {
using digest = operators::legacy_plan::hashing::sha256_digest;
struct entry {
    const char *logical;
    std::uint32_t width, height, mips, dxgi;
    const char *sha256;
};
inline constexpr std::array<entry,10u> k_original_dsr_spec = {{
    {"WP_A_0100_s",256,128,9,71,"755f722b69df5109561ccac166cd30f08946df132d762db547ff7eee4b9e9a27"},
    {"WP_A_0103_s",256,256,9,71,"55aabaa278c39fbeec21f31f9652199fc49f51f61ed328363a3acb6cfa749d61"},
    {"WP_A_0106_mailbreaker_s",256,128,9,71,"2c8555064649f6512eb4d29219affcf40622a7384384560657a8af347890e25e"},
    {"WP_A_0107_s",256,128,9,71,"b11642787b8833e8e340908e34c612ebd0724034f6a81ab110432b9967e88a94"},
    {"WP_A_0211_s",128,1024,11,71,"bf9939e60b8f7716b9b202dedccaa618cbf90b62f7c592382106b953f74610c5"},
    {"WP_A_0652_L_s",256,256,9,71,"c19448fc8263245f92bb9d9bb872697e79caca1828b303f9e795a8e74322f7b1"},
    {"WP_A_0658_s",256,256,9,71,"4fddd3b8f2def1c67ce4f32655355b876ac504d1bffaf2605b466f49e1b36e26"},
    {"WP_A_0801_Scythe_s",128,1024,11,71,"724aa63dc618b9f2f67401394288082f49140b870b64ce2da0bc0488585fb2e5"},
    {"WP_A_0905_s",256,256,9,71,"002bd8f93fd86169a05a99e8a2a3069bd27611697a70bb6853b40268c1ec5e0f"},
    {"WP_A_1100_WingedSpear_s",128,1024,11,71,"06ed8405a522a47e505496239271116a470e32b26825c790bf9a28a6c549c9d8"}
}};
inline const entry *unique_exact(
    std::uint32_t width,std::uint32_t height,
    std::uint32_t mips,std::uint32_t dxgi,
    const digest &source) noexcept
{
    const entry *match = nullptr;
    for (const auto &e : k_original_dsr_spec) {
        if(e.width!=width || e.height!=height ||
           e.mips!=mips || e.dxgi!=dxgi ||
           !operators::legacy_plan::hashing::matches_hex(source,e.sha256))
            continue;
        if(match!=nullptr) return nullptr; // noninjective source digest
        match=&e;
    }
    return match;
}
} // namespace dsrrl::runtime::spc25_stock_source_manifest
