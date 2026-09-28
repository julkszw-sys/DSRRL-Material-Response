#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/generated_dsr_flver_owner_tuples_v1.hpp"
#include "dsrrl/operators/material_response/generated_envspec_router_v1.hpp"
#include "dsrrl/operators/material_response/generated_routes_v1.hpp"
#include "dsrrl/operators/material_response/generated_exact_binding_mr_v1.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <algorithm>
#include <cstring>
#include <iterator>

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

struct direct_nospc_authority_match {
    const generated::envspec_router_record *record = nullptr;
    std::uint32_t ordinal = 0u;
};

std::optional<direct_nospc_authority_match>
resolve_direct_nospc_authority(
    const material_identity &identity) noexcept
{
    if (!identity.valid ||
        identity.semantic_name_hash == 0u ||
        digest_is_zero(identity.raw_mtd_sha256))
        return std::nullopt;

    std::optional<direct_nospc_authority_match> result;
    for (std::size_t i = 0u;
         i < generated::k_envspec_router_v1.size();
         ++i) {
        const auto &record =
            generated::k_envspec_router_v1[i];
        if (record.state !=
                generated::envspec_router_state::nospc_host ||
            record.semantic_name_hash !=
                identity.semantic_name_hash ||
            record.raw_mtd_sha256 !=
                identity.raw_mtd_sha256)
            continue;

        // Exact-name + exact raw-MTD identity must remain single-valued.
        // Any future duplicate/ambiguity fails open instead of selecting one.
        if (result.has_value())
            return std::nullopt;

        result = direct_nospc_authority_match{
            &record,
            static_cast<std::uint32_t>(i)
        };
    }

    return result;
}

float f32_from_bits(std::uint32_t bits) noexcept
{
    float value = 0.0f;
    static_assert(sizeof(value) == sizeof(bits),
                  "PTDE c100 bit carrier must be f32");
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

} // namespace

bool exact_runtime_material_response_identity(
    const material_identity &identity) noexcept
{
    namespace hashing =
        ::dsrrl::operators::legacy_plan::hashing;

    if (!identity.valid ||
        !identity.actual_material_exact ||
        identity.semantic_name_hash == 0u)
        return false;

    // Ordinary generated routes are ordered by route_index. Restrict runtime
    // authentication to the exact route cohort instead of rescanning the
    // entire MR corpus on every eligible draw. Duplicate route tags (route 5)
    // remain explicitly ambiguity-checked through semantic/raw identity.
    const generated::route_seed *match = nullptr;
    const auto ordinary_begin =
        std::lower_bound(
            std::begin(generated::k_material_routes_v1),
            std::end(generated::k_material_routes_v1),
            identity.route_index,
            [](const generated::route_seed &route,
               std::uint32_t route_index) {
                return route.route_index <
                    route_index;
            });

    for (auto it = ordinary_begin;
         it != std::end(
             generated::k_material_routes_v1) &&
         it->route_index ==
             identity.route_index;
         ++it) {
        const auto &route = *it;

        if (mtd_semantic_hash(route.mtd_name) !=
                identity.semantic_name_hash ||
            !hashing::matches_hex(
                identity.raw_mtd_sha256,
                route.sha256) ||
            mtd_semantic_hash(
                route.material_family) !=
                identity.material_family_hash)
            continue;

        if (match != nullptr)
            return false;

        match = &route;
    }

    if (match != nullptr)
        return true;

    const generated::exact_binding_mr_record
        *extension_match = nullptr;
    const auto extension_begin =
        std::lower_bound(
            generated::k_exact_binding_mr_v1.begin(),
            generated::k_exact_binding_mr_v1.end(),
            identity.route_index,
            [](const generated::exact_binding_mr_record &route,
               std::uint32_t route_index) {
                return route.route_tag <
                    route_index;
            });

    for (auto it = extension_begin;
         it != generated::k_exact_binding_mr_v1.end() &&
         it->route_tag ==
             identity.route_index;
         ++it) {
        const auto &route = *it;

        if (!route.runtime_mtd_allowed ||
            mtd_semantic_hash(route.mtd_name) !=
                identity.semantic_name_hash ||
            route.raw_mtd_sha256 !=
                identity.raw_mtd_sha256 ||
            mtd_semantic_hash(
                route.material_family) !=
                identity.material_family_hash)
            continue;

        if (extension_match != nullptr)
            return false;

        extension_match = &route;
    }

    return extension_match != nullptr;
}

bool exact_runtime_pmetal_material_identity(
    const material_identity &identity) noexcept
{
    constexpr std::uint32_t k_route = 345u;
    constexpr const char *k_name =
        "P_Metal[DSB].mtd";

    return
        exact_runtime_material_response_identity(
            identity) &&
        identity.route_index == k_route &&
        identity.semantic_name_hash ==
            mtd_semantic_hash(k_name);
}

