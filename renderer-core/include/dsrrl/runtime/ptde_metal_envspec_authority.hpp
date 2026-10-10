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
    cmetal,
    spc_route_0,
    spc_route_1,
    spc_route_4,
    spc_route_6,
    spc_route_7,
    spc_route_8,
    spc_route_9,
    spc_route_12,
    spc_route_13,
    spc_route_14,
    spc_route_15,
    spc_route_17,
    spc_route_41,
    spc_route_44,
    spc_route_88,
    spc_route_167,
    spc_route_197,
    spc_route_231,
    spc_route_276,
    spc_route_331,
    spc_route_359
};

struct exact_metal_envspec_authority {
    ptde_metal_envspec_profile profile;
    std::uint32_t route_index;
    const char *mtd_name;
    const char *raw_mtd_sha256;
    float c101;
    std::uint8_t envspc_slot;
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
     2.5f, 2u},
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    {ptde_metal_envspec_profile::pmetal_alp, 2u,
     "P_Metal[DSB]_Alp.mtd",
     "45b985a46381199775b0089e9ac6ad5ea14a97b215421bfe29c29bd7ad97535a",
     2.5f, 2u},
    {ptde_metal_envspec_profile::pmetal_edge, 5u,
     "P_Metal[DSB]_Edge.mtd",
     "8b730ce8655401c5cae694ba5535efb088d9d67db2a157d8f142cb5629293485",
     2.5f, 2u},
    {ptde_metal_envspec_profile::cmetal, 229u,
     "C_Metal[DSB].mtd",
     "ae2e8df867fe2859eef37104c13c939e7e7fe4f703b7fe49c408d0230fc71d85",
     2.5f, 2u},
