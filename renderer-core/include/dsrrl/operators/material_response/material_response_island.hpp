#pragma once

#include "dsrrl/core/receiver_registry.hpp"
#include "dsrrl/core/types.hpp"

#include <array>
#include <atomic>
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
    ptde_companion_required,
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
    // Exact runtime material-object provenance recovered at the retail MTD
    // parser and joined back to the selector's actual material pointer.
    // This is a distinct authority from FLVER ownership and is intentionally
    // accepted only by explicitly scoped operator routes.
    bool actual_material_exact = false;
    std::uint32_t route_index = 0;
    std::uint64_t semantic_name_hash = 0;
    core::sha256_digest raw_mtd_sha256{};
    std::uint64_t material_family_hash = 0;
};

// Exact runtime MTD carrier for Material Response only. The retail MTD
// parser supplies semantic name + full raw-MTD bytes, so this authority can
// prove one of the certified MR profiles without requiring an FLVER owner.
// Resource/asset islands keep their own stricter owner/resource gates.
bool exact_runtime_material_response_identity(
    const material_identity &identity) noexcept;

// Narrow P_Metal specialization retained for EnvSpec and other explicitly
// scoped P_Metal consumers. Generic MR authority must not broaden those.
bool exact_runtime_pmetal_material_identity(
    const material_identity &identity) noexcept;

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
    float ptde_specular_power = 0.0f;
    bool ptde_specular_power_verified = false;
    std::uint8_t lod_min = 0;
    std::uint8_t lod_max = 7;
    std::array<std::uint32_t, 4> receiver_ids{};
    std::uint8_t receiver_count = 0;
    std::uint32_t certified_operations = response_none;
    ptde_envspec_presence envspec = ptde_envspec_presence::unknown;
    bool semantic_name_required = false;
    bool ptde_companion_required = false;
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
    float ptde_specular_power = 0.0f;
    bool ptde_specular_power_verified = false;
};

bool direct_pointlight_material_candidate(
    const material_identity &material,
    bool &is_spc) noexcept;

decision evaluate_direct_pointlight_material_identity(
    const material_identity &material,
    bool require_legacy_specular = true) noexcept;

class material_response_island {
public:
    bool register_receiver_recipe(const receiver_recipe &recipe);
    bool register_material_profile(const material_profile &profile);

    // Registration is an AddonInit-only construction phase. Finalization
    // sorts the immutable authority tables and publishes them with release
    // semantics, allowing the draw hot path to evaluate without the registry
    // mutex while preserving the exact same receiver/material gates.
    bool finalize_registration();
    bool registration_finalized() const noexcept;

    decision evaluate(
        std::uint32_t receiver_id,
        const std::optional<material_identity> &material,
        bool ptde_companion_verified = false) const;

    // Direct PTDE PointLight owns a material-local operator and must not borrow
    // the stable no-PointLight receiver namespace merely to recover authored
    // c100/c101/c102. This resolver authenticates the exact FLVER+slot+MTD
    // tuple and resolves one unique registered material profile independent of
    // receiver, requiring a verified PTDE specular-power donor.
    decision evaluate_direct_pointlight_material(
        const material_identity &material,
        bool require_legacy_specular = true) const;

    std::size_t receiver_recipe_count() const noexcept;
    std::size_t material_profile_count() const noexcept;

private:
    std::optional<receiver_recipe> find_receiver(std::uint32_t receiver_id) const;
    std::optional<material_profile> resolve_material(
        const material_identity &identity,
        std::uint32_t receiver_id) const;
    std::optional<material_profile> resolve_material_unscoped(
        const material_identity &identity) const;

    mutable std::mutex mutex_;
    std::vector<receiver_recipe> receiver_recipes_;
    std::vector<material_profile> material_profiles_;
    std::atomic_bool registration_finalized_{false};
};

} // namespace dsrrl::operators::material_response
