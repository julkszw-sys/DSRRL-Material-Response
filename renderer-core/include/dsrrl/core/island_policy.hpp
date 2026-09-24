#pragma once

#include "feature_registry.hpp"
#include "operator_catalog.hpp"

#include <cstdint>

namespace dsrrl::core {

struct activation_context {
    bool receiver_verified = false;
    bool material_verified = false;
    bool resource_ready = false;
    bool producer_ready = false;
    bool consumer_verified = false;
    bool immediate_context = false;
    bool graph_ready = false;
    bool allow_diagnostic = false;
};

enum class activation_reason : std::uint8_t {
    active = 0,
    disabled,
    stock_host_preserved,
    blocked,
    diagnostic_not_allowed,
    missing_receiver,
    missing_material,
    missing_resource,
    missing_producer,
    missing_consumer,
    wrong_context,
    missing_graph
};

struct activation_decision {
    island_state state = island_state::disabled;
    activation_reason reason = activation_reason::disabled;
};

activation_decision evaluate_operator_activation(
    const feature_registry &features,
    operator_id id,
    const activation_context &context) noexcept;

} // namespace dsrrl::core
