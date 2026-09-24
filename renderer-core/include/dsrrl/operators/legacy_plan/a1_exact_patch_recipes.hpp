#pragma once

#include "dsrrl/operators/legacy_plan/generated_a1_exact_patch_recipes_v1.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace dsrrl::operators::legacy_plan {

enum class exact_patch_result : std::uint8_t {
    applied = 0,
    already_applied,
    fail_open_unknown_shader,
    fail_open_code_size_mismatch,
    fail_open_no_ops_for_owner,
    fail_open_out_of_bounds,
    fail_open_token_mismatch,
    fail_open_mixed_state
};

inline const generated::a1_exact_patch_plan *find_a1_exact_patch_plan(
    std::string_view original_sha256) noexcept
{
    for (const auto &plan : generated::k_a1_exact_patch_plans_v1)
        if (original_sha256 == plan.original_sha256)
            return &plan;
    return nullptr;
}

// Applies exactly one semantic island's recovered P2.2 DWORD operations.
//
// Selection must use the SHA-256 of the original/unmodified shader body.
// Exact code size is mandatory. Every owner-targeted word is validated before
// any mutation is performed, so token mismatch, bounds failure or a mixed
// old/new state fails open without a partial write.
inline exact_patch_result apply_a1_exact_owner_patch(
    std::uint8_t *dxbc,
    std::size_t dxbc_size,
    std::string_view original_sha256,
    core::operator_id owner) noexcept
{
    const auto *plan = find_a1_exact_patch_plan(original_sha256);
    if (plan == nullptr)
        return exact_patch_result::fail_open_unknown_shader;

    if (dxbc == nullptr || dxbc_size != plan->code_size)
        return exact_patch_result::fail_open_code_size_mismatch;

    std::size_t owner_ops = 0;
    std::size_t old_words = 0;
    std::size_t new_words = 0;

    for (std::size_t i = 0; i < plan->op_count; ++i) {
        const auto &op =
            generated::k_a1_exact_patch_ops_v1[plan->first_op + i];

        if (op.owner != owner)
            continue;

        ++owner_ops;

        if (op.byte_offset > dxbc_size ||
            dxbc_size - op.byte_offset < sizeof(std::uint32_t))
            return exact_patch_result::fail_open_out_of_bounds;

        std::uint32_t word = 0;
        std::memcpy(&word, dxbc + op.byte_offset, sizeof(word));

        if (word == op.expected_old_word)
            ++old_words;
        else if (word == op.replacement_word)
            ++new_words;
        else
            return exact_patch_result::fail_open_token_mismatch;
    }

    if (owner_ops == 0)
        return exact_patch_result::fail_open_no_ops_for_owner;

    if (old_words != 0 && new_words != 0)
        return exact_patch_result::fail_open_mixed_state;

    if (new_words == owner_ops)
        return exact_patch_result::already_applied;

    if (old_words != owner_ops)
        return exact_patch_result::fail_open_token_mismatch;

    for (std::size_t i = 0; i < plan->op_count; ++i) {
        const auto &op =
            generated::k_a1_exact_patch_ops_v1[plan->first_op + i];

        if (op.owner == owner)
            std::memcpy(
                dxbc + op.byte_offset,
                &op.replacement_word,
                sizeof(op.replacement_word));
    }

    return exact_patch_result::applied;
}

} // namespace dsrrl::operators::legacy_plan
