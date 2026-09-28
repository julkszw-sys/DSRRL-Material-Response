#include "dsrrl/operators/material_response/material_response_diffuse_v1.hpp"

#include "dsrrl/operators/material_response/generated_diffuse_response_v1.hpp"
#include "dsrrl/operators/legacy_plan/a1_create_time_materializer.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_rdef_patch.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include "dsrrl/operators/surface/terminal_sat_rgb_patch.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace dsrrl::operators::material_response {
namespace {

namespace hashing = legacy_plan::hashing;
using legacy_plan::dxbc::read_u32;
using legacy_plan::dxbc::write_u32;

constexpr std::array<std::uint32_t,4> k_cb12_decl = {
    0x04000059u, 0x00208e46u, 0x0000000cu, 0x00000004u
};

struct chunk {
    std::array<char,4> tag{};
    std::vector<std::uint8_t> payload;
};

bool parse_dxbc(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<chunk> &chunks,
    std::size_t &code_index,
    std::vector<std::uint32_t> &words) noexcept
{
    if (source == nullptr ||
        size < 32u ||
        !legacy_plan::dxbc::checksum_container_valid(
            source,
            size))
        return false;

    const auto count =
        read_u32(source + 28u);
    if (count == 0u ||
        count > 64u ||
        32ull + 4ull * count > size)
        return false;

    chunks.clear();
    words.clear();
    code_index =
        static_cast<std::size_t>(-1);

    try {
        chunks.reserve(count);
    } catch (...) {
        return false;
    }

    for (std::uint32_t i = 0u;
         i < count;
         ++i) {
        const auto off =
            read_u32(
                source + 32u +
                i * 4u);

        if (off > size ||
            size - off < 8u)
            return false;

        const auto payload_size =
            read_u32(source + off + 4u);
        if (payload_size >
            size - off - 8u)
            return false;

        chunk current{};
        std::memcpy(
            current.tag.data(),
            source + off,
            4u);
        try {
            current.payload.assign(
                source + off + 8u,
                source + off + 8u +
                    payload_size);
        } catch (...) {
            return false;
        }

        const bool code =
            std::memcmp(
                current.tag.data(),
                "SHEX",
                4u) == 0 ||
            std::memcmp(
                current.tag.data(),
                "SHDR",
                4u) == 0;

        if (code) {
            if (code_index !=
                static_cast<std::size_t>(-1) ||
                (current.payload.size() &
                 3u) != 0u)
                return false;
            code_index =
                chunks.size();
        }

        chunks.push_back(
            std::move(current));
    }

    if (code_index ==
        static_cast<std::size_t>(-1))
        return false;

    const auto &payload =
        chunks[code_index].payload;

    try {
        words.resize(
            payload.size() / 4u);
    } catch (...) {
        return false;
    }

    for (std::size_t i = 0u;
         i < words.size();
         ++i)
        words[i] =
            read_u32(
                payload.data() +
                i * 4u);

    return
        words.size() >= 12u &&
        words[1] == words.size();
}

bool rebuild_dxbc(
    const std::uint8_t *source,
    std::size_t source_size,
    std::vector<chunk> chunks,
    std::size_t code_index,
    const std::vector<std::uint32_t> &words,
    std::vector<std::uint8_t> &output,
    bool defer_surface_operators) noexcept
{
    output.clear();

    if (source == nullptr ||
        code_index >= chunks.size())
        return false;

    try {
        auto &code =
            chunks[code_index].payload;
        code.resize(
            words.size() * 4u);

        for (std::size_t i = 0u;
             i < words.size();
             ++i)
            write_u32(
                code.data() +
                    i * 4u,
                words[i]);

        const auto header_size =
            32u + 4u * chunks.size();

        if (source_size < header_size ||
            header_size >
                std::numeric_limits<
                    std::uint32_t>::max())
            return false;

        output.assign(
            source,
            source + header_size);

        std::vector<std::uint32_t>
            offsets;
        offsets.reserve(
            chunks.size());

        for (const auto &current :
             chunks) {
            if (output.size() >
                std::numeric_limits<
                    std::uint32_t>::max())
                return false;

            offsets.push_back(
                static_cast<
                    std::uint32_t>(
                    output.size()));

            output.insert(
                output.end(),
                reinterpret_cast<
                    const std::uint8_t *>(
                    current.tag.data()),
                reinterpret_cast<
                    const std::uint8_t *>(
                    current.tag.data()) +
                    4u);

            const auto size_offset =
                output.size();
            output.resize(
                size_offset + 4u);
            write_u32(
                output.data() +
                    size_offset,
                static_cast<
                    std::uint32_t>(
                    current.payload.size()));

            output.insert(
                output.end(),
                current.payload.begin(),
                current.payload.end());
        }

        if (output.size() >
            std::numeric_limits<
                std::uint32_t>::max())
            return false;

        write_u32(
            output.data() + 24u,
            static_cast<
                std::uint32_t>(
                output.size()));
        write_u32(
            output.data() + 28u,
            static_cast<
                std::uint32_t>(
                chunks.size()));

        for (std::size_t i = 0u;
             i < offsets.size();
             ++i)
            write_u32(
                output.data() +
                    32u + i * 4u,
                offsets[i]);

        std::fill(
            output.begin() + 4u,
            output.begin() + 20u,
            std::uint8_t{0});

        return
            legacy_plan::dxbc::
                fix_checksum(
                    output.data(),
                    output.size());
    } catch (...) {
        output.clear();
        return false;
    }
}

const generated_diffuse_v1::plan *
find_plan(
    const std::uint8_t *source,
    std::size_t size) noexcept
{
    bool candidate_size = false;
    for (const auto &plan :
         generated_diffuse_v1::k_plans)
        if (plan.stock_size == size) {
            candidate_size = true;
            break;
        }

    if (!candidate_size)
        return nullptr;

    const auto digest =
        hashing::sha256(
            source,
            size);

    const generated_diffuse_v1::plan
        *hit = nullptr;

    for (const auto &plan :
         generated_diffuse_v1::k_plans) {
        if (plan.stock_size != size ||
            !hashing::matches_hex(
                digest,
                plan.stock_sha256))
            continue;

        if (hit != nullptr)
            return nullptr;

        hit = &plan;
    }

    return hit;
}

bool candidate_size(
    std::size_t size) noexcept
{
    for (const auto &plan :
         generated_diffuse_v1::k_plans)
        if (plan.stock_size == size)
            return true;

    return false;
}

std::size_t code_payload_offset(
    const std::uint8_t *source,
    std::size_t size) noexcept
{
    if (source == nullptr ||
        size < 32u)
        return
            static_cast<
                std::size_t>(-1);

    const auto count =
        read_u32(source + 28u);
    if (count == 0u ||
        count > 64u ||
        32ull + 4ull * count > size)
        return
            static_cast<
                std::size_t>(-1);

    std::size_t hit =
        static_cast<
            std::size_t>(-1);

    for (std::uint32_t i = 0u;
         i < count;
         ++i) {
        const auto off =
            read_u32(
                source + 32u +
                i * 4u);

        if (off > size ||
            size - off < 8u)
            return
                static_cast<
                    std::size_t>(-1);

        const bool code =
            std::memcmp(
                source + off,
                "SHEX",
                4u) == 0 ||
            std::memcmp(
                source + off,
                "SHDR",
                4u) == 0;

        if (!code)
            continue;

        if (hit !=
            static_cast<
                std::size_t>(-1))
            return
                static_cast<
                    std::size_t>(-1);

        hit =
            static_cast<
                std::size_t>(off) +
            8u;
    }

    return hit;
}

bool compose_stable_surface_islands(
    const core::feature_registry &features,
    const std::uint8_t *stock,
    std::size_t stock_size,
    std::vector<std::uint8_t> &output,
    core::operator_mask &owners) noexcept
{
    owners = 0u;

    const auto digest =
        hashing::sha256(
            stock,
            stock_size);

    const auto *plan =
        legacy_plan::
            find_a1_plan_by_exact_digest(
                stock_size,
                digest);

    if (plan == nullptr)
        return true;

    const auto stock_code =
        code_payload_offset(
            stock,
            stock_size);
    const auto output_code =
        code_payload_offset(
            output.data(),
            output.size());

    if (stock_code ==
            static_cast<
                std::size_t>(-1) ||
        output_code ==
            static_cast<
                std::size_t>(-1))
        return false;

    for (std::size_t i = 0u;
         i < plan->op_count;
         ++i) {
        const auto &op =
            legacy_plan::generated::
                k_a1_exact_patch_ops_v1[
                    plan->first_op + i];

        // This operator is already owned by this materializer.
        if (op.owner ==
            core::operator_id::
                diffuse_material_domain)
            continue;

        if (!features.enabled(
                op.owner))
            continue;

        if (op.byte_offset <
                stock_code ||
            ((op.byte_offset -
              stock_code) &
             3u) != 0u)
            return false;

        std::size_t word =
            (op.byte_offset -
             stock_code) / 4u;

        // This materializer inserts dcl_constantbuffer b12 at SHEX word 11.
        if (word >= 11u)
            word += 4u;

        const auto target =
            output_code +
            word * 4u;

        if (target > output.size() ||
            output.size() - target <
                4u)
            return false;

        if (read_u32(
                output.data() +
                target) !=
            op.expected_old_word)
            return false;

        write_u32(
            output.data() +
                target,
            op.replacement_word);

        owners |=
            core::operator_bit(
                op.owner);
    }

    return
        legacy_plan::dxbc::
            fix_checksum(
                output.data(),
                output.size());
}

bool finalize_b12_rdef_and_lerp_surface(
    const core::feature_registry &features,
    bool lerp,
    std::vector<std::uint8_t> &output,
    core::operator_mask &owners) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index = 0u;

