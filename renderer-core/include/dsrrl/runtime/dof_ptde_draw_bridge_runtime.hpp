#pragma once

#include <cstdint>

namespace dsrrl::core { class renderer_core; }

namespace dsrrl::runtime::dof {

struct draw_bridge_telemetry {
    std::uint64_t first_pass_hits = 0u;
    std::uint64_t sequences_started = 0u;
    std::uint64_t sequences_completed = 0u;
    std::uint64_t sequence_failures = 0u;
    std::uint64_t dry_run_pass = 0u;
    std::uint64_t visible_handoffs = 0u;
    std::uint64_t tonemap_fallbacks = 0u;
    std::uint64_t missing_host_pass10 = 0u;
    std::uint64_t restore_failures = 0u;
    bool opt_in = false;
    bool armed = false;
    bool quarantined = false;
};

bool register_ptde_draw_bridge_runtime(
    core::renderer_core &core) noexcept;
void unregister_ptde_draw_bridge_runtime() noexcept;

draw_bridge_telemetry ptde_draw_bridge_status() noexcept;

} // namespace dsrrl::runtime::dof
