#pragma once
// v2.0.3 diagnostic only: reuse PR205/PR207 EnvSpec semantic gate,
// but narrow provenance to the independently attested 36 FLVER/slot/MTD tuples.
// This is NOT a material-family-wide authorization, a PTDE pixel verdict,
// or permission to feed a DSR PBL/F0 specular tail with PTDE RGB.
#include "dsrrl/runtime/v203_equipment_flver_slot_spec_manifest.hpp"
#include <cmath>

namespace dsrrl::runtime::v203_equipment {

inline operators::material_response::mtd_semantic_query
make_envspec36_query(
    const operators::material_response::material_identity &material) noexcept
{
    namespace mr = operators::material_response;
    mr::mtd_semantic_query query{};
    query.material = material;
    query.ownership.flver_sha256 = material.flver_sha256;
    query.ownership.flver_identity_hash = material.flver_identity_hash;
    query.ownership.material_slot = material.material_slot;
    query.ownership.material_slot_valid = material.material_slot_valid;
    query.ownership.exact = material.owner_tuple_exact;
    return query;
}

// Static producer eligibility, not a resource match and not a replacement.
inline bool exact_envspec36_material(
    const operators::material_response::material_identity &material) noexcept
{
    namespace mr = operators::material_response;
    // The original exact P_Metal island remains the only owner of route 345
    // and P_Metal[DSB]. Do not broaden its authority with a second gate.
    if (!material.valid || !material.owner_tuple_exact ||
        !material.material_slot_valid ||
        material.route_index == 345u ||
        material.semantic_name_hash ==
            mr::mtd_semantic_hash("P_Metal[DSB].mtd") ||
        material.semantic_name_hash == 0u)
        return false;

    const auto query = make_envspec36_query(material);
    const auto env = mr::classify_mtd_envspec_semantics(query);
    return env.exact_identity_match &&
           env.presence == mr::ptde_envspec_presence::present &&
           env.router_state == mr::mtd_envspec_router_state::present &&
           env.envspc_slot_valid && env.envspc_slot < 4u;
}

// Draw-side witness: the shader/material domain must be certified,
// before the independently checked stock t1 and exact PTDE t10 companion.
inline bool exact_envspec36_draw(
    const operators::material_response::material_identity &material,
    const operators::material_response::decision &decision) noexcept
{
    return exact_envspec36_material(material) &&
           decision.active &&
           decision.receiver_id >= 33u &&
           decision.receiver_id <= 35u &&
           std::isfinite(decision.c101) &&
           decision.c101 >= 0.0f;
}

} // namespace dsrrl::runtime::v203_equipment
