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
    material_aware_material_identity_not_verified,
    material_aware_receiver_identity_not_verified,
    material_aware_selector_transport_not_verified,
    material_aware_replacement_identity_not_verified,
    material_aware_draw_transaction_not_verified,
    replacement_shader_not_materialized,
    replacement_shader_identity_mismatch,
    replacement_shader_lifecycle_not_verified
};

enum class subsurface_carrier_kind : std::uint8_t {
    create_time_global = 0,
    material_aware_draw_local
};

struct subsurface_material_aware_carrier {
    // This is intentionally stricter than generic selector liveness. The token
    // must authenticate the exact Ps_Body[DSBT] material selected for this draw.
    bool exact_material_identity_verified = false;
    // The draw must independently carry the exact certified Subsurf receiver.
    bool exact_receiver_identity_verified = false;
    // Proven transport: selector material record -> draw-local token, consumed
    // once at the corresponding draw boundary. First-consumer is not pixels.
    bool selector_to_draw_transport_verified = false;
    // Exact variant-preserving HemEnvSubsurf -> ordinary DifSpcBmp replacement
    // identity must be selected for the same authenticated draw.
    bool replacement_identity_verified = false;
    // Substitution must execute inside the unified transaction with stock
    // PS/CB/SRV restoration; failure preserves the host draw.
    bool unified_draw_transaction_verified = false;
};

struct subsurface_runtime_context {
    subsurface_route_context route{};
    subsurface_carrier_kind carrier = subsurface_carrier_kind::create_time_global;

    // Global create-time substitution is legal only with source-complete owner
    // exclusivity because create_pipeline has no draw-local material identity.
    bool create_time_owner_exclusivity_verified = false;

    // Alternative narrow carrier proven structurally by capture-free RE. It is
    // draw-local and therefore does not inherit the global owner-exclusivity
    // requirement, but every coordinate below must close independently.
    subsurface_material_aware_carrier material_aware{};

    bool replacement_shader_materialized = false;
    bool replacement_shader_identity_verified = false;
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

    if (context.carrier == subsurface_carrier_kind::material_aware_draw_local) {
        if (!context.material_aware.exact_material_identity_verified) {
            out.reason = subsurface_runtime_reason::material_aware_material_identity_not_verified;
            return out;
        }
        if (!context.material_aware.exact_receiver_identity_verified) {
            out.reason = subsurface_runtime_reason::material_aware_receiver_identity_not_verified;
            return out;
        }
        if (!context.material_aware.selector_to_draw_transport_verified) {
            out.reason = subsurface_runtime_reason::material_aware_selector_transport_not_verified;
            return out;
        }
        if (!context.material_aware.replacement_identity_verified) {
            out.reason = subsurface_runtime_reason::material_aware_replacement_identity_not_verified;
            return out;
        }
        if (!context.material_aware.unified_draw_transaction_verified) {
            out.reason = subsurface_runtime_reason::material_aware_draw_transaction_not_verified;
            return out;
        }
        out.ready = true;
        out.reason = subsurface_runtime_reason::ready;
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
