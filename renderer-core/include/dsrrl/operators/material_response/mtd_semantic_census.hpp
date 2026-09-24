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
    exact_binding_extension
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

std::uint64_t mtd_semantic_hash(const char *text) noexcept;

mtd_semantic_decision classify_mtd_semantic(
    const mtd_semantic_query &query,
    mtd_semantic_operator op) noexcept;

ptde_envspec_presence mtd_envspec_presence(
    const mtd_semantic_query &query) noexcept;

} // namespace dsrrl::operators::material_response
