#pragma once

#include "dsrrl/core/receiver_registry.hpp"
#include "dsrrl/core/types.hpp"

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace dsrrl::operators::material_response {

enum class material_scope_policy : std::uint8_t {
    global_receiver_safe = 0,
    exact_material_required
};

enum class ptde_envspec_presence : std::uint8_t {
    unknown = 0,
    absent,
    present
};

enum material_response_operation : std::uint32_t {
    response_none = 0,
    diffuse_material_domain_linear = 1u << 0,
    specular_factor_c101 = 1u << 1
};

enum class decision_reason : std::uint8_t {
    active = 0,
    unknown_receiver,
    material_required,
    owner_tuple_not_authenticated,
    unknown_material,
    receiver_material_mismatch,
    no_certified_operator
};

struct material_identity {
    bool valid = false;
    // Actual draw-owner provenance. These fields must come from the observed
    // DSR FLVER/material binding, never from an MTD-only inference.
    // Full content digest is authoritative; the legacy 64-bit token is kept
    // only for compatibility with older census APIs and is never sufficient
    // for positive Material Response activation.
    core::sha256_digest flver_sha256{};
    std::uint64_t flver_identity_hash = 0;
    std::uint32_t material_slot = 0;
    bool material_slot_valid = false;
    bool owner_tuple_exact = false;
    std::uint32_t route_index = 0;
    std::uint64_t semantic_name_hash = 0;
    core::sha256_digest raw_mtd_sha256{};
    std::uint64_t material_family_hash = 0;
};

struct receiver_recipe {
    std::uint32_t receiver_id = 0;
    material_scope_policy scope = material_scope_policy::exact_material_required;
    std::uint32_t certified_operations = response_none;
    ptde_envspec_presence envspec = ptde_envspec_presence::unknown;
};

struct material_profile {
    std::uint32_t route_index = 0;
    std::uint64_t semantic_name_hash = 0;
    core::sha256_digest raw_mtd_sha256{};
    std::uint64_t material_family_hash = 0;
    float c101 = 1.0f;
    std::array<float, 3> c100{{1.0f, 1.0f, 1.0f}};
    std::array<float, 3> c101_f0q{{1.0f, 1.0f, 1.0f}};
    std::uint8_t lod_min = 0;
    std::uint8_t lod_max = 7;
    std::array<std::uint32_t, 4> receiver_ids{};
    std::uint8_t receiver_count = 0;
    std::uint32_t certified_operations = response_none;
    ptde_envspec_presence envspec = ptde_envspec_presence::unknown;
    bool semantic_name_required = false;
};

struct decision {
    bool active = false;
    decision_reason reason = decision_reason::unknown_receiver;
    std::uint32_t receiver_id = 0;
    std::uint32_t route_index = 0;
    std::uint32_t certified_operations = response_none;
    float c101 = 1.0f;
    std::uint8_t lod_min = 0;
    std::uint8_t lod_max = 7;
    ptde_envspec_presence envspec = ptde_envspec_presence::unknown;
    std::array<float, 3> c100{{1.0f, 1.0f, 1.0f}};
    std::array<float, 3> c101_f0q{{1.0f, 1.0f, 1.0f}};
};

class material_response_island {
public:
    bool register_receiver_recipe(const receiver_recipe &recipe);
    bool register_material_profile(const material_profile &profile);

    decision evaluate(
        std::uint32_t receiver_id,
        const std::optional<material_identity> &material) const;

    std::size_t receiver_recipe_count() const noexcept;
    std::size_t material_profile_count() const noexcept;

private:
    std::optional<receiver_recipe> find_receiver(std::uint32_t receiver_id) const;
    std::optional<material_profile> resolve_material(
        const material_identity &identity,
        std::uint32_t receiver_id) const;

    mutable std::mutex mutex_;
    std::vector<receiver_recipe> receiver_recipes_;
    std::vector<material_profile> material_profiles_;
};

} // namespace dsrrl::operators::material_response
