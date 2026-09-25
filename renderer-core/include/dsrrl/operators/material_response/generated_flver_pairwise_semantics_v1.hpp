#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace dsrrl::operators::material_response::generated {
inline constexpr char k_flver_pairwise_dsr_zip_sha256[]="a9a2e0eb48625fc735dfe18f7f375105cc5f56be005022969300c778489d0891";
inline constexpr char k_flver_pairwise_ptde_zip_sha256[]="6651acb4fe995a69f1c84e5761b3d6bdcadeb39e1fc4d1087c3b82794f4d7281";
inline constexpr std::size_t k_flver_pairwise_dsr_material_count=19985u;
inline constexpr std::size_t k_flver_pairwise_ptde_material_count=21315u;
inline constexpr std::size_t k_flver_pairwise_dsr_mtd_count=367u;
inline constexpr std::size_t k_flver_pairwise_ptde_mtd_count=261u;
inline constexpr std::size_t k_flver_pairwise_overlap_mtd_count=256u;

// Current pairwise evidence aggregates resource stability by MTD identity.
// It does not materialize a DSR (FLVER identity, material slot, MTD) owner
// table. A caller-provided "exact" bit is therefore insufficient evidence.
inline constexpr bool k_flver_pairwise_owner_tuple_authentication_available=false;

constexpr bool flver_pairwise_owner_tuple_authenticated(
    std::uint64_t,
    std::uint32_t,
    std::uint64_t) noexcept
{
    return false;
}
inline constexpr std::array<std::uint64_t,6> k_flver_pairwise_diffuse_reject = {{
    0x11e72450c49be97aull, // A10_Wet[DSB]_Alp.mtd
    0xe792ac9ec044628bull, // C_5290_Body[DSB][M].mtd
    0xba68ee604cd2ce87ull, // C_5290_Wing[DSB]_Alp.mtd
    0xc51d6e900d2da68eull, // C_RoughCloth[DSB]_Edge.mtd
    0x9eef05fadbfab48aull, // P[D].mtd
    0x5b38314d307f5285ull, // S[DB].mtd
}};
inline constexpr std::array<std::uint64_t,6> k_flver_pairwise_bump_reject = {{
    0x11e72450c49be97aull, // A10_Wet[DSB]_Alp.mtd
    0xe792ac9ec044628bull, // C_5290_Body[DSB][M].mtd
    0xba68ee604cd2ce87ull, // C_5290_Wing[DSB]_Alp.mtd
    0xc51d6e900d2da68eull, // C_RoughCloth[DSB]_Edge.mtd
    0xf4de5bd53d6fe0fdull, // Ps_Wander_Ghost.mtd
    0x5b38314d307f5285ull, // S[DB].mtd
}};
template<std::size_t N> constexpr bool pairwise_rejected(std::uint64_t h,const std::array<std::uint64_t,N>&a) noexcept{if(!h)return true;for(auto x:a)if(x==h)return true;return false;}
constexpr bool flver_pairwise_diffuse_stable_after_ptde_positive(std::uint64_t h) noexcept{return !pairwise_rejected(h,k_flver_pairwise_diffuse_reject);}
constexpr bool flver_pairwise_bump_stable_after_ptde_positive(std::uint64_t h) noexcept{return !pairwise_rejected(h,k_flver_pairwise_bump_reject);}
} // namespace dsrrl::operators::material_response::generated
