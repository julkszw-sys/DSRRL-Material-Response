#pragma once
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include <cstdint>

namespace dsrrl::runtime {

struct material_owner_selection_telemetry {
    std::uint64_t selector_events = 0;
    std::uint64_t accepted_callers = 0;
    std::uint64_t owner_enriched = 0;
    std::uint64_t owner_authenticated = 0;
    std::uint64_t actual_material_authenticated = 0;
    std::uint64_t fail_open = 0;
};

void material_owner_selection_clear() noexcept;
bool material_owner_selection_publish(
    const operators::material_response::material_identity &identity) noexcept;
bool material_owner_selection_consume(
    operators::material_response::material_identity &identity) noexcept;

// Read-only snapshot of the TLS owner before an unrouted/dynamic-skip draw
// clears it. A snapshot is never operator, resource or shader authority.
using warrior_9360_log_sink = void (*)(const char *) noexcept;
void warrior_9360_set_log_sink(warrior_9360_log_sink sink) noexcept;

bool material_owner_selection_peek(
    operators::material_response::material_identity &identity) noexcept;

// Exact 8 FLVER SHA-256 identities for Warrior/Standard 9360 only.
// Bounded read-only first-reject witness; compiled only for V230 telemetry.
enum class warrior_9360_stage : std::uint8_t {
    selector_invalid = 0,
    selector_not_authenticated,
    selector_published,
    pre_route,
    no_pipeline_route,
    no_dynamic_route,
    identity_reject,
    identity_ready,
    prepare_reject,
    prepared,
    // Source-stage observations: never authorize a material or shader.
    parser_registered,
    selector_flver_lookup
};
void warrior_9360_trace(
    warrior_9360_stage stage,
    const operators::material_response::material_identity &identity,
    std::uint32_t route_mask,
    std::uint32_t receiver_id,
    const operators::material_response::decision *decision = nullptr) noexcept;

material_owner_selection_telemetry material_owner_selection_stats() noexcept;
void material_owner_selection_reset_stats() noexcept;

} // namespace dsrrl::runtime
