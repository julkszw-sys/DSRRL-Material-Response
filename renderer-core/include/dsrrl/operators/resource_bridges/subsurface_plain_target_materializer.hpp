#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::resource_bridges {

enum class subsurface_plain_target_materialize_result : std::uint8_t {
    pass_not_candidate = 0,
    applied,
    fail_invalid_source,
    fail_patch_payload,
    fail_target_identity
};

struct subsurface_plain_target_materialize_outcome {
    subsurface_plain_target_materialize_result result =
        subsurface_plain_target_materialize_result::pass_not_candidate;
    std::uint32_t target_plain_receiver_id = 0u;
};

subsurface_plain_target_materialize_outcome
materialize_subsurface_plain_target(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &target) noexcept;

} // namespace dsrrl::operators::resource_bridges
