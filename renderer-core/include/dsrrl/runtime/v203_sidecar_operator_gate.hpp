#pragma once
// V203 sidecar-wide EXPERIMENTAL nonmetal operator authorization.
// Resource whitelist: all 1295 uploaded PTDE Specular logical names.
// Material/consumer authorization: independently full FLVER SHA256 +
// material slot + name+raw MTD SHA256 EnvSpec PRESENT+valid slot,
// DifSpcBmp family, per-material producer, RX33..35.
// Crucially: no actual_material_exact flag requirement, as in the
// independently proved original P_Metal selection. No hash prefix, name-only,
// orphan sidecar, or fallback to global LightBank may authorize the operator.
// Original P_Metal route345 remains independent and untouched.
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/runtime/generated_sidecar_spec_whitelist_20261010.hpp"
#include <cmath>
#include <cstdint>

namespace dsrrl::runtime::sidecar_20261010 {
namespace mr = operators::material_response;

inline mr::mtd_semantic_query make_query(
    const mr::material_identity &material,
    std::uint32_t receiver_id=0u) noexcept {
    mr::mtd_semantic_query q{};
    q.material=material;
    q.receiver_id=receiver_id;
    q.ownership.flver_sha256=material.flver_sha256;
    q.ownership.flver_identity_hash=material.flver_identity_hash;
    q.ownership.material_slot=material.material_slot;
    q.ownership.material_slot_valid=material.material_slot_valid;
    q.ownership.exact=material.owner_tuple_exact;
    return q;
}

inline bool exact_nonmetal_material(
    const mr::material_identity &m) noexcept {
    if (!m.valid || !m.owner_tuple_exact ||
        !m.material_slot_valid ||
        m.route_index==345u ||
        m.semantic_name_hash==mr::mtd_semantic_hash("P_Metal[DSB].mtd") ||
        m.material_family_hash!=mr::mtd_semantic_hash("DifSpcBmp"))
        return false;
    // Complete 256-bit FLVER ownership proof, not a name or SHA4-prefix match.
    const auto query=make_query(m);
    if (!mr::has_exact_flver_material_ownership(query))
        return false;
    // The exact raw SHA here must match the pinned *source* EnvSpec semantic
    // router. Producer-assigned hash variants absent from the router fail open.
    const auto env=mr::classify_mtd_envspec_semantics(query);
    return env.exact_identity_match &&
        env.presence==mr::ptde_envspec_presence::present &&
        env.router_state==mr::mtd_envspec_router_state::present &&
        env.envspc_slot_valid && env.envspc_slot<4u;
}

inline bool exact_nonmetal_draw(
    const mr::material_identity &m,
    const mr::decision &d) noexcept {
    return exact_nonmetal_material(m) &&
        d.active && d.receiver_id>=33u && d.receiver_id<=35u &&
        std::isfinite(d.c101) && d.c101>=0.0f;
}

inline bool exact_nonmetal_stock_t1(
    const mr::mtd_semantic_query &q,
    std::uint64_t live_t1_name_hash,
    bool ptde_companion_ready) noexcept {
    return exact_nonmetal_material(q.material) &&
        live_t1_name_hash!=0ull &&
        spec_allowed(live_t1_name_hash) &&
        ptde_companion_ready;
}

} // namespace dsrrl::runtime::sidecar_20261010
