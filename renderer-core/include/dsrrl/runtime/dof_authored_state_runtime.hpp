#pragma once

#include <cstdint>

namespace dsrrl::core { class renderer_core; }

namespace dsrrl::runtime::dof {

struct authored_state_telemetry {
    std::uint64_t producer_calls = 0;
    std::uint64_t route_matches = 0;
    std::uint64_t route_misses = 0;
    std::uint64_t ptde_applied = 0;
    bool hook_ready = false;
};

bool register_authored_state_runtime(core::renderer_core &core) noexcept;
void unregister_authored_state_runtime() noexcept;
authored_state_telemetry authored_state_status() noexcept;

} // namespace dsrrl::runtime::dof
