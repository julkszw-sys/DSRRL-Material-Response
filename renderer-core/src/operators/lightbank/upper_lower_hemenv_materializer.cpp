#include "dsrrl/operators/lightbank/upper_lower_hemenv_materializer.hpp"
#include "dsrrl/operators/lightbank/generated_upper_lower_hemenv_v1.hpp"
#include "dsrrl/operators/legacy_plan/a1_create_time_materializer.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_rdef_patch.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>

namespace dsrrl::operators::lightbank {
namespace {

using legacy_plan::dxbc::read_u32;
using legacy_plan::dxbc::write_u32;
namespace hashing = legacy_plan::hashing;
namespace generated_ul = generated;

constexpr std::array<std::uint32_t,4> k_cb13_decl = {
    0x04000059u,
    0x00208e46u,
    0x0000000du,
    0x00000008u
};

struct chunk {
    std::array<char,4> tag{};
    std::vector<std::uint8_t> payload;
};

bool candidate_size(std::size_t size) noexcept
{
    for (const auto &plan :
         generated_ul::k_upper_lower_hemenv_plans)
        if (plan.stock_size == size)
            return true;

    return false;
}

const generated_ul::upper_lower_hemenv_plan *
find_plan(
    const std::uint8_t *source,
    std::size_t size) noexcept
{
    const auto digest =
        hashing::sha256(source, size);

    const generated_ul::upper_lower_hemenv_plan *hit =
        nullptr;

    for (const auto &plan :
         generated_ul::k_upper_lower_hemenv_plans) {
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

bool parse_dxbc(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<chunk> &chunks,
    std::size_t &code_index) noexcept
{
    if (!legacy_plan::dxbc::checksum_container_valid(
            source,
            size) ||
        size < 32u)
        return false;

    const std::uint32_t count =
        read_u32(source + 28u);

    if (count == 0u ||
        count > 64u ||
        32ull + 4ull * count > size)
        return false;

    chunks.clear();
    chunks.reserve(count);
    code_index =
        static_cast<std::size_t>(-1);

    for (std::uint32_t i = 0u;
         i < count;
         ++i) {
        const std::uint32_t offset =
            read_u32(
                source + 32u + i * 4u);

        if (offset > size ||
            size - offset < 8u)
            return false;

        const std::uint32_t payload_size =
            read_u32(
                source + offset + 4u);

        if (payload_size >
            size - offset - 8u)
            return false;

        chunk current{};
        std::memcpy(
            current.tag.data(),
            source + offset,
            4u);

        try {
            current.payload.assign(
                source + offset + 8u,
                source + offset + 8u +
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

            code_index = chunks.size();
        }

        chunks.push_back(
            std::move(current));
    }

    return code_index !=
        static_cast<std::size_t>(-1);
}

bool extract_words(
    const std::vector<chunk> &chunks,
    std::size_t code_index,
    std::vector<std::uint32_t> &words) noexcept
{
    if (code_index >= chunks.size())
        return false;

    const auto &payload =
        chunks[code_index].payload;

    if ((payload.size() & 3u) != 0u)
        return false;

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

    return true;
}

bool rebuild(
    const std::uint8_t *source,
    std::size_t source_size,
    std::vector<chunk> chunks,
    std::size_t code_index,
    const std::vector<std::uint32_t> &words,
    std::vector<std::uint8_t> &out) noexcept
{
    if (code_index >= chunks.size())
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
                code.data() + i * 4u,
                words[i]);

        const std::size_t header_size =
            32u + 4u * chunks.size();

        if (source_size < header_size ||
            header_size >
                std::numeric_limits<
                    std::uint32_t>::max())
            return false;

        out.assign(
            source,
            source + header_size);

        std::vector<std::uint32_t> offsets;
        offsets.reserve(chunks.size());

        for (const auto &current : chunks) {
            if (out.size() >
                std::numeric_limits<
                    std::uint32_t>::max())
                return false;

            offsets.push_back(
                static_cast<std::uint32_t>(
                    out.size()));

            out.insert(
                out.end(),
                reinterpret_cast<
                    const std::uint8_t *>(
                        current.tag.data()),
                reinterpret_cast<
                    const std::uint8_t *>(
                        current.tag.data()) +
                    4u);

            const auto size_offset =
                out.size();
            out.resize(
                size_offset + 4u);

            write_u32(
                out.data() + size_offset,
                static_cast<std::uint32_t>(
                    current.payload.size()));

            out.insert(
                out.end(),
                current.payload.begin(),
                current.payload.end());
        }

        if (out.size() >
            std::numeric_limits<
                std::uint32_t>::max())
            return false;

        write_u32(
            out.data() + 24u,
            static_cast<std::uint32_t>(
                out.size()));
        write_u32(
            out.data() + 28u,
            static_cast<std::uint32_t>(
                chunks.size()));

        for (std::size_t i = 0u;
             i < offsets.size();
             ++i)
            write_u32(
                out.data() + 32u +
                    i * 4u,
                offsets[i]);

        std::fill(
            out.begin() + 4u,
            out.begin() + 20u,
            std::uint8_t{0});

        return
            legacy_plan::dxbc::fix_checksum(
                out.data(),
                out.size());
    } catch (...) {
        out.clear();
        return false;
    }
}

std::size_t code_payload_offset(
    const std::uint8_t *source,
    std::size_t size) noexcept
{
    if (source == nullptr ||
        size < 32u)
        return
            static_cast<std::size_t>(-1);

    const std::uint32_t count =
        read_u32(source + 28u);

    if (count == 0u ||
        count > 64u ||
        32ull + 4ull * count > size)
        return
            static_cast<std::size_t>(-1);

    std::size_t hit =
        static_cast<std::size_t>(-1);

    for (std::uint32_t i = 0u;
         i < count;
         ++i) {
        const std::uint32_t off =
            read_u32(
                source + 32u +
                i * 4u);

        if (off > size ||
            size - off < 8u)
            return
                static_cast<std::size_t>(-1);

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
            static_cast<std::size_t>(-1))
            return
                static_cast<std::size_t>(-1);

        hit =
            static_cast<std::size_t>(
                off) + 8u;
    }

    return hit;
}

bool compose_enabled_a1_islands(
    const core::feature_registry &features,
    const std::uint8_t *stock,
    std::size_t stock_size,
    std::vector<std::uint8_t> &replacement,
    core::operator_mask &composed_owners) noexcept
{
    composed_owners = 0u;

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

    const std::size_t stock_code =
        code_payload_offset(
            stock,
            stock_size);
    const std::size_t replacement_code =
        code_payload_offset(
            replacement.data(),
            replacement.size());

    if (stock_code ==
            static_cast<std::size_t>(-1) ||
        replacement_code ==
            static_cast<std::size_t>(-1))
        return false;

    for (std::size_t i = 0u;
         i < plan->op_count;
         ++i) {
        const auto &op =
            legacy_plan::generated::
                k_a1_exact_patch_ops_v1[
                    plan->first_op + i];

        if (!features.enabled(op.owner))
            continue;

        if (op.byte_offset < stock_code ||
            ((op.byte_offset -
              stock_code) & 3u) != 0u)
            return false;

        std::size_t word =
            (op.byte_offset -
             stock_code) / 4u;

        // U/L inserts one b13 declaration at SHEX word 11.
        if (word >= 11u)
            word += 4u;

        const std::size_t target =
            replacement_code +
            word * 4u;

        if (target >
                replacement.size() ||
            replacement.size() - target <
                sizeof(std::uint32_t))
            return false;

        if (read_u32(
                replacement.data() +
                target) !=
            op.expected_old_word)
            return false;

        write_u32(
            replacement.data() + target,
            op.replacement_word);

        composed_owners |=
            core::operator_bit(op.owner);
    }

    if (composed_owners == 0u)
        return true;

    return
        legacy_plan::dxbc::fix_checksum(
            replacement.data(),
            replacement.size());
}

} // namespace

upper_lower_hemenv_materialize_outcome
materialize_upper_lower_hemenv_receiver(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept
{
    upper_lower_hemenv_materialize_outcome outcome{};
    output.clear();

    if (source == nullptr ||
        size == 0u) {
        outcome.result =
            upper_lower_hemenv_materialize_result::
                fail_invalid_dxbc;
        return outcome;
    }

    if (!candidate_size(size)) {
        outcome.result =
            upper_lower_hemenv_materialize_result::
                pass_not_candidate;
        return outcome;
    }

    if (!legacy_plan::dxbc::
            checksum_container_valid(
                source,
                size)) {
        outcome.result =
            upper_lower_hemenv_materialize_result::
                fail_invalid_dxbc;
        return outcome;
    }

    const auto *plan =
        find_plan(source, size);

    if (plan == nullptr) {
        outcome.result =
            upper_lower_hemenv_materialize_result::
                pass_unknown_exact_sha;
        return outcome;
    }

    outcome.plan_index =
        plan->plan_index;
    outcome.shader_index =
        plan->shader_index;
    outcome.stable_receiver_id =
        plan->stable_receiver_id;
    outcome.stratum =
        plan->stratum ==
            generated_ul::
                upper_lower_hemenv_stratum::spc
            ? upper_lower_hemenv_stratum::spc
            : upper_lower_hemenv_stratum::nospc;

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index = 0u;

    if (!parse_dxbc(
            source,
            size,
            chunks,
            code_index) ||
        !extract_words(
            chunks,
            code_index,
            words) ||
        words.size() < 11u ||
        words[1] != words.size()) {
        outcome.result =
            upper_lower_hemenv_materialize_result::
                fail_invalid_dxbc;
        return outcome;
    }

    // Exact retail ordinary HemEnv ABI. The Spc and no-Spc strata differ
    // only in the b0 declaration indexing mode token.
    if (words[2] != 0x0100086au ||
        (words[3] != 0x04000059u &&
         words[3] != 0x04000859u) ||
        words[4] != 0x00208e46u ||
        words[5] != 0u ||
        words[6] != 0x67u ||
        words[7] != 0x04000059u ||
        words[8] != 0x00208e46u ||
        words[9] != 1u ||
        words[10] != 1u) {
        outcome.result =
            upper_lower_hemenv_materialize_result::
                fail_operand_precondition;
        return outcome;
    }

    const std::array<
        std::pair<std::uint32_t,std::uint32_t>,
        3> patches{{
            {plan->u_slot_word, 7u},
            {plan->d_slot_word_0, 8u},
            {plan->d_slot_word_1, 8u}
        }};

    for (const auto &[slot_word, source_register] :
         patches) {
        if (slot_word + 1u >= words.size() ||
            words[slot_word] != 0u ||
            words[slot_word + 1u] !=
                source_register) {
            outcome.result =
                upper_lower_hemenv_materialize_result::
                    fail_operand_precondition;
            return outcome;
        }

        words[slot_word] = 13u;
        words[slot_word + 1u] =
            source_register == 7u
                ? 6u
                : 7u;
    }

    try {
        words.insert(
            words.begin() + 11,
            k_cb13_decl.begin(),
            k_cb13_decl.end());
    } catch (...) {
        outcome.result =
            upper_lower_hemenv_materialize_result::
                fail_rebuild;
        return outcome;
    }

    words[1] +=
        static_cast<std::uint32_t>(
            k_cb13_decl.size());

    chunk *rdef = nullptr;
    for (auto &current : chunks) {
        if (std::memcmp(
                current.tag.data(),
                "RDEF",
                4u) != 0)
            continue;

        if (rdef != nullptr) {
            outcome.result =
                upper_lower_hemenv_materialize_result::
                    fail_rebuild;
            return outcome;
        }

        rdef = &current;
    }

    if (rdef == nullptr ||
        !legacy_plan::dxbc::rdef::
            append_constant_buffer_binding(
                rdef->payload,
                "DSRRL_LightBankCarrier",
                13u,
                128u)) {
        outcome.result =
            upper_lower_hemenv_materialize_result::
                fail_rebuild;
        return outcome;
    }

    if (!rebuild(
            source,
            size,
            std::move(chunks),
            code_index,
            words,
            output)) {
        outcome.result =
            upper_lower_hemenv_materialize_result::
                fail_rebuild;
        return outcome;
    }

    if (output.size() !=
            plan->replacement_size ||
        !hashing::matches_hex(
            hashing::sha256(
                output.data(),
                output.size()),
            plan->replacement_sha256)) {
        output.clear();
        outcome.result =
            upper_lower_hemenv_materialize_result::
                fail_stage_sha;
        return outcome;
    }

    if (!compose_enabled_a1_islands(
            features,
            source,
            size,
            output,
            outcome.composed_owners)) {
        output.clear();
        outcome.result =
            upper_lower_hemenv_materialize_result::
                fail_a1_composition;
        return outcome;
    }

    outcome.result =
        upper_lower_hemenv_materialize_result::
            applied;
    return outcome;
}

} // namespace dsrrl::operators::lightbank
