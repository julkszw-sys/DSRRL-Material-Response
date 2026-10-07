#pragma once

#include "dsrrl/core/types.hpp"
#include "dsrrl/core/feature_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::point_light {

enum class clustered_pnts_rowaware_attenuation_result : std::uint8_t {
    applied = 0,
    pass_not_candidate,
    pass_unknown_exact_sha,
    fail_invalid_dxbc,
    fail_patch_precondition,
    fail_rebuild,
    fail_final
};

struct clustered_pnts_rowaware_attenuation_outcome {
    clustered_pnts_rowaware_attenuation_result result =
        clustered_pnts_rowaware_attenuation_result::pass_not_candidate;
    core::sha256_digest host_sha256{};
    core::sha256_digest replacement_sha256{};
    std::size_t host_size = 0u;
    std::size_t replacement_size = 0u;
    std::uint32_t representative_shader_index = 0u;
};

enum class clustered_pnts_direct_materialize_result : std::uint8_t {
    applied = 0,
    pass_not_candidate,
    pass_unknown_exact_sha,
    fail_invalid_dxbc,
    fail_patch_precondition,
    fail_rebuild,
    fail_final_sha
};

struct clustered_pnts_direct_materialize_outcome {
    clustered_pnts_direct_materialize_result result =
        clustered_pnts_direct_materialize_result::pass_not_candidate;
    core::sha256_digest host_sha256{};
    core::sha256_digest replacement_sha256{};
    std::size_t host_size = 0u;
    std::size_t replacement_size = 0u;
    std::uint32_t representative_shader_index = 0u;
    bool spc = false;
    bool blended_material = false;

    // Exact create-time operators already baked into the direct replacement
    // bytecode. These are shader ownership only: their stock inputs remain
    // host-owned unless another draw-local bridge explicitly replaces them.
    core::operator_mask composed_shader_owners = 0u;

    // The historical direct-PTDE journal was authored against the original
    // PointLight-specific b12 ABI. Runtime-v2 shares the post-reset MR carrier,
    // so materialization must migrate the exact inserted operands before the
    // replacement can be registered.
    bool current_b12_abi = false;

    // Spc only: true iff the exact direct-PTDE legacy local-specular body and
    // its current b12 c101/c102 material operands are both attested. Runtime
    // must fail open on Spc unless this bit survives candidate registration.
    bool legacy_specular_complete = false;
};

constexpr core::operator_mask
clustered_pnts_required_composed_shader_owners(
    bool spc) noexcept
{
    auto owners =
        core::operator_bit(
            core::operator_id::diffuse_material_domain) |
        core::operator_bit(
            core::operator_id::pointlight_pnts_attenuation) |
        core::operator_bit(
            core::operator_id::terminal_sat_rgb);

    if (!spc)
        owners |= core::operator_bit(
            core::operator_id::envspec_nospc_delete);

    return owners;
}

// Deterministic same-length migration used after byte-exact historical
// journal attestation. Exposed for regression tests so future b12 layout
// changes cannot silently strand clustered PointLight again.
bool migrate_clustered_pnts_legacy_b12_words(
    std::vector<std::uint32_t> &words,
    bool spc) noexcept;

clustered_pnts_rowaware_attenuation_outcome
materialize_clustered_pnts_rowaware_attenuation(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

clustered_pnts_direct_materialize_outcome
materialize_clustered_pnts_direct_ptde(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::point_light
