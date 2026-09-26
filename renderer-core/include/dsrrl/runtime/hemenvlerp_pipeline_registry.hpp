#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime {

struct hemenvlerp_receiver_identity {
    std::uint8_t pair_index = 0xffu;
    std::uint32_t semantic_receiver_id = 0u;
    bool exact = false;
};

struct hemenvlerp_pipeline_telemetry {
    std::uint64_t pipeline_inits = 0;
    std::uint64_t candidate_size_hits = 0;
    std::uint64_t exact_hits = 0;
    std::uint64_t hash_misses = 0;
    std::uint64_t pixel_binds = 0;
    std::uint64_t exact_binds = 0;
    std::uint64_t unknown_binds = 0;
    std::uint64_t lookups = 0;
    std::uint64_t lookup_hits = 0;
    std::uint64_t lookup_misses = 0;
    std::uint64_t handle_conflicts = 0;
};

bool hemenvlerp_receiver_observe_pipeline(
    std::uint64_t pipeline_handle,
    const void *pixel_shader_code,
    std::size_t pixel_shader_size) noexcept;

bool hemenvlerp_receiver_observe_pipeline_digest(
    std::uint64_t pipeline_handle,
    const std::array<std::uint8_t,32> &pixel_shader_sha256,
    std::size_t pixel_shader_size) noexcept;

void hemenvlerp_receiver_forget_pipeline(
    std::uint64_t pipeline_handle) noexcept;

void hemenvlerp_receiver_observe_bind(
    const void *command_list_key,
    bool pixel_stage_bound,
    std::uint64_t pipeline_handle) noexcept;

bool hemenvlerp_receiver_bound(
    const void *command_list_key,
    hemenvlerp_receiver_identity &identity) noexcept;

void hemenvlerp_receiver_pipeline_reset() noexcept;

hemenvlerp_pipeline_telemetry
hemenvlerp_receiver_pipeline_stats() noexcept;

} // namespace dsrrl::runtime
