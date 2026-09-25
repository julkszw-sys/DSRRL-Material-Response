#pragma once
#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime {

struct stable_receiver_pipeline_telemetry {
    std::uint64_t pipeline_inits = 0;
    std::uint64_t candidate_size_hits = 0;
    std::uint64_t exact_receiver_hits = 0;
    std::uint64_t candidate_hash_misses = 0;
    std::uint64_t pixel_binds = 0;
    std::uint64_t exact_binds = 0;
    std::uint64_t unknown_binds = 0;
    std::uint64_t lookups = 0;
    std::uint64_t lookup_hits = 0;
    std::uint64_t lookup_misses = 0;
    std::uint64_t handle_conflicts = 0;
};

bool stable_receiver_observe_pipeline(
    std::uint64_t pipeline_handle,
    const void *pixel_shader_code,
    std::size_t pixel_shader_size) noexcept;

void stable_receiver_forget_pipeline(
    std::uint64_t pipeline_handle) noexcept;

void stable_receiver_observe_bind(
    const void *command_list_key,
    bool pixel_stage_bound,
    std::uint64_t pipeline_handle) noexcept;

bool stable_receiver_bound(
    const void *command_list_key,
    std::uint32_t &receiver_id) noexcept;

void stable_receiver_pipeline_reset() noexcept;

stable_receiver_pipeline_telemetry
stable_receiver_pipeline_stats() noexcept;

} // namespace dsrrl::runtime
