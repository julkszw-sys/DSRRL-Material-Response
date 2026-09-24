#include "dsrrl/operators/legacy_plan/a1_create_time_materializer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>
#include <vector>

using namespace dsrrl;

namespace {

bool check(bool condition, const char *expression, int line)
{
    if (condition)
        return true;

    std::cerr << "CHECK FAILED line " << line << ": " << expression << '\n';
    return false;
}

#define CHECK(expr) do { if (!check(static_cast<bool>(expr), #expr, __LINE__)) return 1; } while (false)

void write_word(
    std::vector<std::uint8_t> &bytes,
    std::size_t offset,
    std::uint32_t value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::uint32_t read_word(
    const std::vector<std::uint8_t> &bytes,
    std::size_t offset)
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

const operators::legacy_plan::generated::a1_exact_patch_plan *
find_mask39_plan()
{
    using namespace operators::legacy_plan::generated;

    for (const auto &plan : k_a1_exact_patch_plans_v1)
        if (plan.legacy_mask == 39u)
            return &plan;

    return nullptr;
}

std::vector<std::uint8_t> make_synthetic_original(
    const operators::legacy_plan::generated::a1_exact_patch_plan &plan)
{
    using namespace operators::legacy_plan;
    using namespace operators::legacy_plan::generated;

    std::vector<std::uint8_t> bytes(plan.code_size, 0u);

    bytes[0] = 'D';
    bytes[1] = 'X';
    bytes[2] = 'B';
    bytes[3] = 'C';
    dxbc::write_u32(bytes.data() + 24u, plan.code_size);

    for (std::size_t i = 0; i < plan.op_count; ++i) {
        const auto &op =
            k_a1_exact_patch_ops_v1[plan.first_op + i];
        write_word(bytes, op.byte_offset, op.expected_old_word);
    }

    if (!dxbc::fix_checksum(bytes.data(), bytes.size()))
        return {};
    return bytes;
}

} // namespace

int main()
{
    using namespace operators::legacy_plan;
    using namespace operators::legacy_plan::generated;

    // The in-core SHA-256 implementation is used for exact shader identity.
    const std::uint8_t abc[] = {'a', 'b', 'c'};
    const auto abc_sha = hashing::sha256(abc, sizeof(abc));
    CHECK(hashing::matches_hex(
        abc_sha,
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad"));

    CHECK(k_a1_exact_patch_plan_count_v1 == 144u);
    CHECK(k_a1_exact_patch_op_count_v1 == 312u);

    const auto *plan = find_mask39_plan();
    CHECK(plan != nullptr);
    CHECK(plan->op_count == 7u);
    CHECK(a1_candidate_code_size(plan->code_size));

    auto original = make_synthetic_original(*plan);
    CHECK(dxbc::checksum_container_valid(
        original.data(),
        original.size()));

    core::feature_registry features;
    std::vector<std::uint8_t> replacement;

    // No feature gate: exact plan remains pass-through.
    auto out = materialize_verified_a1_plan(
        features,
        *plan,
        original.data(),
        original.size(),
        replacement);

    CHECK(
        out.result ==
        a1_create_time_result::pass_through_no_enabled_owner);
    CHECK(replacement.empty());

    // PntS-only activation patches exactly its two DWORDs and leaves every
    // other operator-local DWORD untouched.
    CHECK(features.set(
        core::operator_id::pointlight_pnts_attenuation,
        true));

    out = materialize_verified_a1_plan(
        features,
        *plan,
        original.data(),
        original.size(),
        replacement);

    CHECK(out.result == a1_create_time_result::applied);
    CHECK(
        out.selected_owners ==
        core::operator_bit(
            core::operator_id::pointlight_pnts_attenuation));
    CHECK(out.selected_ops == 2u);
    CHECK(!out.full_plan_materialized);
    CHECK(replacement.size() == original.size());

    std::size_t pnts_ops = 0;

    for (std::size_t i = 0; i < plan->op_count; ++i) {
        const auto &op =
            k_a1_exact_patch_ops_v1[plan->first_op + i];

        if (op.owner ==
            core::operator_id::pointlight_pnts_attenuation) {
            ++pnts_ops;
            CHECK(
                read_word(replacement, op.byte_offset) ==
                op.replacement_word);
        } else {
            CHECK(
                read_word(replacement, op.byte_offset) ==
                op.expected_old_word);
        }
    }

    CHECK(pnts_ops == 2u);
    CHECK(dxbc::checksum_container_valid(
        replacement.data(),
        replacement.size()));

    // Unexpected source words fail before an output replacement is exposed.
    auto corrupt = original;
    for (std::size_t i = 0; i < plan->op_count; ++i) {
        const auto &op =
            k_a1_exact_patch_ops_v1[plan->first_op + i];

        if (op.owner ==
            core::operator_id::pointlight_pnts_attenuation) {
            write_word(
                corrupt,
                op.byte_offset,
                op.expected_old_word ^ 0x100u);
            break;
        }
    }

    replacement.assign(16u, 0xABu);

    out = materialize_verified_a1_plan(
        features,
        *plan,
        corrupt.data(),
        corrupt.size(),
        replacement);

    CHECK(
        out.result ==
        a1_create_time_result::fail_open_token_mismatch);
    CHECK(replacement.empty());

    // A candidate-sized synthetic blob cannot enter the public path because
    // its complete SHA-256 is not one of the 144 exact certified identities.
    replacement.clear();

    out = materialize_a1_create_time(
        features,
        original.data(),
        original.size(),
        replacement);

    CHECK(
        out.result ==
        a1_create_time_result::pass_through_unknown_exact_sha);
    CHECK(replacement.empty());

    // Full-plan materialization on synthetic bytes proves the historical
    // replacement hash guard is live: byte ops + checksum are insufficient
    // unless the final exact SHA matches the certified P2.2 replacement.
    CHECK(features.set(
        core::operator_id::terminal_sat_rgb,
        true));
    CHECK(features.set(
        core::operator_id::diffuse_material_domain,
        true));
    CHECK(features.set(
        core::operator_id::envspec_nospc_delete,
        true));
    CHECK(features.set(
        core::operator_id::fixed_postfog_identity,
        true));

    out = materialize_verified_a1_plan(
        features,
        *plan,
        original.data(),
        original.size(),
        replacement);

    CHECK(out.selected_ops == plan->op_count);
    CHECK(out.full_plan_materialized);
    CHECK(
        out.result ==
        a1_create_time_result::
            fail_open_full_replacement_sha_mismatch);
    CHECK(replacement.empty());

    // Non-candidate sizes are rejected before hashing.
    std::vector<std::uint8_t> unrelated(64u, 0u);

    out = materialize_a1_create_time(
        features,
        unrelated.data(),
        unrelated.size(),
        replacement);

    CHECK(
        out.result ==
        a1_create_time_result::pass_through_not_candidate_size);
    CHECK(replacement.empty());

    std::cout << "a1_create_time_materializer_tests: PASS\n";
    return 0;
}
