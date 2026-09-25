#include "dsrrl/runtime/material_owner_producer.hpp"
#include "dsrrl/operators/material_response/generated_dsr_flver_owner_tuples_v1.hpp"
#include "dsrrl/operators/material_response/generated_envspec_router_v1.hpp"
#include "dsrrl/operators/material_response/generated_mtd_spx_negative_v1.hpp"
#include "dsrrl/operators/material_response/generated_routes_v1.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <algorithm>

namespace dsrrl::runtime {
namespace {
bool zero_digest(const core::sha256_digest &digest) noexcept
{
    return std::all_of(digest.begin(), digest.end(), [](std::uint8_t value) { return value == 0; });
}
int hex_nibble(char c) noexcept
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}
bool decode_sha256(const char *hex, core::sha256_digest &out) noexcept
{
    if (!hex) return false;
    for (std::size_t i = 0; i < out.size(); ++i) {
        const int hi = hex_nibble(hex[i * 2]);
        const int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return hex[64] == '\0';
}

// Exact raw-MTD identity evidence is a union of canonical exact surfaces, not
// EnvSpec alone. Any cross-surface disagreement for one semantic hash is
// ambiguity and fails open. This remains an incremental bridge until the full
// DSR MTD corpus is materialized as a dedicated generated identity registry.
bool resolve_exact_raw_mtd_sha(std::uint64_t semantic_hash, core::sha256_digest &raw_sha) noexcept
{
    namespace mr = operators::material_response;
    bool found = false;
    auto accept = [&](const core::sha256_digest &candidate) noexcept {
        if (!found) { raw_sha = candidate; found = true; return true; }
        return raw_sha == candidate;
    };
    for (const auto &candidate : mr::generated::k_envspec_router_v1)
        if (candidate.semantic_name_hash == semantic_hash && !accept(candidate.raw_mtd_sha256)) return false;
    for (const auto &candidate : mr::generated::k_mtd_spx_negative_v1)
        if (candidate.semantic_name_hash == semantic_hash && !accept(candidate.raw_dsr_mtd_sha256)) return false;
    for (const auto &route : mr::generated::k_material_routes_v1) {
        if (mr::mtd_semantic_hash(route.mtd_name) != semantic_hash) continue;
        core::sha256_digest candidate{};
        if (!decode_sha256(route.sha256, candidate) || !accept(candidate)) return false;
    }
    return found;
}
}

bool enrich_exact_owner_mtd_identity(actual_material_owner_observation &observation) noexcept
{
    namespace mr = operators::material_response;
    observation.material = {};
    if (zero_digest(observation.flver_sha256) || !observation.material_slot_valid) return false;
    std::uint64_t semantic_hash = 0u;
    if (!mr::generated::dsr_flver_owner_mtd_hash(observation.flver_sha256, observation.material_slot, semantic_hash)) return false;
    core::sha256_digest raw_mtd_sha{};
    if (!resolve_exact_raw_mtd_sha(semantic_hash, raw_mtd_sha)) return false;
    observation.material.valid = true;
    observation.material.semantic_name_hash = semantic_hash;
    observation.material.raw_mtd_sha256 = raw_mtd_sha;

    const mr::generated::route_seed *route_match = nullptr;
    for (const auto &route : mr::generated::k_material_routes_v1) {
        if (mr::mtd_semantic_hash(route.mtd_name) != semantic_hash ||
            !operators::legacy_plan::hashing::matches_hex(observation.material.raw_mtd_sha256, route.sha256)) continue;
        if (route_match != nullptr && (route_match->route_index != route.route_index ||
            mr::mtd_semantic_hash(route_match->material_family) != mr::mtd_semantic_hash(route.material_family))) {
            observation.material.route_index = 0u;
            observation.material.material_family_hash = 0u;
            return true;
        }
        route_match = &route;
    }
    if (route_match != nullptr) {
        observation.material.route_index = route_match->route_index;
        observation.material.material_family_hash = mr::mtd_semantic_hash(route_match->material_family);
    }
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
