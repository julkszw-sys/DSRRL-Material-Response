#pragma once

#include "dsrrl/operators/lightbank/upper_lower_hemenv_materializer.hpp"

#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime {

struct upper_lower_receiver_identity {
    std::uint16_t plan_index = 0xFFFFu;
    std::uint16_t shader_index = 0xFFFFu;
    std::uint8_t stable_receiver_id = 0u;
    operators::lightbank::upper_lower_hemenv_stratum stratum =
        operators::lightbank::upper_lower_hemenv_stratum::nospc;
    operators::lightbank::upper_lower_hemenv_family family =
        operators::lightbank::upper_lower_hemenv_family::hemenv;

    constexpr bool valid() const noexcept
    {
        return plan_index != 0xFFFFu &&
               shader_index != 0xFFFFu;
    }
};

constexpr bool upper_lower_identity_runtime_shape_valid(
    const upper_lower_receiver_identity &identity) noexcept
{
    if (!identity.valid())
        return false;

    const bool spc =
        identity.stratum ==
            operators::lightbank::
                upper_lower_hemenv_stratum::spc;

    const bool spc_without_stable_receiver_id =
        identity.family ==
            operators::lightbank::
                upper_lower_hemenv_family::phn_faceeye ||
        identity.family ==
            operators::lightbank::
                upper_lower_hemenv_family::gst ||
        identity.family ==
            operators::lightbank::
                upper_lower_hemenv_family::gst_faceeye ||
        identity.family ==
            operators::lightbank::
                upper_lower_hemenv_family::sfx ||
        identity.family ==
            operators::lightbank::
                upper_lower_hemenv_family::snow ||
        identity.family ==
            operators::lightbank::
                upper_lower_hemenv_family::ntoa;

    if (!spc)
        return identity.stable_receiver_id == 0u;

    if (spc_without_stable_receiver_id)
        return identity.stable_receiver_id == 0u;

    return identity.stable_receiver_id >= 24u &&
           identity.stable_receiver_id <= 47u;
}

struct upper_lower_pipeline_telemetry {
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

bool upper_lower_receiver_attest_created_code(
    const void *code,
    std::size_t size,
    const upper_lower_receiver_identity &identity) noexcept;

bool upper_lower_receiver_observe_pipeline(
    std::uint64_t pipeline_handle,
    const void *pixel_shader_code,
    std::size_t pixel_shader_size) noexcept;

void upper_lower_receiver_forget_pipeline(
    std::uint64_t pipeline_handle) noexcept;

void upper_lower_receiver_observe_bind(
    const void *command_list_key,
    bool pixel_stage_bound,
    std::uint64_t pipeline_handle) noexcept;

bool upper_lower_receiver_bound(
    const void *command_list_key,
    upper_lower_receiver_identity &identity) noexcept;

void upper_lower_receiver_pipeline_reset() noexcept;

upper_lower_pipeline_telemetry
upper_lower_receiver_pipeline_stats() noexcept;

} // namespace dsrrl::runtime