    if (!parse_dxbc(
            output.data(),
            output.size(),
            chunks,
            code_index,
            words))
        return false;

    chunk *rdef = nullptr;

    for (auto &current :
         chunks) {
        if (std::memcmp(
                current.tag.data(),
                "RDEF",
                4u) != 0)
            continue;

        if (rdef != nullptr)
            return false;

        rdef = &current;
    }

    if (rdef == nullptr)
        return false;

    if (!legacy_plan::dxbc::rdef::
            has_constant_buffer_binding(
                rdef->payload,
                12u) &&
        !legacy_plan::dxbc::rdef::
            append_constant_buffer_binding(
                rdef->payload,
                "DSRRL_MaterialCarrier",
                12u,
                64u))
        return false;

    if (!legacy_plan::dxbc::rdef::
            has_constant_buffer_binding(
                rdef->payload,
                12u))
        return false;

    if (lerp &&
        features.enabled(
            core::operator_id::
                terminal_sat_rgb)) {
        const auto sat =
            operators::surface::
                apply_unique_terminal_rgb_sat_words(
                    words);

        using sat_result =
            operators::surface::
                terminal_sat_patch_result;

        if (sat !=
                sat_result::applied &&
            sat !=
                sat_result::
                    already_saturated)
            return false;

        owners |=
            core::operator_bit(
                core::operator_id::
                    terminal_sat_rgb);
    }

