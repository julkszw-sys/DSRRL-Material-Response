#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/generated_flver_pairwise_semantics_v1.hpp"

#include <algorithm>

namespace dsrrl::operators::material_response {
namespace {

bool digest_is_zero(const core::sha256_digest &digest) noexcept
{
    return std::all_of(
        digest.begin(),
        digest.end(),
        [](std::uint8_t value) { return value == 0; });
}

bool receiver_allowed(const material_profile &profile, std::uint32_t receiver_id) noexcept
{
    if (profile.receiver_count == 0)
        return true;

    const std::uint8_t count = profile.receiver_count < profile.receiver_ids.size()
        ? profile.receiver_count
        : static_cast<std::uint8_t>(profile.receiver_ids.size());

    for (std::uint8_t i = 0; i < count; ++i)
        if (profile.receiver_ids[i] == receiver_id)
            return true;

    return false;
}

} // namespace

bool material_response_island::register_receiver_recipe(const receiver_recipe &recipe)
{
    if (recipe.receiver_id == 0 ||
        recipe.certified_operations == response_none)
        return false;

    std::lock_guard lock(mutex_);
    for (const auto &existing : receiver_recipes_) {
        if (existing.receiver_id != recipe.receiver_id)
            continue;

        return existing.scope == recipe.scope &&
               existing.certified_operations == recipe.certified_operations &&
               existing.envspec == recipe.envspec;
    }

    receiver_recipes_.push_back(recipe);
    return true;
}

bool material_response_island::register_material_profile(const material_profile &profile)
{
    if (profile.receiver_count > profile.receiver_ids.size() ||
        profile.certified_operations == response_none)
        return false;

    std::lock_guard lock(mutex_);

    for (const auto &existing : material_profiles_) {
        if (existing.route_index != profile.route_index ||
            existing.semantic_name_hash != profile.semantic_name_hash)
            continue;

        return existing.raw_mtd_sha256 == profile.raw_mtd_sha256 &&
               existing.material_family_hash == profile.material_family_hash &&
               existing.c101 == profile.c101 &&
               existing.lod_min == profile.lod_min &&
               existing.lod_max == profile.lod_max &&
               existing.receiver_ids == profile.receiver_ids &&
               existing.receiver_count == profile.receiver_count &&
               existing.certified_operations == profile.certified_operations &&
               existing.envspec == profile.envspec &&
               existing.semantic_name_required == profile.semantic_name_required;
    }

    material_profiles_.push_back(profile);
    return true;
}

std::optional<receiver_recipe> material_response_island::find_receiver(
    std::uint32_t receiver_id) const
{
    std::optional<receiver_recipe> result;

    for (const auto &recipe : receiver_recipes_) {
        if (recipe.receiver_id != receiver_id)
            continue;

        if (result.has_value())
            return std::nullopt;

        result = recipe;
    }

    return result;
}

std::optional<material_profile> material_response_island::resolve_material(
    const material_identity &identity,
    std::uint32_t receiver_id) const
{
    if (!identity.valid)
        return std::nullopt;

    std::optional<material_profile> result;

    for (const auto &profile : material_profiles_) {
        if (profile.route_index != identity.route_index)
            continue;
        if (!receiver_allowed(profile, receiver_id))
            continue;

        if (profile.semantic_name_required &&
            (identity.semantic_name_hash == 0 ||
             identity.semantic_name_hash != profile.semantic_name_hash))
            continue;

        if (profile.semantic_name_hash != 0 &&
            identity.semantic_name_hash != 0 &&
            identity.semantic_name_hash != profile.semantic_name_hash)
            continue;

        if (!digest_is_zero(profile.raw_mtd_sha256) &&
            identity.raw_mtd_sha256 != profile.raw_mtd_sha256)
            continue;

        if (profile.material_family_hash != 0 &&
            identity.material_family_hash != profile.material_family_hash)
            continue;

        if (result.has_value())
            return std::nullopt;

        result = profile;
    }

    return result;
}

decision material_response_island::evaluate(
    std::uint32_t receiver_id,
    const std::optional<material_identity> &material) const
{
    std::lock_guard lock(mutex_);

    const auto recipe = find_receiver(receiver_id);
    if (!recipe.has_value())
        return {false, decision_reason::unknown_receiver, receiver_id};

    if (recipe->scope == material_scope_policy::global_receiver_safe) {
        return {
            true,
            decision_reason::active,
            receiver_id,
            0,
            recipe->certified_operations,
            1.0f,
            0,
            7,
            recipe->envspec
        };
    }

    if (!material.has_value() || !material->valid)
        return {false, decision_reason::material_required, receiver_id};

    // Positive per-material activation is draw-specific. MTD/profile identity
    // alone is insufficient: authenticate the actual FLVER owner + slot + MTD.
    if (!material->owner_tuple_exact ||
        material->flver_identity_hash == 0u ||
        !material->material_slot_valid ||
        material->semantic_name_hash == 0u ||
        !generated::flver_pairwise_owner_tuple_authenticated(
            material->flver_identity_hash,
            material->material_slot,
            material->semantic_name_hash))
        return {
            false,
            decision_reason::owner_tuple_not_authenticated,
            receiver_id
        };

    const auto profile = resolve_material(*material, receiver_id);
    if (!profile.has_value())
        return {false, decision_reason::unknown_material, receiver_id};

    const std::uint32_t operations =
        recipe->certified_operations & profile->certified_operations;

    if (operations == response_none) {
        return {
            false,
            decision_reason::no_certified_operator,
            receiver_id,
            profile->route_index
        };
    }

    const ptde_envspec_presence envspec =
        profile->envspec != ptde_envspec_presence::unknown
        ? profile->envspec
        : recipe->envspec;

    return {
        true,
        decision_reason::active,
        receiver_id,
        profile->route_index,
        operations,
        profile->c101,
        profile->lod_min,
        profile->lod_max,
        envspec
    };
}

std::size_t material_response_island::receiver_recipe_count() const noexcept
{
    std::lock_guard lock(mutex_);
    return receiver_recipes_.size();
}

std::size_t material_response_island::material_profile_count() const noexcept
{
    std::lock_guard lock(mutex_);
    return material_profiles_.size();
}

} // namespace dsrrl::operators::material_response
