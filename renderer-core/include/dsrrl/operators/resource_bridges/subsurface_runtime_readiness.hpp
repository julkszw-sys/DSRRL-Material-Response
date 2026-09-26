#pragma once

#include "dsrrl/core/island_policy.hpp"
#include "dsrrl/operators/resource_bridges/replacement_shader_lifecycle.hpp"
#include "dsrrl/operators/resource_bridges/subsurface_route.hpp"

#include <cstdint>

namespace dsrrl::operators::resource_bridges {

enum class subsurface_runtime_reason : std::uint8_t {
    ready = 0,
    core_gate_not_active,
    route_not_active,
    create_time_owner_exclusivity_not_verified,
    replacement_shader_not_materialized,
    replacement_shader_identity_mismatch,
    replacement_shader_lifecycle_not_verified
};

struct subsurface_runtime_context {
    subsurface_route_context route{};

    // A create-time PS substitution acts on the shader object before any
    // draw-local MTD identity exists. The route contract is material-specific,
    // so the carrier is legal only when source-complete evidence proves that
    // this exact Subsurf source shader identity is exclusive to the authorized
    // Ps_Body route (or an equivalent create-time owner token is available).
    // ABI compatibility alone does not prove this ownership property.
    bool create_time_owner_exclusivity_verified = false;

    bool replacement_shader_materialized = false;
    bool replacement_shader_identity_verified = false;

    // Subsurface uses the narrow create-time PS substitution carrier. The
    // lifecycle proof is structural and independent from runtime activation:
    // persistent replacement bytes -> init SHA attestation -> pipeline-owned
    // record -> bind reachability -> destroy_pipeline retirement -> device
    // retirement. Current A1 bridge implements this exact ownership pattern,
    // but Subsurface is not an A1 owner yet, so callers must provide a closed
    // carrier only after the Subsurface replacement is wired through it.
    replacement_shader_lifecycle_carrier replacement_lifecycle{};
};

struct subsurface_runtime_plan {
    bool ready = false;
    subsurface_runtime_reason reason =
        subsurface_runtime_reason::core_gate_not_active;
    subsurface_route_decision route{};
};

inline subsurface_runtime_plan evaluate_subsurface_runtime_readiness(
    const core::feature_registry &features,
    const core::activation_context &activation,
    const subsurface_runtime_context &context) noexcept
{
    subsurface_runtime_plan out;
    const auto core_gate = core::evaluate_operator_activation(
        features, core::operator_id::subsurface, activation);

    if (core_gate.state != core::island_state::active) {
        out.reason = subsurface_runtime_reason::core_gate_not_active;
        return out;
    }

    out.route = evaluate_subsurface_route(context.route);
    if (out.route.action !=
        subsurface_route_action::route_to_ptde_plain_difspcbmp_surface) {
        out.reason = subsurface_runtime_reason::route_not_active;
        return out;
    }

    if (!context.create_time_owner_exclusivity_verified) {
        out.reason =
            subsurface_runtime_reason::create_time_owner_exclusivity_not_verified;
        return out;
    }

    if (!context.replacement_shader_materialized) {
        out.reason = subsurface_runtime_reason::replacement_shader_not_materialized;
        return out;
    }

    if (!context.replacement_shader_identity_verified) {
        out.reason = subsurface_runtime_reason::replacement_shader_identity_mismatch;
        return out;
    }

    if (validate_replacement_shader_lifecycle(context.replacement_lifecycle) !=
        replacement_shader_lifecycle_result::closed) {
        out.reason = subsurface_runtime_reason::replacement_shader_lifecycle_not_verified;
        return out;
    }

    out.ready = true;
    out.reason = subsurface_runtime_reason::ready;
    return out;
}

} // namespace dsrrl::operators::resource_bridges
