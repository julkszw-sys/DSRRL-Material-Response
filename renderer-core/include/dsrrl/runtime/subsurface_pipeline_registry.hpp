#pragma once

#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime {

struct subsurface_pipeline_telemetry {
    std::uint64_t pipeline_inits = 0;
    std::uint64_t exact_hits = 0;
    std::uint64_t pixel_binds = 0;
    std::uint64_t exact_binds = 0;
    std::uint64_t lookup_hits = 0;
    std::uint64_t lookup_misses = 0;
};

bool subsurface_receiver_observe_pipeline(
    std::uint64_t pipeline_handle,
    const void *pixel_shader_code,
    std::size_t pixel_shader_size) noexcept;

void subsurface_receiver_forget_pipeline(
    std::uint64_t pipeline_handle) noexcept;

void subsurface_receiver_observe_bind(
    const void *command_list_key,
    bool pixel_stage_bound,
    std::uint64_t pipeline_handle) noexcept;

bool subsurface_receiver_bound(
    const void *command_list_key,
    std::uint32_t &target_plain_receiver_id) noexcept;

void subsurface_receiver_pipeline_reset() noexcept;

subsurface_pipeline_telemetry
subsurface_receiver_pipeline_stats() noexcept;

} // namespace dsrrl::runtime
