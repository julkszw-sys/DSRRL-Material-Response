#pragma once

#include "dsrrl/core/types.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/generated_a1_exact_patch_recipes_v1.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace dsrrl::operators::legacy_plan {

enum class a1_compose_result : std::uint8_t {
    applied = 0,
    already_applied,
    no_requested_owner,
    fail_open_unknown_shader,
    fail_open_code_size_mismatch,
    fail_open_invalid_dxbc_container,
    fail_open_unrequested_patch_present,
    fail_open_token_mismatch,
    fail_open_owner_mixed_state
};

struct a1_compose_report {
    a1_compose_result result = a1_compose_result::fail_open_unknown_shader;
    core::operator_mask requested_owner_mask = 0;
    core::operator_mask present_owner_mask = 0;
    core::operator_mask applied_owner_mask = 0;
    std::uint16_t requested_op_count = 0;
    std::uint16_t applied_op_count = 0;
};

inline constexpr core::operator_mask a1_exact_supported_owner_mask =
    core::operator_bit(core::operator_id::terminal_sat_rgb) |
    core::operator_bit(core::operator_id::diffuse_material_domain) |
    core::operator_bit(core::operator_id::pointlight_pnts_attenuation) |
    core::operator_bit(core::operator_id::envspec_nospc_delete) |
    core::operator_bit(core::operator_id::fixed_postfog_identity);

inline const generated::a1_exact_patch_plan *find_a1_exact_compose_plan(
    std::string_view original_sha256) noexcept
{
    for (const auto &plan : generated::k_a1_exact_patch_plans_v1) {
        if (original_sha256 == plan.original_sha256)
            return &plan;
    }

    return nullptr;
}

namespace detail {

struct owner_state {
    std::uint16_t total = 0;
    std::uint16_t old_words = 0;
    std::uint16_t new_words = 0;
};

inline std::size_t owner_slot(core::operator_id owner) noexcept
{
    switch (owner) {
    case core::operator_id::terminal_sat_rgb: return 0u;
    case core::operator_id::diffuse_material_domain: return 1u;
    case core::operator_id::pointlight_pnts_attenuation: return 2u;
    case core::operator_id::envspec_nospc_delete: return 3u;
    case core::operator_id::fixed_postfog_identity: return 4u;
    default: return 5u;
    }
}

} // namespace detail

// Composes the exact recovered A1 operations for the requested closed islands.
//
// Safety contract:
// - exact original shader SHA-256 identifies the recipe;
// - exact DXBC size and container shape must match;
// - every known A1 patch word is inspected before any write;
// - requested owners may be fully-old or fully-new, but never internally mixed;
// - an unrequested owner's replacement word is rejected instead of silently
//   inheriting a disabled legacy operator;
// - mutation happens only after complete validation;
// - the DXBC checksum is repaired exactly once after the final mutation set.
//
// This function is construction/materialization only. The requested owner mask
// must already have been produced by feature/receiver/material policy.
inline a1_compose_report compose_a1_exact_islands(
    std::uint8_t *dxbc_bytes,
    std::size_t dxbc_size,
    std::string_view original_sha256,
    core::operator_mask requested_owner_mask) noexcept
{
    a1_compose_report report;
    report.requested_owner_mask =
        requested_owner_mask & a1_exact_supported_owner_mask;

    const auto *plan = find_a1_exact_compose_plan(original_sha256);
    if (plan == nullptr) {
        report.result = a1_compose_result::fail_open_unknown_shader;
        return report;
    }

    if (dxbc_bytes == nullptr || dxbc_size != plan->code_size) {
        report.result = a1_compose_result::fail_open_code_size_mismatch;
        return report;
    }

    if (!dxbc::checksum_container_valid(dxbc_bytes, dxbc_size)) {
        report.result = a1_compose_result::fail_open_invalid_dxbc_container;
        return report;
    }

    std::array<detail::owner_state, 5> states{};

    for (std::size_t i = 0; i < plan->op_count; ++i) {
        const auto &op =
            generated::k_a1_exact_patch_ops_v1[plan->first_op + i];

        const auto slot = detail::owner_slot(op.owner);
        if (slot >= states.size()) {
            report.result = a1_compose_result::fail_open_token_mismatch;
            return report;
        }

        report.present_owner_mask |= core::operator_bit(op.owner);

        if (op.byte_offset > dxbc_size ||
            dxbc_size - op.byte_offset < sizeof(std::uint32_t)) {
            report.result = a1_compose_result::fail_open_token_mismatch;
            return report;
        }

        std::uint32_t word = 0;
        std::memcpy(
            &word,
            dxbc_bytes + op.byte_offset,
            sizeof(word));

        const bool requested =
            (report.requested_owner_mask & core::operator_bit(op.owner)) != 0u;

        if (!requested) {
            if (word == op.replacement_word) {
                report.result =
                    a1_compose_result::fail_open_unrequested_patch_present;
                return report;
            }

            if (word != op.expected_old_word) {
                report.result = a1_compose_result::fail_open_token_mismatch;
                return report;
            }

            continue;
        }

        auto &state = states[slot];
        ++state.total;
        ++report.requested_op_count;

        if (word == op.expected_old_word)
            ++state.old_words;
        else if (word == op.replacement_word)
            ++state.new_words;
        else {
            report.result = a1_compose_result::fail_open_token_mismatch;
            return report;
        }
    }

    report.requested_owner_mask &= report.present_owner_mask;

    if (report.requested_owner_mask == 0u) {
        report.result = a1_compose_result::no_requested_owner;
        return report;
    }

    for (std::size_t slot = 0; slot < states.size(); ++slot) {
        const auto &state = states[slot];
        if (state.total == 0u)
            continue;

        if (state.old_words != 0u && state.new_words != 0u) {
            report.result = a1_compose_result::fail_open_owner_mixed_state;
            return report;
        }

        if (state.old_words + state.new_words != state.total) {
            report.result = a1_compose_result::fail_open_token_mismatch;
            return report;
        }
    }

    bool needs_write = false;

    for (std::size_t i = 0; i < plan->op_count; ++i) {
        const auto &op =
            generated::k_a1_exact_patch_ops_v1[plan->first_op + i];

        if ((report.requested_owner_mask & core::operator_bit(op.owner)) == 0u)
            continue;

        const auto slot = detail::owner_slot(op.owner);
        if (slot >= states.size())
            continue;

        if (states[slot].old_words == states[slot].total) {
            needs_write = true;
            report.applied_owner_mask |= core::operator_bit(op.owner);
        }
    }

    if (!needs_write) {
        report.result = a1_compose_result::already_applied;
        return report;
    }

    // All validation is complete. Apply only owner sets proven fully old.
    for (std::size_t i = 0; i < plan->op_count; ++i) {
        const auto &op =
            generated::k_a1_exact_patch_ops_v1[plan->first_op + i];

        if ((report.applied_owner_mask & core::operator_bit(op.owner)) == 0u)
            continue;

        std::memcpy(
            dxbc_bytes + op.byte_offset,
            &op.replacement_word,
            sizeof(op.replacement_word));
        ++report.applied_op_count;
    }

    // Container shape was validated before mutation, so this is not expected
    // to fail. Keep fail-open semantics at the validation boundary rather than
    // creating a post-write error state.
    static_cast<void>(dxbc::fix_checksum(dxbc_bytes, dxbc_size));

    report.result = a1_compose_result::applied;
    return report;
}

} // namespace dsrrl::operators::legacy_plan
