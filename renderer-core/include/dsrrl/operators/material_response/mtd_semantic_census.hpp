#pragma once

#include "dsrrl/operators/material_response/material_response_island.hpp"

#include <cstdint>

namespace dsrrl::operators::material_response {

enum class mtd_semantic_state : std::uint8_t {
    unknown = 0,
    use,
    no_use
};

enum class mtd_semantic_operator : std::uint8_t {
    material_response = 0,
    spec_rgb,
    env_spec,
    subsurface,
    diffuse,
    normal_bump,
    upper_lower,
    hemenv,
    hemenv_lerp,
    pointlight,
    alpha_blend,
    parallax,
    emissive_lightmap,
    texture_resource_consumers
};

enum class mtd_semantic_source : std::uint8_t {
    none = 0,
    full24_exact_cohort,
    exact_override,
    exact_binding_extension,
    envspec_router_exact
};

enum class mtd_gate_policy : std::uint8_t {
    none = 0,
    exact_material,
    direct_exact,
    ptde_companion_required
};

struct mtd_semantic_query {
    material_identity material{};
    std::uint32_t receiver_id = 0;
};

struct mtd_semantic_decision {
    mtd_semantic_state state = mtd_semantic_state::unknown;
    mtd_semantic_source source = mtd_semantic_source::none;
    mtd_gate_policy gate_policy = mtd_gate_policy::none;
    bool exact_identity_match = false;
};

enum class mtd_envspec_router_state : std::uint8_t {
    unknown = 0,
    present,
    explicit_none,
    nospc_host
};

struct mtd_envspec_semantics {
    ptde_envspec_presence presence = ptde_envspec_presence::unknown;
    mtd_envspec_router_state router_state =
        mtd_envspec_router_state::unknown;
    bool exact_identity_match = false;
    bool suppress_dsr_only_safe = false;
    bool envspc_slot_valid = false;
    std::uint8_t envspc_slot = 0;
};

std::uint64_t mtd_semantic_hash(const char *text) noexcept;

mtd_semantic_decision classify_mtd_semantic(
    const mtd_semantic_query &query,
    mtd_semantic_operator op) noexcept;

// Exact material-level EnvSpec semantics reconstructed from the historical
// DSREMR01 325-record router. PTDE semantic presence is intentionally
// separated from carrier authorization: EXPLICIT_NONE may be semantically
// absent while suppression remains unsafe and therefore fail-open.
mtd_envspec_semantics classify_mtd_envspec_semantics(
    const mtd_semantic_query &query) noexcept;

// Runtime parser equivalent of the exact router identity. Retail DSR provides
// the UTF-16 BND entry name concurrently with raw MTD bytes at 0x140295ED0.
// The generated router preserves the historical lowercase UTF-16 FNV-1a key,
// so runtime does not need a lossy name conversion.
mtd_envspec_semantics classify_mtd_envspec_semantics_legacy(
    std::uint64_t legacy_name_hash_utf16_lower,
    const core::sha256_digest &raw_mtd_sha256) noexcept;

ptde_envspec_presence mtd_envspec_presence(
    const mtd_semantic_query &query) noexcept;

} // namespace dsrrl::operators::material_response
