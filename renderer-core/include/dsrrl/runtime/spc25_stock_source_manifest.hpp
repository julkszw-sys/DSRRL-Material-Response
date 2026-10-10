#pragma once
// EXACT owner-source DSR parts TPF/DDS compressed mip payloads.
// SHA256 source ZIP a76b13c909fb4a30a1a312cba7d594a851193bef55e578934829fd873f872c97
// 18 unique original DSR parts TPF *_s source fingerprints are available.
// Only the original 10 P_Metal entries have exact FLVER/slot/MTD authority
// (13 source rows). The other 8 are source-only, NOT authenticated SPC25 routes.
// No C_Metal/C_RoughCloth texture is claimed to exist in this partial corpus.
// Byte-content identity does not replace live FLVER/MTD/slot/receiver binding.
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
inline constexpr std::array<entry,18u> k_original_dsr_spec = {{
    {"WP_A_0100_s",256,128,9,71,"755f722b69df5109561ccac166cd30f08946df132d762db547ff7eee4b9e9a27"},
    {"WP_A_0103_s",256,256,9,71,"55aabaa278c39fbeec21f31f9652199fc49f51f61ed328363a3acb6cfa749d61"},
    {"WP_A_0106_mailbreaker_s",256,128,9,71,"2c8555064649f6512eb4d29219affcf40622a7384384560657a8af347890e25e"},
    {"WP_A_0107_s",256,128,9,71,"b11642787b8833e8e340908e34c612ebd0724034f6a81ab110432b9967e88a94"},
    {"WP_A_0211_s",128,1024,11,71,"bf9939e60b8f7716b9b202dedccaa618cbf90b62f7c592382106b953f74610c5"},
    {"WP_A_0652_L_s",256,256,9,71,"c19448fc8263245f92bb9d9bb872697e79caca1828b303f9e795a8e74322f7b1"},
    {"WP_A_0658_s",256,256,9,71,"4fddd3b8f2def1c67ce4f32655355b876ac504d1bffaf2605b466f49e1b36e26"},
    {"WP_A_0801_Scythe_s",128,1024,11,71,"724aa63dc618b9f2f67401394288082f49140b870b64ce2da0bc0488585fb2e5"},
    {"WP_A_0905_s",256,256,9,71,"002bd8f93fd86169a05a99e8a2a3069bd27611697a70bb6853b40268c1ec5e0f"},
    {"WP_A_1100_WingedSpear_s",128,1024,11,71,"06ed8405a522a47e505496239271116a470e32b26825c790bf9a28a6c549c9d8"},
    // Additional exact original compressed payloads from the owner-uploaded
    // parts archive. SOURCE-ONLY until a same-owner FLVER/slot joins each.
    {"AM_F_9450_L_s",256,256,9,71,"4b68ba87b53ea1054904a3ea7461b3e41a8c1c8bc1bba432f3e93dac726114e4"},
    {"AM_M_9372_L_s",256,256,9,71,"d9b55661884b01c7ced4c49cf5266ac31cae82349ac90c489f26830c8bcbfbd0"},
    {"HR_F_0009_L_s",128,128,8,71,"a0b1afef5a8dd6365932c808e18250ad0a3cb4d4b7f584e461a0200c1324f530"},
    {"WP_A_0906_s",256,256,9,71,"4635353e633639ebb29c101c76956cbe51ad0f4460b220eeb7b9ac37bccae002"},
    {"WP_A_0911_s",512,128,10,71,"4b2c5981964d44cfb9a22a5e0c216461e5a729c812695eb1305c235bb23c3b62"},
    {"WP_A_1200_s",64,512,10,71,"a8562c76f0fd538795ca25023039f96734e28a39a54983c1474848c87245da03"},
    {"WP_A_1201_s",64,512,10,71,"661a2ce12f3c0b29e6a2c964a73e239256787e798296b65a1946e94e536bed84"},
    {"WP_A_1700_s",256,256,9,71,"533f7ca5f4c30d21de9d0313745b6578a150be1279a9333bf6bf8929b3fe4746"}
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
