#pragma once
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include <cstdint>

namespace dsrrl::runtime {

struct actual_material_owner_observation {
    core::sha256_digest flver_sha256{};
    std::uint64_t flver_identity_hash = 0;
    std::uint32_t material_slot = 0;
    bool material_slot_valid = false;
    operators::material_response::material_identity material{};
};

// Resolve the exact DSR material identity carried by a source-complete
// (raw FLVER SHA-256, material slot) owner tuple. This is a static host-identity
// join only: no receiver/operator activation follows from it. Missing or
// ambiguous MTD identity fails open and clears observation.material.
bool enrich_exact_owner_mtd_identity(
    actual_material_owner_observation &observation) noexcept;

// Narrow handoff between the engine-side FLVER/material selector observer and
// Material Response. It never infers ownership from MTD identity.
operators::material_response::material_identity
make_actual_material_identity(
    const actual_material_owner_observation &observation) noexcept;

} // namespace dsrrl::runtime
