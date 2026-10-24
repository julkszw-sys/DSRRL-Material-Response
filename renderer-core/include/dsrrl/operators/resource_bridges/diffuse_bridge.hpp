#pragma once

#include "dsrrl/core/draw_transaction_policy.hpp"

#include <cstdint>

namespace dsrrl::operators::resource_bridges {

static_assert(
    core::requires_draw_transaction(core::operator_id::diffuse),
    "Diffuse bridge must use the shared transaction layer.");

enum class diffuse_action : std::uint8_t {
    preserve_existing_route = 0,
    bind_ptde_t0,
    bind_ptde_t0_and_full_material_response = bind_ptde_t0
};

enum class diffuse_reason : std::uint8_t {
    active = 0,
    unsupported_receiver,
    material_not_verified,
    actual_t0_not_verified,
    ptde_companion_not_verified,
    ptde_srv_not_ready,
    c100_donor_not_verified,
    diffuse_linear_receiver_not_ready,
    shared_material_texture_identity_missing
};

struct diffuse_bridge_context {
    std::uint32_t receiver_id = 0;

    bool actual_material_verified = false;
    bool actual_bound_t0_verified = false;
    bool exact_ptde_diffuse_companion_verified = false;
    bool ptde_srv_ready = false;

    // Material Response c100 and the diffuse-linear receiver are separate
    // operator signals, not prerequisites for the t0 resource replacement.
    bool ptde_c100_donor_verified = false;
    bool diffuse_linear_receiver_ready = false;

    // Legacy aggregate-route metadata. Resource-local t0 selects by the
    // current bound stock texture and exact corresponding sidecar.
    bool shared_material_route = false;
    bool exact_texture_identity_conjunction = false;
};

struct diffuse_bridge_decision {
    diffuse_action action = diffuse_action::preserve_existing_route;
    diffuse_reason reason = diffuse_reason::unsupported_receiver;
    std::uint32_t receiver_id = 0;
    std::uint8_t srv_slot = 0;
    bool use_ptde_c100 = false;
    bool require_diffuse_linear_receiver = false;
};

// Equipment Diffuse augmentation for stable no-PointLight DifSpcBmp HemEnv
// receivers 24..35.
//
// Resource-local target: live stock t0 identity -> matching PTDE sidecar -> t0.
// Material Response c100 remains a separate operator.
//
// When any coordinate is missing, preserve the already-selected route. This is
// important for SPEC_ONLY draws: absence of a diffuse companion must not disable
// a separately valid SpecRGB island.
diffuse_bridge_decision evaluate_diffuse_route(
    const diffuse_bridge_context &context) noexcept;

} // namespace dsrrl::operators::resource_bridges
