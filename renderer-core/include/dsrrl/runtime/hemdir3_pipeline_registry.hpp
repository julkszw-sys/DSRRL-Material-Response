#pragma once

#include "dsrrl/operators/lightbank/hemdir3_b13_materializer.hpp"

#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime {

struct hemdir3_receiver_identity {
    std::uint16_t plan_index = 0xFFFFu;
    std::uint16_t shader_index = 0xFFFFu;
    operators::lightbank::hemdir3_native_stratum stratum =
        operators::lightbank::hemdir3_native_stratum::nospc;
    std::uint8_t paired_stable_receiver_id = 0u;

    constexpr bool valid() const noexcept
    {
        return plan_index != 0xFFFFu &&
               shader_index != 0xFFFFu;
    }
};

struct hemdir3_pipeline_telemetry {
    std::uint64_t created_code_attested = 0;
    std::uint64_t created_code_conflict = 0;
    std::uint64_t pipeline_inits = 0;
    std::uint64_t exact_nospc_hits = 0;
    std::uint64_t exact_spc_hits = 0;
    std::uint64_t init_mismatch = 0;
    std::uint64_t pixel_binds = 0;
    std::uint64_t nospc_binds = 0;
    std::uint64_t spc_binds = 0;
    std::uint64_t unknown_binds = 0;
    std::uint64_t lookups = 0;
    std::uint64_t lookup_hits = 0;
    std::uint64_t lookup_misses = 0;
    bool quarantined = false;
};

bool hemdir3_receiver_attest_created_code(
    const void *code,
    std::size_t size,
    const hemdir3_receiver_identity &identity) noexcept;

bool hemdir3_receiver_observe_pipeline(
    std::uint64_t pipeline_handle,
    const void *pixel_shader_code,
    std::size_t pixel_shader_size) noexcept;

void hemdir3_receiver_forget_pipeline(
    std::uint64_t pipeline_handle) noexcept;

void hemdir3_receiver_observe_bind(
    const void *command_list_key,
    bool pixel_stage_bound,
    std::uint64_t pipeline_handle) noexcept;

bool hemdir3_receiver_bound(
    const void *command_list_key,
    hemdir3_receiver_identity &identity) noexcept;

void hemdir3_receiver_pipeline_reset() noexcept;

hemdir3_pipeline_telemetry
hemdir3_receiver_pipeline_stats() noexcept;

} // namespace dsrrl::runtime
