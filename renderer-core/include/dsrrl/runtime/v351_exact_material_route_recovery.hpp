#pragma once
// V351: exact route recovery at the MTD/FLVER selector transport.
// Strictly transports an EXISTING certified MR route; does not invent a
// material profile, receiver, texture equivalence, or resource authorization.
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/material_response/generated_routes_v1.hpp"

namespace dsrrl::runtime::v351 {
inline operators::material_response::material_identity recover_exact_route(
    const operators::material_response::material_identity &identity) noexcept
{
    namespace mr = operators::material_response;
    if (!identity.valid || !identity.actual_material_exact ||
        !identity.owner_tuple_exact || !identity.material_slot_valid ||
        identity.route_index != 0u || identity.semantic_name_hash == 0u ||
        identity.material_family_hash == 0u)
        return identity;

    // Route zero is real for P_Leather[DSB]_Alp. Never overwrite it if
    // the complete raw-MTD digest already certifies route zero.
    if (mr::exact_runtime_material_response_identity(identity))
        return identity;

    mr::material_identity selected{};
    unsigned matches = 0u;
    for (const auto &profile : mr::generated::k_material_routes_v1) {
        if (profile.route_index == 0u ||
            mr::mtd_semantic_hash(profile.mtd_name) != identity.semantic_name_hash ||
            mr::mtd_semantic_hash(profile.material_family) != identity.material_family_hash)
            continue;

        auto candidate = identity;
        candidate.route_index = profile.route_index;
        // This requires complete original raw-MTD SHA256, exact name
        // and family against generated certified route records.
        if (!mr::exact_runtime_material_response_identity(candidate))
            continue;

        ++matches;
        selected = candidate;
        if (matches > 1u)
            return identity; // non-unique; fail closed as before
    }
    return matches == 1u ? selected : identity;
}
} // namespace dsrrl::runtime::v351
