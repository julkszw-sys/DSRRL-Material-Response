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

namespace dsrrl::runtime {
namespace {

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

constexpr std::size_t k_owner_material_cache_slots = 32u;
thread_local std::array<
    owner_material_cache_entry,
    k_owner_material_cache_slots>
    g_owner_material_cache{};

std::size_t owner_material_cache_index(
    const core::sha256_digest &digest,
    std::uint32_t material_slot) noexcept
{
    std::uint64_t h =
        0xcbf29ce484222325ULL;
    for (const auto byte : digest) {
        h ^= byte;
        h *= 0x100000001b3ULL;
    }

    h ^= material_slot;
    h *= 0x100000001b3ULL;
    return static_cast<std::size_t>(
        h % k_owner_material_cache_slots);
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

    auto &cached =
        g_owner_material_cache[
            owner_material_cache_index(
                observation.flver_sha256,
                observation.material_slot)];

    if (cached.occupied &&
        cached.material_slot ==
            observation.material_slot &&
        cached.flver_sha256 ==
            observation.flver_sha256) {
        observation.material =
            cached.material;
        return cached.resolved;
    }

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
    // Exact raw-MTD identity comes from the dedicated generic registry or
    // the evidence-certified special-route supplement. The supplement also
    // carries the exact HOMOLOGOUS_NOSPC identities required by direct
    // PointLight, but does not make generic coverage source-complete.
    // Unknown/ambiguous semantic hashes still fail open. EnvSpec/SPX
    // consumer membership is never used as an identity fallback.
    if (!mr::generated::dsr_mtd_identity_resolve(
            semantic_hash,
            raw_mtd_sha) &&
        !mr::generated::dsr_mtd_identity_supplement_resolve(
            semantic_hash,
            raw_mtd_sha)) {
        const auto *extension =
            find_exact_binding_extension(
                semantic_hash);
        if (extension == nullptr) {
            cached = {
                observation.flver_sha256,
                observation.material_slot,
                {},
                true,
                false
            };
            return false;
        }
        raw_mtd_sha =
            extension->raw_mtd_sha256;
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
