#pragma once
// v2.0.3-dev descendant: exact equipment metal MTD receiver authority.
// The additional profiles are DIAGNOSTIC ONLY and require a matching
// producer/material/receiver/resource/consumer chain at draw time.
// Do not infer authorization from "Metal" name, source RGB or shared shader.
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <cstdint>

namespace dsrrl::runtime {

enum class ptde_metal_envspec_profile : std::uint8_t {
    none = 0,
    pmetal_baseline,
    pmetal_alp,
    pmetal_edge,
    cmetal
};

struct exact_metal_envspec_authority {
    ptde_metal_envspec_profile profile;
    std::uint32_t route_index;
    const char *mtd_name;
    const char *raw_mtd_sha256;
    float c101;
};

// Exact raw-MTD-SHA+semantic-name+MR-route authority. All four source
// records use the same PTDE/DSR FRPG_Phn_ColDifSpcBmp shader family and
// explicit EnvSpec slot 2 according to the independent MTD/SPX census.
// This MTD table is necessary, NOT sufficient: the caller must separately
// verify live FLVER+material-slot, shader receiver, operator-local source,
// PTDE SpecRGB companion and probe A/B consumer.
constexpr exact_metal_envspec_authority k_ptde_metal_envspec_profiles[] = {
    {ptde_metal_envspec_profile::pmetal_baseline, 345u,
     "P_Metal[DSB].mtd",
     "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b",
     2.5f},
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    {ptde_metal_envspec_profile::pmetal_alp, 2u,
     "P_Metal[DSB]_Alp.mtd",
     "45b985a46381199775b0089e9ac6ad5ea14a97b215421bfe29c29bd7ad97535a",
     2.5f},
    {ptde_metal_envspec_profile::pmetal_edge, 5u,
     "P_Metal[DSB]_Edge.mtd",
     "8b730ce8655401c5cae694ba5535efb088d9d67db2a157d8f142cb5629293485",
     2.5f},
    {ptde_metal_envspec_profile::cmetal, 229u,
     "C_Metal[DSB].mtd",
     "ae2e8df867fe2859eef37104c13c939e7e7fe4f703b7fe49c408d0230fc71d85",
     2.5f},
#endif
};

inline const exact_metal_envspec_authority *match_ptde_metal_envspec_material(
    const operators::material_response::material_identity &material) noexcept
{
    namespace mr = operators::material_response;
    namespace hashing = operators::legacy_plan::hashing;
    if (!material.valid || !material.owner_tuple_exact ||
        !material.material_slot_valid)
        return nullptr;
    for (const auto &candidate : k_ptde_metal_envspec_profiles) {
        if (material.route_index == candidate.route_index &&
            material.semantic_name_hash ==
                mr::mtd_semantic_hash(candidate.mtd_name) &&
            hashing::matches_hex(material.raw_mtd_sha256,
                                 candidate.raw_mtd_sha256)) {
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
            if (candidate.profile != ptde_metal_envspec_profile::pmetal_baseline) {
                // An asserted owner flag is insufficient when the full
                // source FLVER digest is missing. Require the real digest;
                // hash-only legacy identity tokens are never authority.
                bool exact_flver_digest_present = false;
                for (const auto byte : material.flver_sha256)
                    exact_flver_digest_present |= byte != 0u;
                if (!exact_flver_digest_present)
                    return nullptr;
            }
#endif
            return &candidate;
        }
    }
    return nullptr;
}

inline bool is_experimental_ptde_metal_envspec_material(
    const operators::material_response::material_identity &material) noexcept
{
    const auto *authority = match_ptde_metal_envspec_material(material);
    return authority != nullptr &&
           authority->profile != ptde_metal_envspec_profile::pmetal_baseline;
}

// This decision is executed by the REAL retail FLVER selector hook, not just
// the later EnvSpec shader consumer. Preserve the original route345 dispatch
// (the exact MTD check occurs inside its producer), while gating every new
// route by a complete exact-material match and an explicit opt-in flag.
inline bool should_dispatch_ptde_metal_selector_source(
    const operators::material_response::material_identity &material) noexcept
{
    if (material.route_index == 345u)
        return true;
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    return is_experimental_ptde_metal_envspec_material(material);
#else
    return false;
#endif
}

} // namespace dsrrl::runtime
