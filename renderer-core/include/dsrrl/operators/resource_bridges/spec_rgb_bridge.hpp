#pragma once

#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"

#include <cstdint>

namespace dsrrl::operators::resource_bridges {

enum class spec_rgb_receiver_family : std::uint8_t {
    unsupported = 0,
    dif_spc_bmp,
    dif_spc
};

enum class spec_rgb_action : std::uint8_t {
    preserve_host = 0,
    bind_ptde_t10_rgb
};

enum class spec_rgb_reason : std::uint8_t {
    active = 0,
    unsupported_receiver,
    material_not_verified,
    no_specular_consumer,
    ptde_companion_not_verified,
    sidecar_not_ready,
    t10_transport_not_ready,
    stock_t1_not_preserved,
    mtd_census_not_authorized
};

struct spec_rgb_context {
    std::uint32_t receiver_id = 0;
    bool actual_material_verified = false;
    bool material_specular_consumer_verified = false;
    bool exact_name_ptde_companion_verified = false;
    bool ptde_sidecar_ready = false;
    bool native_t10_transport_ready = false;
    bool stock_t1_preserved = false;
};

struct spec_rgb_decision {
    spec_rgb_action action = spec_rgb_action::preserve_host;
    spec_rgb_reason reason = spec_rgb_reason::unsupported_receiver;
    spec_rgb_receiver_family family = spec_rgb_receiver_family::unsupported;
    std::uint32_t receiver_id = 0;
    std::uint8_t ptde_rgb_srv_slot = 10;
    bool preserve_stock_t1 = true;
};

// Confirmed FULL24 no-PointLight stable HemEnv SpecRGB routing contract.
//
// receivers 24..35 -> DifSpcBmp family
// receivers 36..47 -> DifSpc family
//
// PTDE SpecRGB is an independent RGB resource at t10. Stock DSR t1 is retained
// unchanged, including alpha/roughness. Material and exact logical texture
// identity are mandatory because shared hosts/materials exist. Any incomplete
// route fails open to the stock DSR resource state.
spec_rgb_decision evaluate_spec_rgb_route(
    const spec_rgb_context &context) noexcept;

spec_rgb_decision evaluate_spec_rgb_route(
    const spec_rgb_context &context,
    const material_response::mtd_semantic_query &query) noexcept;

} // namespace dsrrl::operators::resource_bridges
