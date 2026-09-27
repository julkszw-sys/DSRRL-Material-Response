#pragma once

#include "dsrrl/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::point_light {

enum class clustered_pnts_direct_materialize_result : std::uint8_t {
    applied = 0,
    pass_not_candidate,
    pass_unknown_exact_sha,
    fail_invalid_dxbc,
    fail_patch_precondition,
    fail_rebuild,
    fail_final_sha
};

struct clustered_pnts_direct_materialize_outcome {
    clustered_pnts_direct_materialize_result result =
        clustered_pnts_direct_materialize_result::pass_not_candidate;
    core::sha256_digest host_sha256{};
    core::sha256_digest replacement_sha256{};
    std::size_t host_size = 0u;
    std::size_t replacement_size = 0u;
    std::uint32_t representative_shader_index = 0u;
    bool spc = false;
    bool blended_material = false;
};

clustered_pnts_direct_materialize_outcome
materialize_clustered_pnts_direct_ptde(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::point_light
