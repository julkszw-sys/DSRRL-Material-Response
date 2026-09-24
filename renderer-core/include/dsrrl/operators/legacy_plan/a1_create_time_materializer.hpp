#pragma once

#include "dsrrl/core/feature_registry.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/generated_a1_exact_patch_recipes_v1.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace dsrrl::operators::legacy_plan {

inline constexpr core::operator_mask a1_create_time_supported_operators =
    core::operator_bit(core::operator_id::terminal_sat_rgb) |
    core::operator_bit(core::operator_id::diffuse_material_domain) |
    core::operator_bit(core::operator_id::pointlight_pnts_attenuation) |
    core::operator_bit(core::operator_id::envspec_nospc_delete) |
    core::operator_bit(core::operator_id::fixed_postfog_identity);

enum class a1_create_time_result : std::uint8_t {
    applied = 0,
    pass_through_not_candidate_size,
    pass_through_unknown_exact_sha,
    pass_through_no_enabled_owner,
    fail_open_invalid_dxbc,
    fail_open_plan_inconsistent,
    fail_open_token_mismatch,
    fail_open_checksum,
    fail_open_full_replacement_sha_mismatch
};

struct a1_create_time_outcome {
    a1_create_time_result result =
        a1_create_time_result::pass_through_unknown_exact_sha;

    core::operator_mask selected_owners = 0;
    std::uint16_t selected_ops = 0;
    bool full_plan_materialized = false;

    core::sha256_digest source_sha256{};
    core::sha256_digest output_sha256{};

    const generated::a1_exact_patch_plan *plan = nullptr;
};

inline bool a1_candidate_code_size(std::size_t size) noexcept
{
    for (const auto &plan : generated::k_a1_exact_patch_plans_v1)
        if (plan.code_size == size)
            return true;

    return false;
}

inline const generated::a1_exact_patch_plan *find_a1_plan_by_exact_digest(
    std::size_t size,
    const core::sha256_digest &digest) noexcept
{
    for (const auto &plan : generated::k_a1_exact_patch_plans_v1) {
        if (plan.code_size != size)
            continue;

        if (hashing::matches_hex(digest, plan.original_sha256))
            return &plan;
    }

    return nullptr;
}

inline bool a1_materializable_owner(core::operator_id owner) noexcept
{
    return
        (a1_create_time_supported_operators &
         core::operator_bit(owner)) != 0u;
}

// Lower-level transaction used only after the caller has already resolved an
// exact original SHA-256 to one generated A1 plan.
//
// The function validates every selected owner DWORD against the original word
// before allocating a replacement. Source bytes are never modified. A failed
// transaction returns with output empty.
inline a1_create_time_outcome materialize_verified_a1_plan(
    const core::feature_registry &features,
    const generated::a1_exact_patch_plan &plan,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept
{
    a1_create_time_outcome outcome;
    outcome.plan = &plan;
    output.clear();

    if (!dxbc::checksum_container_valid(source, size) ||
        size != plan.code_size) {
        outcome.result = a1_create_time_result::fail_open_invalid_dxbc;
        return outcome;
    }

    if (plan.first_op > generated::k_a1_exact_patch_op_count_v1 ||
        plan.op_count >
            generated::k_a1_exact_patch_op_count_v1 - plan.first_op) {
        outcome.result =
            a1_create_time_result::fail_open_plan_inconsistent;
        return outcome;
    }

    for (std::size_t i = 0; i < plan.op_count; ++i) {
        const auto &op =
            generated::k_a1_exact_patch_ops_v1[plan.first_op + i];

        if (!a1_materializable_owner(op.owner)) {
            outcome.result =
                a1_create_time_result::fail_open_plan_inconsistent;
            return outcome;
        }

        if (!features.enabled(op.owner))
            continue;

        outcome.selected_owners |= core::operator_bit(op.owner);
        ++outcome.selected_ops;

        if (op.byte_offset > size ||
            size - op.byte_offset < sizeof(std::uint32_t)) {
            outcome.result =
                a1_create_time_result::fail_open_plan_inconsistent;
            return outcome;
        }

        std::uint32_t word = 0;
        std::memcpy(
            &word,
            source + op.byte_offset,
            sizeof(word));

        if (word != op.expected_old_word) {
            outcome.result =
                a1_create_time_result::fail_open_token_mismatch;
            return outcome;
        }
    }

    if (outcome.selected_ops == 0u) {
        outcome.result =
            a1_create_time_result::pass_through_no_enabled_owner;
        return outcome;
    }

    try {
        output.assign(source, source + size);
    } catch (...) {
        output.clear();
        outcome.result =
            a1_create_time_result::fail_open_plan_inconsistent;
        return outcome;
    }

    for (std::size_t i = 0; i < plan.op_count; ++i) {
        const auto &op =
            generated::k_a1_exact_patch_ops_v1[plan.first_op + i];

        if ((outcome.selected_owners &
             core::operator_bit(op.owner)) == 0u)
            continue;

        std::memcpy(
            output.data() + op.byte_offset,
            &op.replacement_word,
            sizeof(op.replacement_word));
    }

    if (!dxbc::fix_checksum(output.data(), output.size())) {
        output.clear();
        outcome.result = a1_create_time_result::fail_open_checksum;
        return outcome;
    }

    outcome.full_plan_materialized =
        outcome.selected_ops == plan.op_count;

    outcome.output_sha256 =
        hashing::sha256(output.data(), output.size());

    if (outcome.full_plan_materialized &&
        !hashing::matches_hex(
            outcome.output_sha256,
            plan.replacement_sha256)) {
        output.clear();
        outcome.result =
            a1_create_time_result::
                fail_open_full_replacement_sha_mismatch;
        return outcome;
    }

    outcome.result = a1_create_time_result::applied;
    return outcome;
}

// Public create-time entrypoint.
//
// It does not trust a fast hash or caller-provided identity. Candidate size is
// only a cheap prefilter; the selected plan is resolved from SHA-256 of the
// exact source DXBC bytes. Feature gates are independent per semantic island.
// The output buffer is populated only after a complete fail-open preflight.
inline a1_create_time_outcome materialize_a1_create_time(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept
{
    a1_create_time_outcome outcome;
    output.clear();

    if (!a1_candidate_code_size(size)) {
        outcome.result =
            a1_create_time_result::pass_through_not_candidate_size;
        return outcome;
    }

    if (!dxbc::checksum_container_valid(source, size)) {
        outcome.result =
            a1_create_time_result::fail_open_invalid_dxbc;
        return outcome;
    }

    outcome.source_sha256 = hashing::sha256(source, size);

    const auto *plan =
        find_a1_plan_by_exact_digest(size, outcome.source_sha256);

    if (plan == nullptr) {
        outcome.result =
            a1_create_time_result::pass_through_unknown_exact_sha;
        return outcome;
    }

    outcome =
        materialize_verified_a1_plan(
            features,
            *plan,
            source,
            size,
            output);

    outcome.source_sha256 = hashing::sha256(source, size);
    return outcome;
}

static_assert(
    generated::k_a1_exact_patch_plan_count_v1 == 144u,
    "A1 exact create-time materializer expects 144 certified plans.");

static_assert(
    generated::k_a1_exact_patch_op_count_v1 == 312u,
    "A1 exact create-time materializer expects 312 exact DWORD ops.");

} // namespace dsrrl::operators::legacy_plan
