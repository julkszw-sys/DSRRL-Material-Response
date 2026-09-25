#include "dsrrl/runtime/material_owner_producer.hpp"
#include "dsrrl/operators/material_response/generated_dsr_flver_owner_tuples_v1.hpp"
#include "dsrrl/operators/material_response/generated_envspec_router_v1.hpp"
#include "dsrrl/operators/material_response/generated_routes_v1.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <algorithm>

namespace dsrrl::runtime {
namespace {
bool zero_digest(const core::sha256_digest &digest) noexcept
{
    return std::all_of(digest.begin(), digest.end(),
        [](std::uint8_t value) { return value == 0; });
}
}

bool enrich_exact_owner_mtd_identity(
    actual_material_owner_observation &observation) noexcept
{
    namespace mr = operators::material_response;
    observation.material = {};

    if (zero_digest(observation.flver_sha256) ||
        !observation.material_slot_valid)
        return false;

    std::uint64_t semantic_hash = 0u;
    if (!mr::generated::dsr_flver_owner_mtd_hash(
            observation.flver_sha256,
            observation.material_slot,
            semantic_hash))
        return false;

    const mr::generated::envspec_router_record *match = nullptr;
    for (const auto &candidate : mr::generated::k_envspec_router_v1) {
        if (candidate.semantic_name_hash != semantic_hash)
            continue;

        if (match == nullptr) {
            match = &candidate;
            continue;
        }

        // A name hash that resolves to more than one raw DSR MTD payload is
        // not an exact host identity and must remain fail-open.
        if (match->raw_mtd_sha256 != candidate.raw_mtd_sha256)
            return false;
    }

    if (match == nullptr)
        return false;

    observation.material.valid = true;
    observation.material.semantic_name_hash = semantic_hash;
    observation.material.raw_mtd_sha256 = match->raw_mtd_sha256;

    const mr::generated::route_seed *route_match = nullptr;
    for (const auto &route : mr::generated::k_material_routes_v1) {
        if (mr::mtd_semantic_hash(route.mtd_name) != semantic_hash ||
            !operators::legacy_plan::hashing::matches_hex(
                observation.material.raw_mtd_sha256,
                route.sha256))
            continue;

        if (route_match != nullptr &&
            (route_match->route_index != route.route_index ||
             mr::mtd_semantic_hash(route_match->material_family) !=
                 mr::mtd_semantic_hash(route.material_family))) {
            observation.material.route_index = 0u;
            observation.material.material_family_hash = 0u;
            return true;
        }

        route_match = &route;
    }

    if (route_match != nullptr) {
        observation.material.route_index = route_match->route_index;
        observation.material.material_family_hash =
            mr::mtd_semantic_hash(route_match->material_family);
    }

    return true;
}

operators::material_response::material_identity
make_actual_material_identity(
    const actual_material_owner_observation &observation) noexcept
{
    auto result = observation.material;

    // The producer requires the complete FLVER digest, material slot and
    // semantic material identity. The legacy 64-bit FLVER token is auxiliary
    // compatibility/telemetry only and is never an authorization requirement.
    if (!result.valid ||
        zero_digest(observation.flver_sha256) ||
        !observation.material_slot_valid ||
        result.semantic_name_hash == 0u) {
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

    // This flag means the producer observed all identity components. Corpus
    // membership is still checked independently by the Material Response gate.
    result.owner_tuple_exact = true;
    return result;
}

} // namespace dsrrl::runtime