    std::vector<std::uint8_t>
        rebuilt;

    if (!rebuild_dxbc(
            output.data(),
            output.size(),
            std::move(chunks),
            code_index,
            words,
            rebuilt))
        return false;

    output =
        std::move(rebuilt);

    return
        legacy_plan::dxbc::
            checksum_container_valid(
                output.data(),
                output.size());
}

} // namespace

diffuse_v1_outcome
materialize_ptde_diffuse_response_v1(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept
{
    diffuse_v1_outcome outcome{};
    output.clear();

    if (source == nullptr ||
        size == 0u) {
        outcome.result =
            diffuse_v1_result::
                fail_invalid_dxbc;
        return outcome;
    }

    if (!candidate_size(size)) {
        outcome.result =
            diffuse_v1_result::
                pass_not_candidate;
        return outcome;
    }

    if (!legacy_plan::dxbc::
            checksum_container_valid(
                source,
                size)) {
        outcome.result =
            diffuse_v1_result::
                fail_invalid_dxbc;
        return outcome;
    }

    const auto *plan =
        find_plan(
            source,
            size);

    if (plan == nullptr) {
        outcome.result =
            diffuse_v1_result::
                pass_unknown_exact_sha;
        return outcome;
    }

    outcome.receiver_id =
        plan->receiver_id;
    outcome.family_index =
        plan->family_index;

    const bool lerp =
        plan->family ==
            generated_diffuse_v1::
                receiver_family::
                    hemenvlerp;

    outcome.family =
        lerp
            ? diffuse_v1_family::
                  hemenvlerp
            : diffuse_v1_family::
                  stable_hemenv;

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index = 0u;

    if (!parse_dxbc(
            source,
            size,
            chunks,
            code_index,
            words)) {
        outcome.result =
            diffuse_v1_result::
                fail_invalid_dxbc;
        return outcome;
    }

    for (const auto site :
         plan->c100_sites) {
        if (site + 1u >=
                words.size() ||
            words[site] != 0u ||
            words[site + 1u] !=
                9u) {
            outcome.result =
                diffuse_v1_result::
                    fail_patch_precondition;
            return outcome;
        }
    }

    if (plan->diffuse_pow_site +
            2u >=
            words.size() ||
        words[
            plan->diffuse_pow_site] !=
            0x400ccccdu ||
        words[
            plan->diffuse_pow_site +
            1u] !=
            0x400ccccdu ||
        words[
            plan->diffuse_pow_site +
            2u] !=
            0x400ccccdu) {
        outcome.result =
            diffuse_v1_result::
                fail_patch_precondition;
        return outcome;
    }

    // The only Material Response semantic mutation in this generic bridge:
    // PTDE c100 becomes the material factor and the DSR diffuse-only ^2.2
    // domain transform is removed.
    for (const auto site :
         plan->c100_sites) {
        words[site] = 12u;
        words[site + 1u] = 1u;
    }

    words[
        plan->diffuse_pow_site] =
        0x3f800000u;
    words[
        plan->diffuse_pow_site + 1u] =
        0x3f800000u;
    words[
        plan->diffuse_pow_site + 2u] =
        0x3f800000u;

    try {
        words.insert(
            words.begin() + 11,
            k_cb12_decl.begin(),
            k_cb12_decl.end());
    } catch (...) {
        outcome.result =
            diffuse_v1_result::
                fail_rebuild;
        return outcome;
    }

    words[1] += 4u;

    if (!rebuild_dxbc(
            source,
            size,
            std::move(chunks),
            code_index,
            words,
            output)) {
        outcome.result =
            diffuse_v1_result::
                fail_rebuild;
        return outcome;
    }

    if (!defer_surface_operators &&
        !lerp &&
        !compose_stable_surface_islands(
            features,
            source,
            size,
            output,
            outcome.composed_owners)) {
        output.clear();
        outcome.result =
            diffuse_v1_result::
                fail_patch_precondition;
        return outcome;
    }

    if (!finalize_b12_rdef_and_lerp_surface(
            features,
            lerp && !defer_surface_operators,
            output,
            outcome.composed_owners)) {
        output.clear();
        outcome.result =
            diffuse_v1_result::
                fail_patch_precondition;
        return outcome;
    }

    outcome.result =
        diffuse_v1_result::applied;
    return outcome;
}

} // namespace dsrrl::operators::material_response
