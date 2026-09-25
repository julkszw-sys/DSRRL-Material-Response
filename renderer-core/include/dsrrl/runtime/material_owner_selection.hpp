#pragma once
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include <cstdint>
#include <optional>

namespace dsrrl::runtime {

struct material_owner_selection_telemetry {
    std::uint64_t selector_events = 0;
    std::uint64_t accepted_callers = 0;
    std::uint64_t owner_enriched = 0;
    std::uint64_t owner_authenticated = 0;
    std::uint64_t fail_open = 0;
};

void material_owner_selection_clear() noexcept;
void material_owner_selection_publish(
    const operators::material_response::material_identity &identity) noexcept;
std::optional<operators::material_response::material_identity>
material_owner_selection_current() noexcept;
material_owner_selection_telemetry material_owner_selection_stats() noexcept;
void material_owner_selection_reset_stats() noexcept;

} // namespace dsrrl::runtime
