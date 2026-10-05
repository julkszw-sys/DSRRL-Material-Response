#pragma once

#include "dsrrl/operators/dof/dof_island.hpp"

#include <cstdint>

struct ID3D11VertexShader;
struct ID3D11PixelShader;

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

struct retained_native_shader_pair {
    ID3D11VertexShader *vertex = nullptr;
    ID3D11PixelShader *pixel = nullptr;
    operators::dof::retained_shader_role role =
        operators::dof::retained_shader_role::count;
    bool ready = false;
};

bool acquire_retained_native_shader_pair(
    operators::dof::retained_shader_role role,
    retained_native_shader_pair &out) noexcept;

void release_retained_native_shader_pair(
    retained_native_shader_pair &pair) noexcept;

bool retained_execution_set_ready() noexcept;

preflight_telemetry telemetry() noexcept;

} // namespace dsrrl::runtime::dof
