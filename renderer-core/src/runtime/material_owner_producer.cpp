#include "dsrrl/runtime/material_owner_producer.hpp"
#include "dsrrl/operators/material_response/generated_dsr_flver_owner_tuples_v1.hpp"
#include "dsrrl/operators/material_response/generated_dsr_mtd_identity_v1.hpp"
#include "dsrrl/operators/material_response/generated_dsr_mtd_identity_supplement_v1.hpp"
#include "dsrrl/operators/material_response/generated_routes_v1.hpp"
#include "dsrrl/operators/material_response/generated_exact_binding_mr_v1.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

namespace dsrrl::runtime {
namespace {

#if defined(DSRRL_V334_WANDERER_9530_DULLLEATHER_MR_OWNER_FIX)
// Source-attested exact owner of the one non-metal regression comparator.
// The old generic DSR MTD registry maps this same semantic name to
// stale digest 8a577e8e..., which does NOT match the retail 7ea7d6ce...
// and therefore silently loses certified Material Response route 160.
// Only this complete owner+slot+semantic tuple may bypass that stale row;
// other P_DullLeather owners, all P_Leather and all PointLight remain stock.
constexpr core::sha256_digest k_v334_wanderer_model_sha = {
    0x13u, 0x98u, 0xeeu, 0xc9u, 0xe1u, 0x54u, 0x5fu, 0xb5u, 0x26u, 0x88u, 0xc6u, 0x91u, 0xd1u, 0xffu, 0x3cu, 0xaau, 0x04u, 0x2du, 0xe3u, 0x75u, 0x0bu, 0xccu, 0xc6u, 0xb2u, 0x67u, 0x16u, 0x5au, 0x13u, 0x6cu, 0x3cu, 0xa8u, 0x45u
};
constexpr core::sha256_digest k_v334_retail_dullleather_sha = {
    0x7eu, 0xa7u, 0xd6u, 0xceu, 0x9bu, 0xb7u, 0x14u, 0x8du, 0xb6u, 0x2eu, 0x8cu, 0x27u, 0xeeu, 0x45u, 0xedu, 0xefu, 0xb9u, 0x54u, 0x70u, 0x18u, 0xd0u, 0xb7u, 0x23u, 0xddu, 0x47u, 0x53u, 0xd4u, 0xc3u, 0x7eu, 0x29u, 0xf2u, 0x93u
};
bool v334_exact_wanderer_dullleather(
    const actual_material_owner_observation &observation,
    std::uint64_t semantic_hash) noexcept
{
    return observation.material_slot_valid &&
        observation.material_slot == 9u &&
        observation.flver_sha256 == k_v334_wanderer_model_sha &&
        semantic_hash ==
            operators::material_response::mtd_semantic_hash(
                "P_DullLeather[DSB].mtd");
}
#endif

bool zero_digest(const core::sha256_digest &digest) noexcept
{
    return std::all_of(
        digest.begin(),
        digest.end(),
        [](std::uint8_t value) {
            return value == 0;
        });
}

struct owner_material_cache_entry {
    core::sha256_digest flver_sha256{};
    std::uint32_t material_slot = 0u;
    operators::material_response::material_identity material{};
    bool occupied = false;
    bool resolved = false;
};

constexpr std::size_t k_owner_material_cache_sets = 128u;
constexpr std::size_t k_owner_material_cache_ways = 4u;
constexpr std::size_t k_owner_material_cache_slots =
    k_owner_material_cache_sets *
    k_owner_material_cache_ways;
thread_local std::array<
    owner_material_cache_entry,
    k_owner_material_cache_slots>
    g_owner_material_cache{};
thread_local std::array<
    std::uint8_t,
    k_owner_material_cache_sets>
    g_owner_material_cache_victim{};

std::uint64_t owner_material_cache_entropy(
    const actual_material_owner_observation &observation) noexcept
{
    if (observation.flver_identity_hash != 0u)
        return observation.flver_identity_hash;

    std::uint64_t folded = 0u;
    static_assert(
        sizeof(folded) <=
        core::sha256_digest{}.size());
    std::memcpy(
        &folded,
        observation.flver_sha256.data(),
        sizeof(folded));
    return folded;
}

std::size_t owner_material_cache_set(
    const actual_material_owner_observation &observation) noexcept
{
    // The optional legacy token is bucket entropy only. A missing token falls
    // back to a cheap SHA fold; the full SHA-256 equality check remains the
    // positive cache authority.
    const std::uint64_t h =
        owner_material_cache_entropy(observation) ^
        (static_cast<std::uint64_t>(
             observation.material_slot) *
         0x9E3779B185EBCA87ULL);
    return static_cast<std::size_t>(
        h & (k_owner_material_cache_sets - 1u));
}

owner_material_cache_entry *owner_material_cache_hit(
    const actual_material_owner_observation &observation) noexcept
{
    const auto set =
        owner_material_cache_set(
            observation);
    const auto base =
        set *
        k_owner_material_cache_ways;

    for (std::size_t way = 0u;
         way < k_owner_material_cache_ways;
         ++way) {
        auto &entry =
            g_owner_material_cache[
                base + way];
        if (entry.occupied &&
            entry.material_slot ==
                observation.material_slot &&
            entry.flver_sha256 ==
                observation.flver_sha256)
            return &entry;
    }

    return nullptr;
}

owner_material_cache_entry &owner_material_cache_slot(
    const actual_material_owner_observation &observation) noexcept
{
    const auto set =
        owner_material_cache_set(
            observation);
    const auto base =
        set *
        k_owner_material_cache_ways;

    for (std::size_t way = 0u;
         way < k_owner_material_cache_ways;
         ++way) {
        auto &entry =
            g_owner_material_cache[
                base + way];
        if (!entry.occupied)
            return entry;
    }

    const auto victim =
        static_cast<std::size_t>(
            g_owner_material_cache_victim[set]++ &
            static_cast<std::uint8_t>(
                k_owner_material_cache_ways - 1u));

    return g_owner_material_cache[
        base + victim];
}

const operators::material_response::generated::
    exact_binding_mr_record *
find_exact_binding_extension(
    std::uint64_t semantic_hash,
    const core::sha256_digest *raw_mtd_sha256 = nullptr) noexcept
{
    namespace mr = operators::material_response;
    const mr::generated::exact_binding_mr_record
        *match = nullptr;

    for (const auto &route :
         mr::generated::k_exact_binding_mr_v1) {
        if (mr::mtd_semantic_hash(
                route.mtd_name) !=
            semantic_hash)
            continue;
        if (raw_mtd_sha256 != nullptr &&
            route.raw_mtd_sha256 !=
                *raw_mtd_sha256)
            continue;
        if (match != nullptr)
            return nullptr;
        match = &route;
    }

    return match;
}

} // namespace

bool enrich_exact_owner_mtd_identity(
    actual_material_owner_observation &observation) noexcept
{
    namespace mr = operators::material_response;

    observation.material = {};
    if (zero_digest(observation.flver_sha256) ||
        !observation.material_slot_valid)
        return false;

    if (auto *cached =
            owner_material_cache_hit(
                observation);
        cached != nullptr) {
        observation.material =
            cached->material;
        return cached->resolved;
    }

    auto &cached =
        owner_material_cache_slot(
            observation);

    mr::material_identity resolved{};
    std::uint64_t semantic_hash = 0u;
    if (!mr::generated::dsr_flver_owner_mtd_hash(
            observation.flver_sha256,
            observation.material_slot,
            semantic_hash)) {
        cached = {
            observation.flver_sha256,
            observation.material_slot,
            {},
            true,
            false
        };
        return false;
    }

    core::sha256_digest raw_mtd_sha{};
    // The exact-binding MR authority is narrower than the legacy generic
    // name->raw-MTD registry and carries the certified raw SHA for these
    // eight semantic names. Prefer it explicitly: the generic registry is
    // source-incomplete and currently contains a stale P_Leather[DSB] digest.
    // Outside this exact extension, retain the generic/supplement fail-open
    // policy used by the rest of the owner producer.
    const auto *exact_extension =
        find_exact_binding_extension(
            semantic_hash);
#if defined(DSRRL_V334_WANDERER_9530_DULLLEATHER_MR_OWNER_FIX)
    if (v334_exact_wanderer_dullleather(observation, semantic_hash)) {
        // An exact source owner and exact retail MTD are independently
        // attested. Restore the existing certified P_DullLeather profile;
        // do not copy P_Leather parameters or modify shader/resource paths.
        raw_mtd_sha = k_v334_retail_dullleather_sha;
    } else
#endif
    if (exact_extension != nullptr) {
        raw_mtd_sha =
            exact_extension->raw_mtd_sha256;
    } else if (
        !mr::generated::dsr_mtd_identity_resolve(semantic_hash, raw_mtd_sha) &&
        !mr::generated::dsr_mtd_identity_supplement_resolve(semantic_hash, raw_mtd_sha)) {
        cached = {
            observation.flver_sha256,
            observation.material_slot,
            {},
            true,
            false
        };
        return false;
    }

    resolved.valid = true;
    resolved.semantic_name_hash =
        semantic_hash;
    resolved.raw_mtd_sha256 =
        raw_mtd_sha;

    const mr::generated::route_seed *route_match =
        nullptr;
    for (const auto &route :
         mr::generated::k_material_routes_v1) {
        if (mr::mtd_semantic_hash(
                route.mtd_name) !=
                semantic_hash ||
            !operators::legacy_plan::hashing::
                matches_hex(
                    resolved.raw_mtd_sha256,
                    route.sha256))
            continue;

        if (route_match != nullptr &&
            (route_match->route_index !=
                 route.route_index ||
             mr::mtd_semantic_hash(
                 route_match->material_family) !=
             mr::mtd_semantic_hash(
                 route.material_family))) {
            resolved.route_index = 0u;
            resolved.material_family_hash = 0u;
            observation.material = resolved;
            cached = {
                observation.flver_sha256,
                observation.material_slot,
                resolved,
                true,
                true
            };
            return true;
        }

        route_match =
            &route;
    }

    if (route_match != nullptr) {
        resolved.route_index =
            route_match->route_index;
        resolved.material_family_hash =
            mr::mtd_semantic_hash(
                route_match->material_family);
    } else {
        const auto *extension =
            find_exact_binding_extension(
                semantic_hash,
                &resolved.raw_mtd_sha256);
        if (extension != nullptr) {
            resolved.route_index =
                extension->route_tag;
            resolved.material_family_hash =
                mr::mtd_semantic_hash(
                    extension->material_family);
        }
    }

    observation.material =
        resolved;
    cached = {
        observation.flver_sha256,
        observation.material_slot,
        resolved,
        true,
        true
    };
    return true;
}

operators::material_response::material_identity make_actual_material_identity(const actual_material_owner_observation &observation) noexcept
{
    auto result = observation.material;
    if (!result.valid || zero_digest(observation.flver_sha256) || !observation.material_slot_valid || result.semantic_name_hash == 0u) {
        result.owner_tuple_exact = false;
        result.flver_sha256 = {};
        result.flver_identity_hash = 0u;
        result.material_slot = 0u;
        result.material_slot_valid = false;
        return result;
    }
    result.flver_sha256 = observation.flver_sha256;
    result.flver_identity_hash = observation.flver_identity_hash;
    result.material_slot = observation.material_slot;
    result.material_slot_valid = true;
    result.owner_tuple_exact = true;
    return result;
}

} // namespace dsrrl::runtime
