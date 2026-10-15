#pragma once
// V299 -- close the producer/consumer AUTHORIZATION CUT for the three
// original PTDE slot-zero SPX material profiles (V297).
// EXPERIMENTAL and opt-in: original shader/renderer pixel behavior OPEN.
// Source MTD bytes, exact FLVER+slot, PTDE router slot and receiver
// conditions are checked at the same cut; no generic material whitelist.
#include "dsrrl/runtime/v297_authored_slot0_material.hpp"
#include <cmath>
#include <cstdint>

namespace dsrrl::runtime::v299 {
namespace mr = dsrrl::operators::material_response;

inline bool authored_slot0_source(
    const mr::material_identity &material) noexcept
{
    // Source selection has no bound shader yet. All three rows have the
    // identical PTDE authorizing material/owner/slot0 provenance, but the
    // receiver is checked anew at draw time. No header-name-only gate.
    mr::decision inactive{};
    mr::decision candidate{};
    return v297::promote_exact_slot0_source(
        material, inactive, 33u, candidate);
}

inline bool authored_slot0_draw(
    const mr::material_identity &material,
    const mr::decision &decision) noexcept
{
    if (!decision.active ||
        decision.receiver_id < 33u ||
        decision.receiver_id > 35u)
        return false;

    mr::decision inactive{};
    mr::decision canonical{};
    if (!v297::promote_exact_slot0_source(
            material, inactive, decision.receiver_id,
            canonical))
        return false;

    // Actual draw decision must be the EXACT producer-approved one.
    // Do not reuse generic MR c100/c101 or P_Metal route345.
    if (decision.route_index != canonical.route_index ||
        decision.certified_operations !=
            canonical.certified_operations ||
        decision.receiver_id != canonical.receiver_id ||
        !decision.ptde_specular_power_verified ||
        !std::isfinite(decision.c101) ||
        !std::isfinite(decision.ptde_specular_power) ||
        decision.c101 != canonical.c101 ||
        decision.ptde_specular_power !=
            canonical.ptde_specular_power ||
        decision.envspec != canonical.envspec)
        return false;

    for (unsigned i=0u;i<3u;++i)
        if (decision.c100[i] != canonical.c100[i])
            return false;

    return true;
}
} // namespace dsrrl::runtime::v299
