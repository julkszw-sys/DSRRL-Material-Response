#pragma once

#include "dsrrl/operators/dof/dof_island.hpp"

#include <cstddef>
#include <cstdint>

struct ID3D11ShaderResourceView;

namespace reshade::api {
struct command_list;
}

namespace dsrrl::runtime::dof {

struct scheduler_external_inputs {
    // Exact PTDE +0x5C/+0x60 scene-history carrier. This must be a
    // history-preserving 1024x720 BGRA8/Q8 sidecar, never a late R11 copy.
    ID3D11ShaderResourceView *scene_history_q8 = nullptr;

    // Native DSR depth/support representation used as the narrow adapter for
    // PTDE +0x68/+0x8C/+0x9C and terminal retained Dof t5.
    ID3D11ShaderResourceView *depth_support_t1 = nullptr;

    bool scene_history_q8_verified = false;
    bool depth_support_verified = false;
};

struct scheduler_draw_shape {
    bool indexed = false;

    std::uint32_t vertex_count = 0u;
    std::uint32_t index_count = 0u;
    std::uint32_t instance_count = 0u;

    std::uint32_t first_vertex = 0u;
    std::uint32_t first_index = 0u;
    std::int32_t vertex_offset = 0;
    std::uint32_t first_instance = 0u;
};

enum class scheduler_result : std::uint8_t {
    not_ready = 0,
    input_rejected,
    state_capture_failed,
    pass_failed,
    restore_failed,
    executed
};

struct scheduler_telemetry {
    std::uint64_t init_pipeline_events = 0u;
    std::uint64_t exact_shader_pairs = 0u;
    std::uint64_t execute_requests = 0u;
    std::uint64_t execute_ok = 0u;
    std::uint64_t execute_fail = 0u;
    std::uint64_t pass_draws = 0u;
    std::uint64_t restore_fail = 0u;
    bool execution_set_ready = false;
    bool quarantined = false;
};

bool register_ptde_scheduler_runtime() noexcept;
void unregister_ptde_scheduler_runtime() noexcept;

bool ptde_scheduler_execution_set_ready(
    const scheduler_external_inputs &inputs) noexcept;

bool ptde_scheduler_plain_rate_ready() noexcept;

scheduler_result execute_ptde_pass(
    reshade::api::command_list *cmd_list,
    const operators::dof::activation_context &activation,
    const scheduler_external_inputs &inputs,
    const scheduler_draw_shape &shape,
    std::size_t pass_index) noexcept;

scheduler_result execute_ptde_graph(
    reshade::api::command_list *cmd_list,
    const operators::dof::activation_context &activation,
    const scheduler_external_inputs &inputs,
    const scheduler_draw_shape &shape,
    ID3D11ShaderResourceView **terminal_output) noexcept;

scheduler_telemetry ptde_scheduler_status() noexcept;

} // namespace dsrrl::runtime::dof
