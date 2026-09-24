#include "dsrrl/operators/legacy_plan/a1_exact_island_composer.hpp"

#include <array>
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

bool checksum_nonzero(const std::vector<std::uint8_t> &bytes)
{
    for (std::size_t i = 4u; i < 20u; ++i)
        if (bytes[i] != 0u)
            return true;
    return false;
}

const operators::legacy_plan::generated::a1_exact_patch_plan *find_mask(
    std::uint32_t mask)
{
    using namespace operators::legacy_plan::generated;

    for (const auto &plan : k_a1_exact_patch_plans_v1)
        if (plan.legacy_mask == mask)
            return &plan;

    return nullptr;
}

std::vector<std::uint8_t> make_original_shader(
    const operators::legacy_plan::generated::a1_exact_patch_plan &plan)
{
    using namespace operators::legacy_plan::generated;

    std::vector<std::uint8_t> bytes(plan.code_size, 0u);
    bytes[0] = 'D';
    bytes[1] = 'X';
    bytes[2] = 'B';
    bytes[3] = 'C';
    operators::legacy_plan::dxbc::write_u32(
        bytes.data() + 24u,
        plan.code_size);

    for (std::size_t i = 0; i < plan.op_count; ++i) {
        const auto &op = k_a1_exact_patch_ops_v1[plan.first_op + i];
        write_word(bytes, op.byte_offset, op.expected_old_word);
    }

    return bytes;
}

std::size_t owner_op_count(
    const operators::legacy_plan::generated::a1_exact_patch_plan &plan,
    core::operator_id owner)
{
    using namespace operators::legacy_plan::generated;

    std::size_t count = 0;
    for (std::size_t i = 0; i < plan.op_count; ++i)
        if (k_a1_exact_patch_ops_v1[plan.first_op + i].owner == owner)
            ++count;
    return count;
}

} // namespace