#endif
#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
    // Opt-in only. Exact original MTD SHA, FLVER/slot, receiver, native
    // SPX homology and operator-local resource binders remain mandatory.
    // Excludes Body, Lit, Mul, SHA-ambiguous DullLeather and S_Metal alias.
    {ptde_metal_envspec_profile::spc_route_0, 0u,
     "P_Leather[DSB]_Alp.mtd",
     "4c728a9b5957a75d0eb82b2b77b800829e1973632c7c0690a7e31a074c85e7fb",
     1.5f, 1u},
    {ptde_metal_envspec_profile::spc_route_1, 1u,
     "P_Leather[DSB]_Edge.mtd",
     "cf968a22722667777d10429a03a2e4a87f867412949a7fe5590cb8cefaa2bfa4",
     1.5f, 1u},
    {ptde_metal_envspec_profile::spc_route_4, 4u,
     "C_2320_Metal[DSB].mtd",
     "03d46faee57fd70c3af82939802bb1c2115aa62f80003346512276d1b9bd5e0a",
     2.5f, 2u},
    {ptde_metal_envspec_profile::spc_route_6, 6u,
     "P_RoughCloth[DSB].mtd",
     "db197a28abd96f55ce54da65dfea74e8788464dd6ae13ca7ceee038c56d281b1",
     1.0f, 0u},
    {ptde_metal_envspec_profile::spc_route_7, 7u,
     "P_RoughCloth[DSB]_Edge.mtd",
     "4b3451c1d83d31f5ac1aad11bb5a16985ce49928bca3fcc10005032c784e45b8",
     1.0f, 0u},
    {ptde_metal_envspec_profile::spc_route_8, 8u,
     "P_Wet[DSB].mtd",
     "7af76c9ed5adbb22a574b2afa382a10dfec37dc17470a3d9ea47be3bfb1ea97b",
     2.0f, 3u},
    {ptde_metal_envspec_profile::spc_route_9, 9u,
     "P_Wet[DSB]_Edge.mtd",
     "a7f73fd656797fea6eeb59131a5bd5c25198ad5c654524dadf76798c57aebb86",
     2.0f, 3u},
    {ptde_metal_envspec_profile::spc_route_12, 12u,
     "C_Leather[DSB]_Alp.mtd",
     "9ef69c874a7ceff3797a3513026677d131f622f3f8476015a5ce4b70fcd6e190",
     1.5f, 1u},
    {ptde_metal_envspec_profile::spc_route_13, 13u,
     "C_Metal[DSB]_Alp.mtd",
     "45eb950965998f67d2d2b62fbcfc841f07e71e24097e1c6ceb97758752a651f0",
     2.5f, 2u},
    {ptde_metal_envspec_profile::spc_route_14, 14u,
     "C_RoughCloth[DSB].mtd",
     "9885c34b18f247b9f4446dbaeddef427e2058597d58e6297a1b9e6e8e21b8e51",
     1.0f, 0u},
    {ptde_metal_envspec_profile::spc_route_15, 15u,
     "C_RoughCloth[DSB]_Edge.mtd",
     "0a29732f36dc7ba939bd0114c02af09e5b72a11745c76f90e9ed4991752994bd",
     1.0f, 0u},
    {ptde_metal_envspec_profile::spc_route_17, 17u,
     "C_Wet[DSB]_Alp.mtd",
     "9c46069d7ee8c7e964704d5e805594d1cdf0263123e0f68c65ff38ef4420e7d9",
     2.0f, 3u},
    {ptde_metal_envspec_profile::spc_route_41, 41u,
     "C_Leather[DSB].mtd",
     "236bb0a0614e4bf3326d610915f8b22c3cacb09ee1c0523a51fa08200dfbe524",
     1.5f, 1u},
    {ptde_metal_envspec_profile::spc_route_44, 44u,
     "C_Leather[DSB]_Edge.mtd",
     "25ffd8635203147f291c8f8af945dbad1898a1b5dfdcf31bb4136083fbda4d39",
     1.5f, 1u},
    {ptde_metal_envspec_profile::spc_route_88, 88u,
     "P_DullLeather[DSB]_Edge.mtd",
     "44e1188bafa99d49349883dcc2052c13365f196e341ca5e7c788cbad0a4e058d",
     1.0f, 0u},
    {ptde_metal_envspec_profile::spc_route_167, 167u,
     "C_3530_Unique_Wet[DSB]_Edge.mtd",
     "833bb7b09f0dd02a61c75e9edbb76ad5ec14bdab44d663c1c652f53963eee963",
     2.0f, 3u},
    {ptde_metal_envspec_profile::spc_route_197, 197u,
     "C_Wet[DSB]_Edge.mtd",
     "94fb44042dee45d9b022b51ee5a030382ad7d4b7b10d7ec44e213f06fa1f377a",
     2.0f, 3u},
    {ptde_metal_envspec_profile::spc_route_231, 231u,
     "C_3530_Unique_Wet[DSB].mtd",
     "ae7f795b03dcb2c204948e9a8e3b2798abe1c9b8912812e3817c095b07e5cd8f",
     2.0f, 3u},
    {ptde_metal_envspec_profile::spc_route_276, 276u,
     "C_DullLeather[DSB]_Alp.mtd",
     "c1b1baca69cc33b449f7ff06bb08a959a1b74c90862d67acd003194062306dff",
     1.0f, 0u},
    {ptde_metal_envspec_profile::spc_route_331, 331u,
     "C_DullLeather[DSB]_Edge.mtd",
     "e2d819b90e8201a93e67e84e1179e9c3bf3f57988d5f86b44c5e024d05dee1a1",
     1.0f, 0u},
    {ptde_metal_envspec_profile::spc_route_359, 359u,
     "C_Metal[DSB]_Edge.mtd",
     "f797c63ac4f476ab7ebb7bc150ecc2a9c61851999f5051ceb864b62c95d924d9",
     2.5f, 2u},
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
            // The experimental SPC batch is only homologous in the exact
            // DifSpcBmp receiver family, never in Lit/Mul/Body branches.
            (candidate.profile < ptde_metal_envspec_profile::spc_route_0 ||
             material.material_family_hash ==
                 mr::mtd_semantic_hash("DifSpcBmp")) &&
            material.semantic_name_hash ==
                mr::mtd_semantic_hash(candidate.mtd_name) &&
            hashing::matches_hex(material.raw_mtd_sha256,
                                 candidate.raw_mtd_sha256)) {
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC) || defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
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
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC) || defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
    return is_experimental_ptde_metal_envspec_material(material);
#else
    return false;
#endif
}

} // namespace dsrrl::runtime
