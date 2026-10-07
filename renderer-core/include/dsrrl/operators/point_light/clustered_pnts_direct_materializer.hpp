#pragma once

#include "dsrrl/core/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::point_light {

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


enum class clustered_pnts_marker_attenuation_result : std::uint8_t {
    applied = 0,
    pass_not_candidate,
    pass_unknown_exact_sha,
    fail_invalid_dxbc,
    fail_identity_precondition,
    fail_post_a1_pattern,
    fail_rebuild
};

struct clustered_pnts_marker_attenuation_identity {
    core::sha256_digest host_sha256{};
    std::size_t host_size = 0u;
    std::uint32_t representative_shader_index = 0u;
    bool spc = false;
    std::array<std::uint32_t,11> t18_offset0_load{};
    std::array<std::uint32_t,7> square_mul{};
    std::array<std::uint32_t,7> cubic_mul_sat{};
    bool exact = false;
};

struct clustered_pnts_marker_attenuation_outcome {
    clustered_pnts_marker_attenuation_result result =
        clustered_pnts_marker_attenuation_result::pass_not_candidate;
    core::sha256_digest host_sha256{};
    core::sha256_digest replacement_sha256{};
    std::size_t host_size = 0u;
    std::size_t replacement_size = 0u;
    std::uint32_t representative_shader_index = 0u;
};

// First stage: authenticate the original stock PntS body by the existing
// full36 exact-SHA corpus and capture the exact local-light instruction
// identities before any other create-time island changes the shader.
clustered_pnts_marker_attenuation_result
identify_clustered_pnts_marker_attenuation(
    const std::uint8_t *source,
    std::size_t size,
    clustered_pnts_marker_attenuation_identity &identity) noexcept;

// Second stage: after A1/MR create-time composition, locate the unchanged
// authenticated local-light instructions and add a per-light t18+0x28 gate.
// marker=1 selects PTDE SAT(x); marker=0 preserves stock DSR SAT(x^3).
clustered_pnts_marker_attenuation_outcome
materialize_clustered_pnts_marker_attenuation(
    const clustered_pnts_marker_attenuation_identity &identity,
    const std::uint8_t *post_a1_source,
    std::size_t post_a1_size,
    std::vector<std::uint8_t> &output) noexcept;

clustered_pnts_direct_materialize_outcome
materialize_clustered_pnts_direct_ptde(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::point_light
