#include "dsrrl/operators/legacy_plan/a1_exact_patch_recipes.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
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

} // namespace

int main()
{
    using namespace operators::legacy_plan;
    using namespace operators::legacy_plan::generated;

    CHECK(k_a1_exact_patch_plan_count_v1 == 144u);
    CHECK(k_a1_exact_patch_op_count_v1 == 312u);

    std::size_t terminal = 0;
    std::size_t diffuse = 0;
    std::size_t pnts = 0;
    std::size_t no_spc = 0;
    std::size_t postfog = 0;

    for (const auto &op : k_a1_exact_patch_ops_v1) {
        switch (op.owner) {
        case core::operator_id::terminal_sat_rgb:
            ++terminal;
            break;
        case core::operator_id::diffuse_material_domain:
            ++diffuse;
            break;
        case core::operator_id::pointlight_pnts_attenuation:
            ++pnts;
            break;
        case core::operator_id::envspec_nospc_delete:
            ++no_spc;
            break;
        case core::operator_id::fixed_postfog_identity:
            ++postfog;
            break;
        default:
            CHECK(false);
        }
    }

    CHECK(terminal == 144u);
    CHECK(diffuse == 108u);
    CHECK(pnts == 24u);
    CHECK(no_spc == 12u);
    CHECK(postfog == 24u);

    const a1_exact_patch_plan *pnts_plan = nullptr;
    for (const auto &plan : k_a1_exact_patch_plans_v1) {
        if (plan.legacy_mask == 39u) {
            pnts_plan = &plan;
            break;
        }
    }
    CHECK(pnts_plan != nullptr);

    std::vector<std::uint8_t> shader(pnts_plan->code_size, 0u);

    for (std::size_t i = 0; i < pnts_plan->op_count; ++i) {
        const auto &op =
            k_a1_exact_patch_ops_v1[pnts_plan->first_op + i];
        write_word(shader, op.byte_offset, op.expected_old_word);
    }

    auto result = apply_a1_exact_owner_patch(
        shader.data(),
        shader.size(),
        pnts_plan->original_sha256,
        core::operator_id::pointlight_pnts_attenuation);
    CHECK(result == exact_patch_result::applied);

    std::size_t pnts_seen = 0;
    for (std::size_t i = 0; i < pnts_plan->op_count; ++i) {
        const auto &op =
            k_a1_exact_patch_ops_v1[pnts_plan->first_op + i];

        if (op.owner == core::operator_id::pointlight_pnts_attenuation) {
            ++pnts_seen;
            CHECK(read_word(shader, op.byte_offset) == op.replacement_word);
        } else {
            CHECK(read_word(shader, op.byte_offset) == op.expected_old_word);
        }
    }
    CHECK(pnts_seen == 2u);

    result = apply_a1_exact_owner_patch(
        shader.data(),
        shader.size(),
        pnts_plan->original_sha256,
        core::operator_id::pointlight_pnts_attenuation);
    CHECK(result == exact_patch_result::already_applied);

    // Mixed state must fail before the remaining old word is modified.
    for (std::size_t i = 0; i < pnts_plan->op_count; ++i) {
        const auto &op =
            k_a1_exact_patch_ops_v1[pnts_plan->first_op + i];

        if (op.owner == core::operator_id::pointlight_pnts_attenuation)
            write_word(shader, op.byte_offset, op.expected_old_word);
    }

    const a1_exact_patch_op *first_pnts = nullptr;
    const a1_exact_patch_op *second_pnts = nullptr;

    for (std::size_t i = 0; i < pnts_plan->op_count; ++i) {
        const auto &op =
            k_a1_exact_patch_ops_v1[pnts_plan->first_op + i];

        if (op.owner != core::operator_id::pointlight_pnts_attenuation)
            continue;

        if (first_pnts == nullptr)
            first_pnts = &op;
        else {
            second_pnts = &op;
            break;
        }
    }

    CHECK(first_pnts != nullptr);
    CHECK(second_pnts != nullptr);

    write_word(
        shader,
        first_pnts->byte_offset,
        first_pnts->replacement_word);

    const auto second_before =
        read_word(shader, second_pnts->byte_offset);

    result = apply_a1_exact_owner_patch(
        shader.data(),
        shader.size(),
        pnts_plan->original_sha256,
        core::operator_id::pointlight_pnts_attenuation);
    CHECK(result == exact_patch_result::fail_open_mixed_state);
    CHECK(read_word(shader, second_pnts->byte_offset) == second_before);

    result = apply_a1_exact_owner_patch(
        shader.data(),
        shader.size() - 4u,
        pnts_plan->original_sha256,
        core::operator_id::pointlight_pnts_attenuation);
    CHECK(result == exact_patch_result::fail_open_code_size_mismatch);

    result = apply_a1_exact_owner_patch(
        shader.data(),
        shader.size(),
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        core::operator_id::pointlight_pnts_attenuation);
    CHECK(result == exact_patch_result::fail_open_unknown_shader);

    // A plan without PntS ops must fail open for that owner.
    const a1_exact_patch_plan *fixed_plan = nullptr;
    for (const auto &plan : k_a1_exact_patch_plans_v1) {
        if (plan.legacy_mask == 8u) {
            fixed_plan = &plan;
            break;
        }
    }
    CHECK(fixed_plan != nullptr);

    std::vector<std::uint8_t> fixed_shader(fixed_plan->code_size, 0u);
    result = apply_a1_exact_owner_patch(
        fixed_shader.data(),
        fixed_shader.size(),
        fixed_plan->original_sha256,
        core::operator_id::pointlight_pnts_attenuation);
    CHECK(result == exact_patch_result::fail_open_no_ops_for_owner);

    std::cout << "a1_exact_recipe_tests: PASS\n";
    return 0;
}
