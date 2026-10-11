#pragma once
// Opt-in immutable MTD index; the authoritative 325-row source is unchanged.
// Only call after exact authenticated nonmetal full-FLVER/slot ownership.
#include "dsrrl/runtime/v203_equipment_constexpr_lookup.hpp"
#include "dsrrl/operators/material_response/generated_envspec_router_v1.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime::v203_equipment::mtd_fast {
namespace mr = operators::material_response;
namespace gen = mr::generated;
inline constexpr auto k_dull =
    constexpr_lookup::fnv1a("P_DullLeather[DSB].mtd");
inline constexpr auto k_leather =
    constexpr_lookup::fnv1a("P_Leather[DSB].mtd");

constexpr std::size_t count() noexcept {
    std::size_t n = 0;
    for (const auto &r : gen::k_envspec_router_v1)
        if (r.semantic_name_hash == k_dull ||
            r.semantic_name_hash == k_leather) ++n;
    return n;
}
static_assert(count() == 3u, "Exact leather MTD corpus changed");

constexpr std::array<std::size_t,3> make_indices() noexcept {
    std::array<std::size_t,3> out{};
    std::size_t n = 0;
    for (std::size_t i = 0; i < gen::k_envspec_router_record_count; ++i) {
        const auto &r = gen::k_envspec_router_v1[i];
        if (r.semantic_name_hash == k_dull ||
            r.semantic_name_hash == k_leather) {
            if (n < out.size()) out[n] = i;
            ++n;
        }
    }
    return out;
}
inline constexpr auto k_indices = make_indices();

constexpr bool same_digest(
    const std::array<std::uint8_t,32> &a,
    const std::array<std::uint8_t,32> &b) noexcept {
    for (std::size_t j = 0; j < a.size(); ++j)
        if (a[j] != b[j]) return false;
    return true;
}
constexpr bool validate() noexcept {
    std::size_t dull = 0, leather = 0;
    for (const auto i : k_indices) {
        const auto &r = gen::k_envspec_router_v1[i];
        if (r.state != gen::envspec_router_state::present ||
            r.explicit_none_safe ||
            r.semantic_name_hash != constexpr_lookup::fnv1a(r.mtd_name))
            return false;
        if (r.semantic_name_hash == k_dull && r.envspc_slot == 0) ++dull;
        else if (r.semantic_name_hash == k_leather && r.envspc_slot == 1) ++leather;
        else return false;
        std::size_t duplicates = 0;
        for (const auto &other : gen::k_envspec_router_v1)
            if (r.semantic_name_hash == other.semantic_name_hash &&
                same_digest(r.raw_mtd_sha256,other.raw_mtd_sha256)) ++duplicates;
        if (duplicates != 1) return false;
    }
    return dull == 2 && leather == 1;
}
static_assert(validate(), "Raw MTD source identity/slot contract changed");

inline mr::mtd_envspec_semantics classify(
    const mr::mtd_semantic_query &q) noexcept {
    mr::mtd_envspec_semantics out{};
    if (!q.material.valid || !q.material.semantic_name_hash) return out;
    for (const auto i : k_indices) {
        const auto &r = gen::k_envspec_router_v1[i];
        if (q.material.semantic_name_hash != r.semantic_name_hash ||
            q.material.raw_mtd_sha256 != r.raw_mtd_sha256) continue;
        out.exact_identity_match = true;
        out.presence = mr::ptde_envspec_presence::present;
        out.router_state = mr::mtd_envspec_router_state::present;
        out.envspc_slot_valid = true;
        out.envspc_slot = r.envspc_slot;
        out.suppress_dsr_only_safe = r.explicit_none_safe;
        return out;
    }
    return out;
}
} // namespace dsrrl::runtime::v203_equipment::mtd_fast
