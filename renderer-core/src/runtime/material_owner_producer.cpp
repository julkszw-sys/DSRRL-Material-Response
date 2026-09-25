#include "dsrrl/runtime/material_owner_producer.hpp"
#include <algorithm>

namespace dsrrl::runtime {
namespace {
bool zero_digest(const core::sha256_digest &digest) noexcept
{
    return std::all_of(digest.begin(), digest.end(),
        [](std::uint8_t value) { return value == 0; });
}
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
