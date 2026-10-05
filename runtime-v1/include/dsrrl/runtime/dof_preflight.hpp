#pragma once

#include "dsrrl/operators/dof/dof_island.hpp"

#include <cstdint>

namespace reshade::api {
struct command_list;
}

namespace dsrrl::runtime::dof {

struct preflight_telemetry {
    std::uint64_t init_pipeline_events = 0;
    std::uint64_t exact_pipeline_hits = 0;
    std::uint64_t bind_hits = 0;
    std::uint64_t bind_misses = 0;
    std::uint32_t seen_role_mask = 0;
    bool active_flat_set_seen = false;
    bool plain_dofrate_seen = false;
};

bool register_preflight_runtime() noexcept;
void unregister_preflight_runtime() noexcept;

bool bound_retained_role(
    reshade::api::command_list *cmd_list,
    operators::dof::retained_shader_role &role) noexcept;

preflight_telemetry telemetry() noexcept;

} // namespace dsrrl::runtime::dof