bool material_response_island::register_receiver_recipe(const receiver_recipe &recipe)
{
    if (recipe.receiver_id == 0 ||
        recipe.certified_operations == response_none)
        return false;

    if (registration_finalized_.load(
            std::memory_order_acquire))
        return false;

    std::lock_guard lock(mutex_);
    if (registration_finalized_.load(
            std::memory_order_relaxed))
        return false;
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

    if (registration_finalized_.load(
            std::memory_order_acquire))
        return false;

    std::lock_guard lock(mutex_);
    if (registration_finalized_.load(
            std::memory_order_relaxed))
        return false;

    for (const auto &existing : material_profiles_) {
        if (existing.route_index != profile.route_index ||
            existing.semantic_name_hash != profile.semantic_name_hash)
            continue;

        return existing.raw_mtd_sha256 == profile.raw_mtd_sha256 &&
               existing.material_family_hash == profile.material_family_hash &&
               existing.c101 == profile.c101 &&
               existing.c100 == profile.c100 &&
               existing.ptde_specular_power == profile.ptde_specular_power &&
               existing.ptde_specular_power_verified == profile.ptde_specular_power_verified &&
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

bool material_response_island::finalize_registration()
{
    std::lock_guard lock(mutex_);

    if (registration_finalized_.load(
            std::memory_order_relaxed))
        return true;

    std::sort(
        receiver_recipes_.begin(),
        receiver_recipes_.end(),
        [](const receiver_recipe &a,
           const receiver_recipe &b) {
            return a.receiver_id <
                b.receiver_id;
        });

    std::sort(
        material_profiles_.begin(),
        material_profiles_.end(),
        [](const material_profile &a,
           const material_profile &b) {
            if (a.route_index !=
                b.route_index)
                return a.route_index <
                    b.route_index;
            if (a.semantic_name_hash !=
                b.semantic_name_hash)
                return a.semantic_name_hash <
                    b.semantic_name_hash;
            return a.raw_mtd_sha256 <
                b.raw_mtd_sha256;
        });

    registration_finalized_.store(
        true,
        std::memory_order_release);
    return true;
}

bool material_response_island::
registration_finalized() const noexcept
{
    return registration_finalized_.load(
        std::memory_order_acquire);
}

std::optional<receiver_recipe> material_response_island::find_receiver(
    std::uint32_t receiver_id) const
{
    if (registration_finalized_.load(
            std::memory_order_acquire)) {
        const auto it =
            std::lower_bound(
                receiver_recipes_.begin(),
                receiver_recipes_.end(),
                receiver_id,
                [](const receiver_recipe &recipe,
                   std::uint32_t id) {
                    return recipe.receiver_id < id;
                });

        if (it == receiver_recipes_.end() ||
            it->receiver_id != receiver_id)
            return std::nullopt;

        return *it;
    }

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

    auto first = material_profiles_.begin();
    const auto last = material_profiles_.end();
    const bool finalized =
        registration_finalized_.load(
            std::memory_order_acquire);

    if (finalized) {
        first = std::lower_bound(
            first,
            last,
            identity.route_index,
            [](const material_profile &profile,
               std::uint32_t route) {
                return profile.route_index < route;
            });
    }

    for (auto it = first; it != last; ++it) {
        const auto &profile = *it;

        if (profile.route_index != identity.route_index) {
            if (finalized &&
                profile.route_index > identity.route_index)
                break;
            continue;
        }
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

std::optional<material_profile>
material_response_island::resolve_material_unscoped(
    const material_identity &identity) const
{
    if (!identity.valid)
        return std::nullopt;

    std::optional<material_profile> result;

    auto first = material_profiles_.begin();
    const auto last = material_profiles_.end();
    const bool finalized =
        registration_finalized_.load(
            std::memory_order_acquire);

    if (finalized) {
        first = std::lower_bound(
            first,
            last,
            identity.route_index,
            [](const material_profile &profile,
               std::uint32_t route) {
                return profile.route_index < route;
            });
    }

    for (auto it = first; it != last; ++it) {
        const auto &profile = *it;

        if (profile.route_index != identity.route_index) {
            if (finalized &&
                profile.route_index > identity.route_index)
                break;
            continue;
        }

        if (profile.semantic_name_required &&
            (identity.semantic_name_hash == 0u ||
             identity.semantic_name_hash != profile.semantic_name_hash))
            continue;

        if (profile.semantic_name_hash != 0u &&
            identity.semantic_name_hash != 0u &&
            identity.semantic_name_hash != profile.semantic_name_hash)
            continue;

        if (!digest_is_zero(profile.raw_mtd_sha256) &&
            identity.raw_mtd_sha256 != profile.raw_mtd_sha256)
            continue;

        if (profile.material_family_hash != 0u &&
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
    const std::optional<material_identity> &material,
    bool ptde_companion_verified) const
{
    std::unique_lock<std::mutex> lock;
    if (!registration_finalized_.load(
            std::memory_order_acquire))
        lock = std::unique_lock<std::mutex>(
            mutex_);

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

    // Shared HemEnv hosts require a positive draw-local material authority.
    // FLVER+slot+MTD remains the asset-owner authority, but Material Response
    // itself is MTD-local: the retail MTD parser can independently prove an
    // exact certified MR profile by semantic name + full raw-MTD SHA. This
    // authority is intentionally not inherited by resource/asset operators.
    const bool flver_owner_authenticated =
        material->owner_tuple_exact &&
        material->material_slot_valid &&
        material->semantic_name_hash != 0u &&
        generated::dsr_flver_owner_tuple_authenticated(
            material->flver_sha256,
            material->material_slot,
            material->semantic_name_hash);

    const bool exact_runtime_material =
        exact_runtime_material_response_identity(
            *material);

    if (!flver_owner_authenticated &&
        !exact_runtime_material)
        return {
            false,
            decision_reason::owner_tuple_not_authenticated,
            receiver_id
        };

    const auto profile = resolve_material(*material, receiver_id);
    if (!profile.has_value())
        return {false, decision_reason::unknown_material, receiver_id};

    if (profile->ptde_companion_required &&
        !ptde_companion_verified)
        return {
            false,
            decision_reason::ptde_companion_required,
            receiver_id,
            profile->route_index
        };

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
        envspec,
        profile->c100,
        profile->ptde_specular_power,
        profile->ptde_specular_power_verified
    };
}

decision
material_response_island::evaluate_direct_pointlight_material(
    const material_identity &material,
    bool require_legacy_specular) const
{
    std::unique_lock<std::mutex> lock;
    if (!registration_finalized_.load(
            std::memory_order_acquire))
        lock = std::unique_lock<std::mutex>(
            mutex_);

    if (!material.valid)
        return {false, decision_reason::material_required, 0u};

    if (!material.owner_tuple_exact ||
        !material.material_slot_valid ||
        material.semantic_name_hash == 0u ||
        !generated::dsr_flver_owner_tuple_authenticated(
            material.flver_sha256,
            material.material_slot,
            material.semantic_name_hash))
        return {
            false,
            decision_reason::owner_tuple_not_authenticated,
            0u
        };

    const auto profile =
        resolve_material_unscoped(material);

    // The ordinary MR profile surface plus exact-binding extensions remain
    // separate from PointLight. Direct PointLight may additionally consume the exact
    // PTDE NoSpc c100 authority carried by the 325-row pairwise MTD router,
    // but only after the FLVER+slot owner tuple above has authenticated the
    // draw and only on an exact semantic-name + raw-MTD match.
    if (!profile.has_value()) {
        const auto nospc =
            resolve_direct_nospc_authority(material);
        if (!nospc.has_value())
            return {
                false,
                decision_reason::unknown_material,
                0u
            };

        if (require_legacy_specular)
            return {
                false,
                decision_reason::no_certified_operator,
                0u
            };

        std::array<float, 3> c100{};
        for (std::size_t i = 0u; i < c100.size(); ++i)
            c100[i] =
                f32_from_bits(
                    nospc->record->c100_bits[i]);

        // High-bit route tags are telemetry-only and cannot alias the normal
        // Material Response route namespace.
        const std::uint32_t route_tag =
            0x80000000u | nospc->ordinal;

        return {
            true,
            decision_reason::active,
            0u,
            route_tag,
            diffuse_material_domain_linear,
            0.0f,
            0u,
            7u,
            ptde_envspec_presence::absent,
            c100,
            0.0f,
            false
        };
    }

    // Exact-binding MR extensions are explicitly NO_USE for PointLight in
    // the semantic census. Registering their ordinary MR profile must not
    // broaden that authority into the direct PointLight island.
    if (generated::is_exact_binding_mr_route_tag(
            profile->route_index))
        return {
            false,
            decision_reason::no_certified_operator,
            0u,
            profile->route_index
        };

    const auto required =
        diffuse_material_domain_linear |
        (require_legacy_specular
             ? specular_factor_c101
             : response_none);

    if ((profile->certified_operations & required) != required ||
        (require_legacy_specular &&
         (!profile->ptde_specular_power_verified ||
          !(profile->ptde_specular_power > 0.0f))))
        return {
            false,
            decision_reason::no_certified_operator,
            0u,
            profile->route_index
        };

    return {
        true,
        decision_reason::active,
        0u,
        profile->route_index,
        required,
        profile->c101,
        profile->lod_min,
        profile->lod_max,
        profile->envspec,
        profile->c100,
        profile->ptde_specular_power,
        profile->ptde_specular_power_verified
    };
}

std::size_t material_response_island::receiver_recipe_count() const noexcept
{
    if (registration_finalized_.load(
            std::memory_order_acquire))
        return receiver_recipes_.size();

    std::lock_guard lock(mutex_);
    return receiver_recipes_.size();
}

std::size_t material_response_island::material_profile_count() const noexcept
{
    if (registration_finalized_.load(
            std::memory_order_acquire))
        return material_profiles_.size();

    std::lock_guard lock(mutex_);
    return material_profiles_.size();
}

} // namespace dsrrl::operators::material_response
