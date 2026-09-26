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
    replacement_shader_not_materialized,
    replacement_shader_identity_mismatch,
    replacement_shader_lifecycle_not_verified
};

struct subsurface_runtime_context {
    subsurface_route_context route{};
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
