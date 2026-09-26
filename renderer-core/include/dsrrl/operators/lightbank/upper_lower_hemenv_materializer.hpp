#pragma once

#include "dsrrl/core/feature_registry.hpp"
#include "dsrrl/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::lightbank {

enum class upper_lower_hemenv_materialize_result : std::uint8_t {
    applied = 0,
    pass_not_candidate,
    pass_unknown_exact_sha,
    fail_invalid_dxbc,
    fail_plan_inconsistent,
    fail_operand_precondition,
    fail_rebuild,
    fail_stage_sha,
    fail_a1_composition
};

enum class upper_lower_hemenv_stratum : std::uint8_t {
    nospc = 0,
    spc
};

enum class upper_lower_hemenv_family : std::uint8_t {
    hemenv = 0,
    hemenvlerp,
    hemenv_parallax,
    hemenvlerp_parallax,
    phn_pnts,
    phn_faceeye,
    phn_subsurf,
    gst,
    gst_faceeye,
    sfx,
    snow,
    ntoa
};

struct upper_lower_hemenv_materialize_outcome {
    upper_lower_hemenv_materialize_result result =
        upper_lower_hemenv_materialize_result::pass_not_candidate;
    std::uint16_t plan_index = 0xFFFFu;
    std::uint16_t shader_index = 0xFFFFu;
    std::uint8_t stable_receiver_id = 0u;
    upper_lower_hemenv_stratum stratum =
        upper_lower_hemenv_stratum::nospc;
    upper_lower_hemenv_family family =
        upper_lower_hemenv_family::hemenv;
    core::operator_mask composed_owners = 0u;
};

upper_lower_hemenv_materialize_outcome
materialize_upper_lower_hemenv_receiver(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

// Compose only the Upper/Lower consumer cut onto a previously verified
// replacement derived from the same exact stock shader. The caller supplies
// how many SHEX words the verified base inserted before stock word 11.
// No A1 pass is repeated here: composed create-time operators must already
// be present in the verified base.
upper_lower_hemenv_materialize_outcome
augment_upper_lower_hemenv_verified_base(
    const std::uint8_t *stock_source,
    std::size_t stock_size,
    const std::uint8_t *verified_base,
    std::size_t verified_base_size,
    std::uint32_t words_inserted_at_11,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::lightbank
