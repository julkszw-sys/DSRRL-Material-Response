#pragma once

#include "dsrrl/core/island_policy.hpp"

#include <cstdint>

namespace dsrrl::operators::resource_bridges {

enum class envdiffuse_receiver_family : std::uint8_t {
    unsupported = 0,
    heme_env,
    heme_env_lerp
};

enum class envdiffuse_runtime_reason : std::uint8_t {
    ready = 0,
    core_gate_not_active,
    producer_snapshot_not_ready,
    endpoint_inverse_not_verified,
    alpha_beta_not_preserved,
    draw_multiplier_not_preserved,
    envspec_lanes_not_preserved,
    receiver_not_verified,
    unsupported_receiver_family,
    probe_assignment_not_verified,
    probe_a_not_ready,
    probe_b_not_ready,
    sampler_s11_not_verified,
    sampler_s13_not_verified,
    resource_ownership_not_ready,
    draw_restore_not_ready
};

struct envdiffuse_runtime_context {
    bool producer_snapshot_ready = false;
    bool endpoint_inverse_verified = false;
    bool alpha_beta_preserved = false;
    bool draw_multiplier_preserved = false;
    bool envspec_lanes_preserved = false;

    bool receiver_verified = false;
    envdiffuse_receiver_family receiver_family =
        envdiffuse_receiver_family::unsupported;

    bool exact_probe_assignment_verified = false;
    bool probe_a_srv_ready = false;
    bool probe_b_srv_ready = false;
    bool sampler_s11_verified = false;
    bool sampler_s13_verified = false;
    bool resource_ownership_ready = false;
    bool draw_restore_ready = false;
};

struct envdiffuse_runtime_plan {
    bool ready = false;
    envdiffuse_runtime_reason reason =
        envdiffuse_runtime_reason::core_gate_not_active;

    std::uint8_t probe_a_srv_slot = 11u;
    std::uint8_t probe_b_srv_slot = 13u;
    std::uint8_t probe_a_sampler_slot = 11u;
    std::uint8_t probe_b_sampler_slot = 13u;

    bool bridge_endpoint_xyz_only = true;
    bool preserve_alpha_beta = true;
    bool preserve_draw_multiplier = true;
    bool preserve_envspec_lanes = true;
};

inline envdiffuse_runtime_plan evaluate_envdiffuse_runtime_readiness(
    const core::feature_registry &features,
    const core::activation_context &activation,
    const envdiffuse_runtime_context &context) noexcept
{
    envdiffuse_runtime_plan out;
    const auto core_gate =
        core::evaluate_operator_activation(
            features,
            core::operator_id::env_diffuse,
            activation);

    if (core_gate.state != core::island_state::active) {
        out.reason = envdiffuse_runtime_reason::core_gate_not_active;
        return out;
    }
    if (!context.producer_snapshot_ready) {
        out.reason = envdiffuse_runtime_reason::producer_snapshot_not_ready;
        return out;
    }
    if (!context.endpoint_inverse_verified) {
        out.reason = envdiffuse_runtime_reason::endpoint_inverse_not_verified;
        return out;
    }
    if (!context.alpha_beta_preserved) {
        out.reason = envdiffuse_runtime_reason::alpha_beta_not_preserved;
        return out;
    }
    if (!context.draw_multiplier_preserved) {
        out.reason = envdiffuse_runtime_reason::draw_multiplier_not_preserved;
        return out;
    }
    if (!context.envspec_lanes_preserved) {
        out.reason = envdiffuse_runtime_reason::envspec_lanes_not_preserved;
        return out;
    }
    if (!context.receiver_verified) {
        out.reason = envdiffuse_runtime_reason::receiver_not_verified;
        return out;
    }
    if (context.receiver_family != envdiffuse_receiver_family::heme_env &&
        context.receiver_family != envdiffuse_receiver_family::heme_env_lerp) {
        out.reason = envdiffuse_runtime_reason::unsupported_receiver_family;
        return out;
    }
    if (!context.exact_probe_assignment_verified) {
        out.reason = envdiffuse_runtime_reason::probe_assignment_not_verified;
        return out;
    }
    if (!context.probe_a_srv_ready) {
        out.reason = envdiffuse_runtime_reason::probe_a_not_ready;
        return out;
    }
    if (context.receiver_family == envdiffuse_receiver_family::heme_env_lerp &&
        !context.probe_b_srv_ready) {
        out.reason = envdiffuse_runtime_reason::probe_b_not_ready;
        return out;
    }
    if (!context.sampler_s11_verified) {
        out.reason = envdiffuse_runtime_reason::sampler_s11_not_verified;
        return out;
    }
    if (context.receiver_family == envdiffuse_receiver_family::heme_env_lerp &&
        !context.sampler_s13_verified) {
        out.reason = envdiffuse_runtime_reason::sampler_s13_not_verified;
        return out;
    }
    if (!context.resource_ownership_ready) {
        out.reason = envdiffuse_runtime_reason::resource_ownership_not_ready;
        return out;
    }
    if (!context.draw_restore_ready) {
        out.reason = envdiffuse_runtime_reason::draw_restore_not_ready;
        return out;
    }

    out.ready = true;
    out.reason = envdiffuse_runtime_reason::ready;
    return out;
}

} // namespace dsrrl::operators::resource_bridges
