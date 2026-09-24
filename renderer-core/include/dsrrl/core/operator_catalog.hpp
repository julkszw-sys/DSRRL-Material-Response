#pragma once

#include "types.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace dsrrl::core {

enum class canonical_status : std::uint8_t {
    confirmed = 0,
    high_confidence,
    hypothesis,
    rejected
};

enum class port_state : std::uint8_t {
    off = 0,
    active_candidate,
    partial,
    blocked,
    diagnostic,
    stock_host
};

enum class carrier_kind : std::uint8_t {
    none = 0,
    param,
    shader,
    constant_buffer,
    resource,
    material,
    asset,
    hybrid,
    composite,
    host_preserve
};

enum requirement_bit : std::uint32_t {
    require_none = 0,
    require_receiver = 1u << 0,
    require_material = 1u << 1,
    require_resource = 1u << 2,
    require_producer = 1u << 3,
    require_consumer = 1u << 4,
    require_immediate_context = 1u << 5,
    require_graph = 1u << 6
};

struct operator_contract {
    operator_id id = operator_id::material_response;
    const char *operator_key = nullptr;
    const char *canonical_key = nullptr;
    const char *display_name = nullptr;
    canonical_status status = canonical_status::confirmed;
    port_state default_state = port_state::off;
    carrier_kind carrier = carrier_kind::none;
    std::uint32_t requirements = require_none;
    bool fail_open_stock = true;
};

const std::array<operator_contract, operator_count> &known_operator_catalog() noexcept;
std::optional<operator_contract> find_operator_contract(operator_id id) noexcept;
std::optional<operator_contract> find_operator_contract(const char *operator_key) noexcept;

} // namespace dsrrl::core
