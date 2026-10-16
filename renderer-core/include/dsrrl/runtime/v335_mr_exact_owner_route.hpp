#pragma once

// V335: exact, draw-local material-response identity repair.
// No static MTD-name -> synthetic SHA mapping, no resource bridge changes.
// A route is recovered only when original live full raw MTD SHA is ALREADY
// present and the exact FLVER+material slot+semantic owner is attested.
// Remaining MR decisions are made by the stock certified material island.

#include "dsrrl/operators/material_response/generated_routes_v1.hpp"
#include "dsrrl/operators/material_response/generated_dsr_flver_owner_tuples_v1.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <cstdint>

namespace dsrrl::runtime {

inline bool v335_recover_exact_mr_route(
    operators::material_response::material_identity &material,
    std::uint32_t receiver_id) noexcept
{
    namespace mr = operators::material_response;
    namespace gen = mr::generated;

    // Only missing normal MR routing is repaired. Certified custom
    // extensions, direct PointLight, or a previously selected route remain
    // wholly unchanged. The sole relevant receiver cohort is HemEnv RX33-35.
    if (!material.valid ||
        !material.owner_tuple_exact ||
        !material.material_slot_valid ||
        material.semantic_name_hash == 0u ||
        material.route_index != 0u ||
        (receiver_id != 33u &&
         receiver_id != 34u &&
         receiver_id != 35u))
        return false;

    if (!gen::dsr_flver_owner_tuple_authenticated(
            material.flver_sha256,
            material.material_slot,
            material.semantic_name_hash))
        return false;

    const gen::route_seed *match = nullptr;
    std::uint64_t match_family_hash = 0u;
    for (const auto &route : gen::k_material_routes_v1) {
        if (route.route_index == 0u ||
            mr::mtd_semantic_hash(route.mtd_name) !=
                material.semantic_name_hash ||
            !operators::legacy_plan::hashing::matches_hex(
                material.raw_mtd_sha256, route.sha256) ||
            (receiver_id != route.receiver0 &&
             receiver_id != route.receiver1 &&
             receiver_id != route.receiver2))
            continue;

        const auto family_hash =
            mr::mtd_semantic_hash(route.material_family);
        if (material.material_family_hash != 0u &&
            material.material_family_hash != family_hash)
            continue;

        // An ambiguous source record may never resolve by guessing.
        if (match != nullptr &&
            (match->route_index != route.route_index ||
             match_family_hash != family_hash))
            return false;

        match = &route;
        match_family_hash = family_hash;
    }

    if (match == nullptr)
        return false;

    material.route_index = match->route_index;
    material.material_family_hash = match_family_hash;
    return true;
}

} // namespace dsrrl::runtime
