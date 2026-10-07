#pragma once

#include <cstdint>

namespace dsrrl::core { class renderer_core; }
namespace reshade::api { struct command_list; }

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

// True only while the DoF island is issuing synthetic D3D11 work.
// Other renderer islands must fail open during this scope so private DoF
// draws/binds cannot mutate shared route/material/resource observations.
bool internal_replay_active() noexcept;

bool handle_draw_event(
    reshade::api::command_list *cmd_list,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance) noexcept;

bool handle_draw_indexed_event(
    reshade::api::command_list *cmd_list,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept;

draw_bridge_telemetry ptde_draw_bridge_status() noexcept;

} // namespace dsrrl::runtime::dof
