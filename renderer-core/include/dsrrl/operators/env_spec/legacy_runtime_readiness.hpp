#pragma once

#include "dsrrl/core/island_policy.hpp"

#include <cstdint>

namespace dsrrl::operators::env_spec {

enum class legacy_resource_class : std::uint8_t {
    unsupported = 0,
    packed_gi,
    classic_rgb24
};

enum class legacy_runtime_reason : std::uint8_t {
    ready = 0,
    core_gate_not_active,
    unsupported_resource_class,
    receiver_not_verified,
    material_not_verified,
    ptde_receiver_math_not_ready,
    dsr_pbl_bypass_not_ready,
    material_response_b12_not_ready,
    sampler_not_verified,
    sidecar_lookup_not_ready,
    packed_gi_resource_not_ready,
    packed_gi_alpha_not_preserved,
    classic_resource_not_ready,
    classic_api_alpha_not_one,
    classic_selector_algorithm_not_verified,
    classic_authored_selector_values_not_verified,
    draw_transaction_not_ready
};

struct legacy_runtime_context {
    legacy_resource_class resource_class =
        legacy_resource_class::unsupported;

    bool receiver_verified = false;
    bool material_verified = false;
    bool ptde_receiver_math_ready = false;
    bool dsr_pbl_bypass_ready = false;
    bool material_response_b12_ready = false;
    bool sampler_descriptor_verified = false;
    bool semantic_sidecar_lookup_ready = false;

    bool packed_gi_resource_ready = false;
    bool packed_gi_stored_alpha_preserved = false;

    bool classic_resource_ready = false;
    bool classic_api_alpha_one = false;
    bool classic_selector_algorithm_verified = false;
    bool classic_authored_selector_values_verified = false;

    bool draw_transaction_ready = false;
};

struct legacy_runtime_plan {
    bool ready = false;
    legacy_runtime_reason reason =
        legacy_runtime_reason::core_gate_not_active;
    bool bypass_dsr_pbl_tail = true;
    bool preserve_ptde_sample_alpha = true;
};

inline legacy_runtime_plan evaluate_legacy_runtime_readiness(
    const core::feature_registry &features,
    const core::activation_context &activation,
    const legacy_runtime_context &context) noexcept
{
    legacy_runtime_plan out;
    const auto core_gate =
        core::evaluate_operator_activation(
            features,
            core::operator_id::env_spec,
            activation);

    if (core_gate.state != core::island_state::active) {
        out.reason = legacy_runtime_reason::core_gate_not_active;
        return out;
    }
    if (context.resource_class == legacy_resource_class::unsupported) {
        out.reason = legacy_runtime_reason::unsupported_resource_class;
        return out;
    }
    if (!context.receiver_verified) {
        out.reason = legacy_runtime_reason::receiver_not_verified;
        return out;
    }
    if (!context.material_verified) {
        out.reason = legacy_runtime_reason::material_not_verified;
        return out;
    }
    if (!context.ptde_receiver_math_ready) {
        out.reason = legacy_runtime_reason::ptde_receiver_math_not_ready;
        return out;
    }
    if (!context.dsr_pbl_bypass_ready) {
        out.reason = legacy_runtime_reason::dsr_pbl_bypass_not_ready;
        return out;
    }
    if (!context.material_response_b12_ready) {
        out.reason = legacy_runtime_reason::material_response_b12_not_ready;
        return out;
    }
    if (!context.sampler_descriptor_verified) {
        out.reason = legacy_runtime_reason::sampler_not_verified;
        return out;
    }
    if (!context.semantic_sidecar_lookup_ready) {
        out.reason = legacy_runtime_reason::sidecar_lookup_not_ready;
        return out;
    }

    if (context.resource_class == legacy_resource_class::packed_gi) {
        if (!context.packed_gi_resource_ready) {
            out.reason = legacy_runtime_reason::packed_gi_resource_not_ready;
            return out;
        }
        if (!context.packed_gi_stored_alpha_preserved) {
            out.reason =
                legacy_runtime_reason::packed_gi_alpha_not_preserved;
            return out;
        }
    } else {
        if (!context.classic_resource_ready) {
            out.reason = legacy_runtime_reason::classic_resource_not_ready;
            return out;
        }
        if (!context.classic_api_alpha_one) {
            out.reason = legacy_runtime_reason::classic_api_alpha_not_one;
            return out;
        }
        if (!context.classic_selector_algorithm_verified) {
            out.reason =
                legacy_runtime_reason::classic_selector_algorithm_not_verified;
            return out;
        }
        if (!context.classic_authored_selector_values_verified) {
            out.reason =
                legacy_runtime_reason::
                    classic_authored_selector_values_not_verified;
            return out;
        }
    }

    if (!context.draw_transaction_ready) {
        out.reason = legacy_runtime_reason::draw_transaction_not_ready;
        return out;
    }

    out.ready = true;
    out.reason = legacy_runtime_reason::ready;
    return out;
}

} // namespace dsrrl::operators::env_spec