int main()
{
    using namespace operators::legacy_plan;
    using namespace operators::legacy_plan::generated;

    const auto *multi = find_mask(39u);
    CHECK(multi != nullptr);

    auto shader = make_original_shader(*multi);
    CHECK(dxbc::checksum_container_valid(shader.data(), shader.size()));
    CHECK(!checksum_nonzero(shader));

    const auto pnts_bit =
        core::operator_bit(core::operator_id::pointlight_pnts_attenuation);
    const auto diffuse_bit =
        core::operator_bit(core::operator_id::diffuse_material_domain);
    const auto terminal_bit =
        core::operator_bit(core::operator_id::terminal_sat_rgb);
    const auto no_spc_bit =
        core::operator_bit(core::operator_id::envspec_nospc_delete);

    CHECK(owner_op_count(
        *multi,
        core::operator_id::pointlight_pnts_attenuation) == 2u);

    auto report = compose_a1_exact_islands(
        shader.data(),
        shader.size(),
        multi->original_sha256,
        pnts_bit);

    CHECK(report.result == a1_compose_result::applied);
    CHECK(report.requested_owner_mask == pnts_bit);
    CHECK(report.applied_owner_mask == pnts_bit);
    CHECK(report.requested_op_count == 2u);
    CHECK(report.applied_op_count == 2u);
    CHECK(checksum_nonzero(shader));

    std::array<std::uint8_t, 16> checksum_after_pnts{};
    std::memcpy(
        checksum_after_pnts.data(),
        shader.data() + 4u,
        checksum_after_pnts.size());

    // Idempotent when the same island set is requested.
    report = compose_a1_exact_islands(
        shader.data(),
        shader.size(),
        multi->original_sha256,
        pnts_bit);
    CHECK(report.result == a1_compose_result::already_applied);
    CHECK(report.applied_owner_mask == 0u);
    CHECK(std::memcmp(
        checksum_after_pnts.data(),
        shader.data() + 4u,
        checksum_after_pnts.size()) == 0);

    // Convergent composition is allowed: one requested owner may already be
    // fully new while another requested owner is still fully old.
    report = compose_a1_exact_islands(
        shader.data(),
        shader.size(),
        multi->original_sha256,
        pnts_bit | diffuse_bit);
    CHECK(report.result == a1_compose_result::applied);
    CHECK(report.requested_owner_mask == (pnts_bit | diffuse_bit));
    CHECK(report.applied_owner_mask == diffuse_bit);
    CHECK(report.applied_op_count ==
          owner_op_count(*multi, core::operator_id::diffuse_material_domain));

    // But a previously active island may not be silently inherited when it is
    // no longer requested.
    report = compose_a1_exact_islands(
        shader.data(),
        shader.size(),
        multi->original_sha256,
        diffuse_bit);
    CHECK(report.result ==
          a1_compose_result::fail_open_unrequested_patch_present);

    // Fresh body: request all four semantic owners present in mask 0x27.
    shader = make_original_shader(*multi);
    const auto all_present =
        pnts_bit | diffuse_bit | terminal_bit | no_spc_bit;

    report = compose_a1_exact_islands(
        shader.data(),
        shader.size(),
        multi->original_sha256,
        all_present);
    CHECK(report.result == a1_compose_result::applied);
    CHECK(report.requested_owner_mask == all_present);
    CHECK(report.applied_owner_mask == all_present);
    CHECK(report.applied_op_count == multi->op_count);

    for (std::size_t i = 0; i < multi->op_count; ++i) {
        const auto &op = k_a1_exact_patch_ops_v1[multi->first_op + i];
        CHECK(read_word(shader, op.byte_offset) == op.replacement_word);
    }

    // Internal partial state for one owner is rejected before any additional
    // requested operation is written.
    shader = make_original_shader(*multi);

    const a1_exact_patch_op *first_pnts = nullptr;
    const a1_exact_patch_op *second_pnts = nullptr;

    for (std::size_t i = 0; i < multi->op_count; ++i) {
        const auto &op = k_a1_exact_patch_ops_v1[multi->first_op + i];
        if (op.owner != core::operator_id::pointlight_pnts_attenuation)
            continue;

        if (first_pnts == nullptr)
            first_pnts = &op;
        else {
            second_pnts = &op;
            break;
        }
    }

    CHECK(first_pnts != nullptr && second_pnts != nullptr);
    write_word(
        shader,
        first_pnts->byte_offset,
        first_pnts->replacement_word);

    const auto second_before =
        read_word(shader, second_pnts->byte_offset);

    report = compose_a1_exact_islands(
        shader.data(),
        shader.size(),
        multi->original_sha256,
        pnts_bit);
    CHECK(report.result == a1_compose_result::fail_open_owner_mixed_state);
    CHECK(read_word(shader, second_pnts->byte_offset) == second_before);

    // Invalid identity/container paths stay fail-open.
    shader = make_original_shader(*multi);
    report = compose_a1_exact_islands(
        shader.data(),
        shader.size() - 4u,
        multi->original_sha256,
        pnts_bit);
    CHECK(report.result == a1_compose_result::fail_open_code_size_mismatch);

    report = compose_a1_exact_islands(
        shader.data(),
        shader.size(),
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        pnts_bit);
    CHECK(report.result == a1_compose_result::fail_open_unknown_shader);

    shader[0] = 'X';
    report = compose_a1_exact_islands(
        shader.data(),
        shader.size(),
        multi->original_sha256,
        pnts_bit);
    CHECK(report.result == a1_compose_result::fail_open_invalid_dxbc_container);

    // An operator not present in the selected plan is a no-op, not a reason to
    // activate some neighboring legacy bit.
    shader = make_original_shader(*multi);
    report = compose_a1_exact_islands(
        shader.data(),
        shader.size(),
        multi->original_sha256,
        core::operator_bit(core::operator_id::upper_lower));
    CHECK(report.result == a1_compose_result::no_requested_owner);
    CHECK(report.applied_op_count == 0u);

    // Direct checksum helper is stable after recomputation.
    const auto *fixed = find_mask(8u);
    CHECK(fixed != nullptr);
    auto checksum_shader = make_original_shader(*fixed);
    CHECK(dxbc::fix_checksum(checksum_shader.data(), checksum_shader.size()));

    std::array<std::uint8_t, 16> checksum_once{};
    std::memcpy(
        checksum_once.data(),
        checksum_shader.data() + 4u,
        checksum_once.size());

    CHECK(dxbc::fix_checksum(checksum_shader.data(), checksum_shader.size()));
    CHECK(std::memcmp(
        checksum_once.data(),
        checksum_shader.data() + 4u,
        checksum_once.size()) == 0);

    std::cout << "a1_island_composer_tests: PASS\n";
    return 0;
}
