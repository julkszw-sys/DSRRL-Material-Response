#include "dsrrl/operators/point_light/clustered_pnts_direct_materializer.hpp"

#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include "dsrrl/operators/point_light/generated_clustered_pnts_direct_v1.hpp"
#include "dsrrl/operators/lightbank/generated_upper_lower_phn_pnts_v1.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace dsrrl::operators::point_light {
namespace {

using legacy_plan::dxbc::read_u32;
using legacy_plan::dxbc::write_u32;
namespace hashing = legacy_plan::hashing;
namespace generated_ul_pnts =
    dsrrl::operators::lightbank::generated_pnts;

constexpr std::array<std::uint32_t,4> k_cb13_decl = {
    0x04000059u,
    0x00208e46u,
    13u,
    8u
};

struct chunk {
    std::array<char,4> tag{};
    std::vector<std::uint8_t> payload;
};

bool candidate_size(std::size_t size) noexcept
{
    for (const auto &plan : generated::k_clustered_pnts_plans_v1)
        if (plan.stock_size == size)
            return true;
    return false;
}

const generated::clustered_pnts_plan_v1 *find_plan(
    const std::uint8_t *source,
    std::size_t size,
    core::sha256_digest &digest) noexcept
{
    digest = hashing::sha256(source, size);
    const generated::clustered_pnts_plan_v1 *hit = nullptr;
    for (const auto &plan : generated::k_clustered_pnts_plans_v1) {
        if (plan.stock_size != size ||
            !hashing::matches_hex(digest, plan.original_sha256))
            continue;
        if (hit != nullptr)
            return nullptr;
        hit = &plan;
    }
    return hit;
}

const generated_ul_pnts::upper_lower_phn_pnts_plan *
find_upper_lower_pnts_plan(
    std::size_t size,
    const core::sha256_digest &digest) noexcept
{
    const generated_ul_pnts::upper_lower_phn_pnts_plan *hit = nullptr;
    for (const auto &plan :
         generated_ul_pnts::k_upper_lower_phn_pnts_plans) {
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

bool remap_stock_word_after_clustered_journal(
    const generated::clustered_pnts_plan_v1 &plan,
    std::uint32_t stock_word,
    std::size_t &mapped) noexcept
{
    std::int64_t shift = 0;

    for (std::uint32_t i = 0u;
         i < plan.op_count;
         ++i) {
        const auto &op =
            generated::k_clustered_pnts_journal_ops_v1[
                plan.first_op + i];

        // Any replacement/deletion range touching the U/L operand means the
        // two operator patches are not independently composable.
        if (op.old_count != 0u &&
            stock_word >= op.start &&
            stock_word < op.end)
            return false;

        // Insertions at exactly stock_word occur before that original word.
        const bool before =
            op.old_count == 0u
                ? op.start <= stock_word
                : op.end <= stock_word;
        if (!before)
            continue;

        shift +=
            static_cast<std::int64_t>(
                op.new_count) -
            static_cast<std::int64_t>(
                op.old_count);
    }

    const auto result =
        static_cast<std::int64_t>(
            stock_word) + shift;
    if (result < 0)
        return false;

    mapped = static_cast<std::size_t>(result);
    return true;
}

bool compose_upper_lower_pnts(
    std::vector<std::uint32_t> &words,
    const generated::clustered_pnts_plan_v1 &clustered,
    const generated_ul_pnts::upper_lower_phn_pnts_plan &ul) noexcept
{
    const bool ul_spc =
        ul.stratum ==
        generated_ul_pnts::
            upper_lower_phn_pnts_stratum::spc;

    if (ul_spc != clustered.spc ||
        ul.shader_index !=
            clustered.representative_shader_index ||
        words.size() < 15u ||
        words[11] != k_cb13_decl[0] ||
        words[12] != k_cb13_decl[1] ||
        words[13] != 12u ||
        words[14] != 4u)
        return false;

    const std::array<
        std::pair<std::uint32_t,std::uint32_t>,
        3> patches{{
            {ul.u_slot_word, 7u},
            {ul.d_slot_word_0, 8u},
            {ul.d_slot_word_1, 8u}
        }};

    for (const auto &[stock_slot, source_register] :
         patches) {
        std::size_t slot = 0u;
        std::size_t register_word = 0u;
        if (!remap_stock_word_after_clustered_journal(
                clustered,
                stock_slot,
                slot) ||
            !remap_stock_word_after_clustered_journal(
                clustered,
                stock_slot + 1u,
                register_word) ||
            register_word != slot + 1u ||
            register_word >= words.size() ||
            words[slot] != 0u ||
            words[register_word] != source_register)
            return false;

        // Exact PTDE U/L consumer cut:
        // b0[7] -> b13[6], b0[8] -> b13[7] twice.
        words[slot] = 13u;
        words[register_word] =
            source_register == 7u
                ? 6u
                : 7u;
    }

    // The clustered journal already inserted dcl_constantbuffer b12[4] at
    // words 11..14. Compose the independent U/L carrier declaration directly
    // after it. RDEF is stripped by the clustered island by design, so the
    // executable declaration is the complete runtime binding contract.
    try {
        words.insert(
            words.begin() + 15,
            k_cb13_decl.begin(),
            k_cb13_decl.end());
    } catch (...) {
        return false;
    }

    if (words.size() >
        std::numeric_limits<std::uint32_t>::max())
        return false;
    words[1] =
        static_cast<std::uint32_t>(
            words.size());
    return true;
}

bool parse_dxbc(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<chunk> &chunks,
    std::size_t &code_index,
    std::vector<std::uint32_t> &words) noexcept
{
    if (source == nullptr || size < 32u ||
        !legacy_plan::dxbc::checksum_container_valid(source, size))
        return false;

    const auto count = read_u32(source + 28u);
    if (count == 0u || count > 64u ||
        32ull + 4ull * count > size)
        return false;

    chunks.clear();
    code_index = static_cast<std::size_t>(-1);
    try {
        chunks.reserve(count);
    } catch (...) {
        return false;
    }

    for (std::uint32_t i = 0u; i < count; ++i) {
        const auto offset = static_cast<std::size_t>(
            read_u32(source + 32u + i * 4u));
        if (offset > size || size - offset < 8u)
            return false;

        const auto payload_size = static_cast<std::size_t>(
            read_u32(source + offset + 4u));
        if (payload_size > size - offset - 8u)
            return false;

        chunk current{};
        std::memcpy(current.tag.data(), source + offset, 4u);
        try {
            current.payload.assign(
                source + offset + 8u,
                source + offset + 8u + payload_size);
        } catch (...) {
            return false;
        }

        if (std::memcmp(current.tag.data(), "SHEX", 4u) == 0 ||
            std::memcmp(current.tag.data(), "SHDR", 4u) == 0) {
            if (code_index != static_cast<std::size_t>(-1) ||
                (payload_size & 3u) != 0u)
                return false;
            code_index = chunks.size();
        }
        chunks.push_back(std::move(current));
    }

    if (code_index == static_cast<std::size_t>(-1))
        return false;

    const auto &payload = chunks[code_index].payload;
    try {
        words.resize(payload.size() / 4u);
    } catch (...) {
        return false;
    }
    for (std::size_t i = 0u; i < words.size(); ++i)
        words[i] = read_u32(payload.data() + i * 4u);

    return words.size() >= 3u &&
           words[1] == static_cast<std::uint32_t>(words.size());
}

bool validate_plan(
    const std::vector<std::uint32_t> &words,
    const generated::clustered_pnts_plan_v1 &plan) noexcept
{
    if (plan.first_op > generated::k_clustered_pnts_journal_ops_v1.size() ||
        plan.op_count >
            generated::k_clustered_pnts_journal_ops_v1.size() - plan.first_op)
        return false;

    for (std::uint32_t i = 0u; i < plan.op_count; ++i) {
        const auto &op =
            generated::k_clustered_pnts_journal_ops_v1[
                plan.first_op + i];

        if (op.start > op.end ||
            op.end > words.size() ||
            op.old_count != op.end - op.start ||
            op.old_offset >
                generated::k_clustered_pnts_journal_tokens_v1.size() ||
            op.old_count >
                generated::k_clustered_pnts_journal_tokens_v1.size() -
                    op.old_offset ||
            op.new_offset >
                generated::k_clustered_pnts_journal_tokens_v1.size() ||
            op.new_count >
                generated::k_clustered_pnts_journal_tokens_v1.size() -
                    op.new_offset)
            return false;

        for (std::uint32_t n = 0u; n < op.old_count; ++n) {
            if (words[op.start + n] !=
                generated::k_clustered_pnts_journal_tokens_v1[
                    op.old_offset + n])
                return false;
        }
    }

    return true;
}

bool apply_plan(
    std::vector<std::uint32_t> &words,
    const generated::clustered_pnts_plan_v1 &plan) noexcept
{
    if (!validate_plan(words, plan))
        return false;

    try {
        for (std::uint32_t reverse = plan.op_count;
             reverse > 0u;
             --reverse) {
            const auto &op =
                generated::k_clustered_pnts_journal_ops_v1[
                    plan.first_op + reverse - 1u];

            auto first =
                words.begin() +
                static_cast<std::ptrdiff_t>(op.start);
            auto last =
                words.begin() +
                static_cast<std::ptrdiff_t>(op.end);

            first = words.erase(first, last);
            words.insert(
                first,
                generated::k_clustered_pnts_journal_tokens_v1.begin() +
                    static_cast<std::ptrdiff_t>(op.new_offset),
                generated::k_clustered_pnts_journal_tokens_v1.begin() +
                    static_cast<std::ptrdiff_t>(
                        op.new_offset + op.new_count));
        }
    } catch (...) {
        return false;
    }

    if (words.size() < 3u ||
        words.size() >
            std::numeric_limits<std::uint32_t>::max())
        return false;

    words[1] = static_cast<std::uint32_t>(words.size());
    return true;
}

bool strip_rdef(std::vector<chunk> &chunks) noexcept
{
    const auto before = chunks.size();
    chunks.erase(
        std::remove_if(
            chunks.begin(),
            chunks.end(),
            [](const chunk &current) {
                return std::memcmp(
                    current.tag.data(),
                    "RDEF",
                    4u) == 0;
            }),
        chunks.end());
    return chunks.size() + 1u == before;
}

bool rebuild(
    const std::uint8_t *basis,
    std::size_t basis_size,
    std::vector<chunk> chunks,
    const std::vector<std::uint32_t> &words,
    std::vector<std::uint8_t> &out) noexcept
{
    if (basis == nullptr || basis_size < 32u)
        return false;

    std::size_t code_index =
        static_cast<std::size_t>(-1);
    for (std::size_t i = 0u; i < chunks.size(); ++i) {
        if (std::memcmp(chunks[i].tag.data(), "SHEX", 4u) != 0 &&
            std::memcmp(chunks[i].tag.data(), "SHDR", 4u) != 0)
            continue;
        if (code_index != static_cast<std::size_t>(-1))
            return false;
        code_index = i;
    }
    if (code_index == static_cast<std::size_t>(-1))
        return false;

    try {
        auto &payload = chunks[code_index].payload;
        payload.resize(words.size() * 4u);
        for (std::size_t i = 0u; i < words.size(); ++i)
            write_u32(payload.data() + i * 4u, words[i]);

        const auto header_size =
            32u + 4u * chunks.size();
        if (header_size >
            std::numeric_limits<std::uint32_t>::max())
            return false;

        out.assign(
            basis,
            basis + std::min<std::size_t>(basis_size, 32u));
        out.resize(header_size, 0u);
        std::memcpy(out.data(), basis, 24u);
        write_u32(
            out.data() + 28u,
            static_cast<std::uint32_t>(chunks.size()));

        std::vector<std::uint32_t> offsets;
        offsets.reserve(chunks.size());
        for (const auto &current : chunks) {
            if (out.size() >
                std::numeric_limits<std::uint32_t>::max())
                return false;

            offsets.push_back(
                static_cast<std::uint32_t>(out.size()));
            out.insert(
                out.end(),
                reinterpret_cast<const std::uint8_t *>(
                    current.tag.data()),
                reinterpret_cast<const std::uint8_t *>(
                    current.tag.data()) + 4u);

            const auto size_at = out.size();
            out.resize(size_at + 4u);
            write_u32(
                out.data() + size_at,
                static_cast<std::uint32_t>(
                    current.payload.size()));
            out.insert(
                out.end(),
                current.payload.begin(),
                current.payload.end());
        }

        if (out.size() >
            std::numeric_limits<std::uint32_t>::max())
            return false;

        write_u32(
            out.data() + 24u,
            static_cast<std::uint32_t>(out.size()));
        for (std::size_t i = 0u; i < offsets.size(); ++i)
            write_u32(
                out.data() + 32u + i * 4u,
                offsets[i]);

        std::fill(
            out.begin() + 4u,
            out.begin() + 20u,
            std::uint8_t{0});

        return legacy_plan::dxbc::fix_checksum(
            out.data(),
            out.size());
    } catch (...) {
        out.clear();
        return false;
    }
}

} // namespace

clustered_pnts_direct_materialize_outcome
materialize_clustered_pnts_direct_ptde(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept
{
    clustered_pnts_direct_materialize_outcome outcome{};
    output.clear();

    if (source == nullptr || size == 0u) {
        outcome.result =
            clustered_pnts_direct_materialize_result::
                pass_not_candidate;
        return outcome;
    }

    if (!candidate_size(size)) {
        outcome.result =
            clustered_pnts_direct_materialize_result::
                pass_not_candidate;
        return outcome;
    }

    if (!legacy_plan::dxbc::checksum_container_valid(
            source,
            size)) {
        outcome.result =
            clustered_pnts_direct_materialize_result::
                fail_invalid_dxbc;
        return outcome;
    }

    core::sha256_digest host_digest{};
    const auto *plan =
        find_plan(
            source,
            size,
            host_digest);
    if (plan == nullptr) {
        outcome.result =
            clustered_pnts_direct_materialize_result::
                pass_unknown_exact_sha;
        return outcome;
    }

    outcome.host_sha256 = host_digest;
    outcome.host_size = size;
    outcome.representative_shader_index =
        plan->representative_shader_index;
    outcome.spc = plan->spc;
    outcome.blended_material =
        plan->blended_material;

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
            clustered_pnts_direct_materialize_result::
                fail_invalid_dxbc;
        return outcome;
    }

    if (!apply_plan(words, *plan)) {
        outcome.result =
            clustered_pnts_direct_materialize_result::
                fail_patch_precondition;
        return outcome;
    }

    // First certify the existing direct-PointLight replacement byte-for-byte.
    // U/L composition is allowed only on top of this already verified base.
    std::vector<std::uint8_t> pointlight_only;
    auto pointlight_chunks = chunks;
    if (!strip_rdef(pointlight_chunks) ||
        !rebuild(
            source,
            size,
            std::move(pointlight_chunks),
            words,
            pointlight_only)) {
        outcome.result =
            clustered_pnts_direct_materialize_result::
                fail_rebuild;
        return outcome;
    }

    const auto pointlight_digest =
        hashing::sha256(
            pointlight_only.data(),
            pointlight_only.size());
    if (pointlight_only.size() !=
            plan->replacement_size ||
        !hashing::matches_hex(
            pointlight_digest,
            plan->replacement_sha256)) {
        outcome.result =
            clustered_pnts_direct_materialize_result::
                fail_final_sha;
        return outcome;
    }

    // Every exact clustered PntS stock SHA has one exact U/L PntS consumer
    // authority. The two patch sets are required to be disjoint; stock-word
    // operands are deterministically remapped through the clustered journal.
    const auto *ul =
        find_upper_lower_pnts_plan(
            size,
            host_digest);
    if (ul == nullptr ||
        !compose_upper_lower_pnts(
            words,
            *plan,
            *ul)) {
        outcome.result =
            clustered_pnts_direct_materialize_result::
                fail_upper_lower_composition;
        return outcome;
    }

    if (!strip_rdef(chunks) ||
        !rebuild(
            source,
            size,
            std::move(chunks),
            words,
            output)) {
        output.clear();
        outcome.result =
            clustered_pnts_direct_materialize_result::
                fail_rebuild;
        return outcome;
    }

    if (output.size() !=
            plan->replacement_size +
                sizeof(k_cb13_decl) ||
        !legacy_plan::dxbc::
            checksum_container_valid(
                output.data(),
                output.size())) {
        output.clear();
        outcome.result =
            clustered_pnts_direct_materialize_result::
                fail_upper_lower_composition;
        return outcome;
    }

    outcome.replacement_sha256 =
        hashing::sha256(
            output.data(),
            output.size());
    outcome.replacement_size =
        output.size();
    outcome.upper_lower_composed = true;
    outcome.result =
        clustered_pnts_direct_materialize_result::
            applied;
    return outcome;
}

} // namespace dsrrl::operators::point_light
