#pragma once

#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/core/draw_transaction_policy.hpp"

#include <cstdint>

namespace dsrrl::operators::resource_bridges {

static_assert(
    core::requires_draw_transaction(core::operator_id::spec_rgb),
    "Fixed PointLight SpecRGB bridge must use the shared transaction layer.");

enum class fixed_pointlight_spec_rgb_action : std::uint8_t {
    preserve_host = 0,
    bind_single_t10,
    bind_blended_t10_t16
};

enum class fixed_pointlight_spec_rgb_reason : std::uint8_t {
    active = 0,
    receiver_not_verified,
    material_not_verified,
    no_specular_consumer,
    endpoint_a_not_verified,
    endpoint_a_sidecar_not_ready,
    endpoint_b_not_verified,
    endpoint_b_sidecar_not_ready,
    transport_not_ready,
    stock_endpoints_not_preserved,
    mtd_census_not_authorized
};

struct fixed_pointlight_spec_rgb_context {
    bool exact_fixed_receiver_verified = false;
    bool blended_material = false;
    bool actual_material_verified = false;
    bool material_specular_consumer_verified = false;

    bool endpoint_a_identity_verified = false;
    bool endpoint_a_sidecar_ready = false;

    bool endpoint_b_identity_verified = false;
    bool endpoint_b_sidecar_ready = false;

    bool t10_t16_transport_ready = false;
    bool stock_t1_t4_preserved = false;
};

struct fixed_pointlight_spec_rgb_decision {
    fixed_pointlight_spec_rgb_action action =
        fixed_pointlight_spec_rgb_action::preserve_host;
    fixed_pointlight_spec_rgb_reason reason =
        fixed_pointlight_spec_rgb_reason::receiver_not_verified;

    std::uint8_t endpoint_a_srv_slot = 10u;
    std::uint8_t endpoint_b_srv_slot = 16u;
    bool preserve_stock_t1 = true;
    bool preserve_stock_t4 = true;
};

// Exact fixed PntSS/PntSSSS SpecRGB transport.
// Base endpoint: stock logical t1 -> PTDE RGB sidecar at t10.
// Blended endpoint B: stock logical t4 -> PTDE RGB sidecar at t16.
// t11 is deliberately forbidden here because it is already declared by
// 48/48 audited stock fixed Spc hosts. t10/t16 are fixed-corpus scoped.
fixed_pointlight_spec_rgb_decision
evaluate_fixed_pointlight_spec_rgb_route(
    const fixed_pointlight_spec_rgb_context &context,
    const material_response::mtd_semantic_query &query) noexcept;

} // namespace dsrrl::operators::resource_bridges
