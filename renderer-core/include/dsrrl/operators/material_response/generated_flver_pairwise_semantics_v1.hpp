#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::material_response::generated {

// PTDE<->DSR FLVER owner-slot pairwise census.
// This is a NEGATIVE refinement after an independently exact positive PTDE
// capability match. A hash absent from the reject set is accepted only after
// the caller has already proven exact host MTD identity, exact FLVER/material
// ownership and positive PTDE semantic capability.
inline constexpr char k_flver_pairwise_dsr_zip_sha256[] =
    "a9a2e0eb48625fc735dfe18f7f375105cc5f56be005022969300c778489d0891";
inline constexpr char k_flver_pairwise_ptde_zip_sha256[] =
    "6651acb4fe995a69f1c84e5761b3d6bdcadeb39e1fc4d1087c3b82794f4d7281";
inline constexpr char k_flver_pairwise_full_census_json_sha256[] =
    "f9f186dafd9165fc3ac5740a0c5b18dde5e89914923011082ec7ac4e802c12ea";
inline constexpr char k_flver_pairwise_census_artifact_sha256[] =
    "beebc5d7dd55a9ffed363d634bff0d0dc690df748806ec89b8b37cf400b53362";

inline constexpr std::size_t k_flver_pairwise_dsr_material_count = 19985u;
inline constexpr std::size_t k_flver_pairwise_ptde_material_count = 21315u;
inline constexpr std::size_t k_flver_pairwise_dsr_mtd_count = 367u;
inline constexpr std::size_t k_flver_pairwise_ptde_mtd_count = 261u;
inline constexpr std::size_t k_flver_pairwise_overlap_mtd_count = 256u;
inline constexpr std::size_t k_flver_pairwise_mixed_overlap_mtd_count = 56u;
inline constexpr std::size_t k_flver_pairwise_stable_diffuse_mtd_count = 233u;
inline constexpr std::size_t k_flver_pairwise_stable_bump_mtd_count = 173u;
inline constexpr std::size_t k_flver_pairwise_stable_specular_mtd_count = 149u;

// These exact semantic-name hashes are the complete fail-open exceptions for
// the currently activated ownership-sensitive Diffuse and Normal/Bump gates.
// Five are PTDE-only MTD identities with no observed DSR owner slot; P[D].mtd
// additionally has no shared stable Diffuse resource. Ps_Wander_Ghost.mtd has
// no shared stable Bump resource.
inline constexpr std::array<std::uint64_t,6> k_flver_pairwise_diffuse_reject = {{
    0x9eef05fadbfab48aull, // P[D].mtd
    0x11e72450c49be97aull, // A10_Wet[DSB]_Alp.mtd (PTDE-only)
    0xe792ac9ec044628bull, // C_5290_Body[DSB][M].mtd (PTDE-only)
    0xba68ee604cd2ce87ull, // C_5290_Wing[DSB]_Alp.mtd (PTDE-only)
    0xc51d6e900d2da68eull, // C_RoughCloth[DSB]_Edge.mtd (PTDE-only)
    0x5b38314d307f5285ull  // S[DB].mtd (PTDE-only)
}};

inline constexpr std::array<std::uint64_t,6> k_flver_pairwise_bump_reject = {{
    0xf4de5bd53d6fe0fdull, // Ps_Wander_Ghost.mtd
    0x11e72450c49be97aull, // A10_Wet[DSB]_Alp.mtd (PTDE-only)
    0xe792ac9ec044628bull, // C_5290_Body[DSB][M].mtd (PTDE-only)
    0xba68ee604cd2ce87ull, // C_5290_Wing[DSB]_Alp.mtd (PTDE-only)
    0xc51d6e900d2da68eull, // C_RoughCloth[DSB]_Edge.mtd (PTDE-only)
    0x5b38314d307f5285ull  // S[DB].mtd (PTDE-only)
}};

template <std::size_t N>
constexpr bool pairwise_rejected(
    std::uint64_t semantic_name_hash,
    const std::array<std::uint64_t,N> &reject) noexcept
{
    if (semantic_name_hash == 0u)
        return true;
    for (const auto hash : reject)
        if (hash == semantic_name_hash)
            return true;
    return false;
}

constexpr bool flver_pairwise_diffuse_stable_after_ptde_positive(
    std::uint64_t semantic_name_hash) noexcept
{
    return !pairwise_rejected(
        semantic_name_hash, k_flver_pairwise_diffuse_reject);
}

constexpr bool flver_pairwise_bump_stable_after_ptde_positive(
    std::uint64_t semantic_name_hash) noexcept
{
    return !pairwise_rejected(
        semantic_name_hash, k_flver_pairwise_bump_reject);
}

} // namespace dsrrl::operators::material_response::generated
