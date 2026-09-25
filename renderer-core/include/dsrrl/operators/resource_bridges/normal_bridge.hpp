#pragma once

#include "dsrrl/core/draw_transaction_policy.hpp"

#include <cstdint>

namespace dsrrl::operators::resource_bridges {

static_assert(
    core::requires_draw_transaction(core::operator_id::normal),
    "Normal bridge must use the shared transaction layer.");

enum class normal_route_authority : std::uint8_t {
    none = 0,
    homologous_material_route,
    safe_exact_resource_tuple
};

enum class normal_action : std::uint8_t {
    preserve_host = 0,
    bind_ptde_t2
};

enum class normal_reason : std::uint8_t {
    active = 0,
    unsupported_receiver,
    route_not_authorized,
    actual_t2_not_verified,
    ptde_sidecar_not_verified,
    ptde_srv_not_ready,
    ambiguous_target,
    nonhomologous_route,
    sampler_contract_not_preserved
};

struct normal_bridge_context {
    std::uint32_t receiver_id = 0;
    normal_route_authority authority = normal_route_authority::none;

    // Semantic route facts. The material-route path requires a homologous PTDE
    // Bmp operator. The tuple path represents a pre-certified exact t0+t1+t2
    // whitelist entry and therefore does not infer material from filenames.
    bool homologous_bmp_material_route = false;
    bool safe_exact_t0_t1_t2_tuple = false;
    bool target_unambiguous = false;

    // Resource identity must originate from the actually bound DSR t2/g_Bumpmap.
    bool actual_bound_t2_verified = false;
    bool exact_ptde_normal_sidecar_verified = false;
    bool ptde_srv_ready = false;

    // This bridge replaces only t2. The existing host sampler contract at s2 is
    // preserved rather than guessed/re-authored.
    bool preserve_stock_s2 = true;
};

struct normal_bridge_decision {
    normal_action action = normal_action::preserve_host;
    normal_reason reason = normal_reason::route_not_authorized;
    std::uint32_t receiver_id = 0;
    std::uint8_t srv_slot = 2;
    std::uint8_t sampler_slot = 2;
    bool preserve_stock_sampler = true;
};

// Conservative equipment-normal bridge for stable no-PointLight DifSpcBmp
// HemEnv receivers 24..35.
//
// Legal authorization:
//   A) exact homologous Bmp material route, OR
//   B) certified safe exact application-bound t0+t1+t2 tuple.
//
// Both paths additionally require the actual bound t2 identity, an exact PTDE
// normal sidecar resolving to one unambiguous logical target, and a ready PTDE
// SRV. Filename-derived _s -> _n inference is intentionally absent.
normal_bridge_decision evaluate_normal_route(
    const normal_bridge_context &context) noexcept;

} // namespace dsrrl::operators::resource_bridges
