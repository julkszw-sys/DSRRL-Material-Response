#include "dsrrl/operators/material_response/material_response_v211_materializer.hpp"
#include "dsrrl/operators/material_response/generated_v211_plans.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_rdef_patch.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include "dsrrl/operators/legacy_plan/a1_create_time_materializer.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace dsrrl::operators::material_response {
namespace {

using legacy_plan::dxbc::read_u32;
using legacy_plan::dxbc::write_u32;
namespace hashing = legacy_plan::hashing;

constexpr std::array<std::uint32_t,4> k_cb12_decl = {
    0x04000059u, 0x00208e46u, 0x0000000cu, 0x00000004u
};

struct chunk {
    std::array<char,4> tag{};
    std::vector<std::uint8_t> payload;
};

bool candidate_size(std::size_t size) noexcept
{
    for (const auto &plan : generated::k_v211_plans)
        if (plan.stock_size == size)
            return true;
    return false;
}

const generated::v211_plan *find_plan(
    const std::uint8_t *source,
    std::size_t size) noexcept
{
    const auto digest = hashing::sha256(source, size);
    const generated::v211_plan *hit = nullptr;

    for (const auto &plan : generated::k_v211_plans) {
        if (plan.stock_size != size ||
            !hashing::matches_hex(digest, plan.original_sha256))
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
    std::size_t &shex_index) noexcept
{
    if (!legacy_plan::dxbc::checksum_container_valid(source, size) ||
        size < 32u)
        return false;

    const std::uint32_t count = read_u32(source + 28u);
    if (count == 0u || count > 64u ||
        32ull + 4ull * count > size)
        return false;

    chunks.clear();
    chunks.reserve(count);
    shex_index = static_cast<std::size_t>(-1);

    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t offset =
            read_u32(source + 32u + i * 4u);

        if (offset > size || size - offset < 8u)
            return false;

        const std::uint32_t payload_size =
            read_u32(source + offset + 4u);

        if (payload_size > size - offset - 8u)
            return false;

        chunk c{};
        std::memcpy(c.tag.data(), source + offset, 4u);
        c.payload.assign(
            source + offset + 8u,
            source + offset + 8u + payload_size);

        const bool code =
            std::memcmp(c.tag.data(), "SHEX", 4u) == 0 ||
            std::memcmp(c.tag.data(), "SHDR", 4u) == 0;

        if (code) {
            if (shex_index != static_cast<std::size_t>(-1))
                return false;
            if ((c.payload.size() & 3u) != 0u)
                return false;
            shex_index = chunks.size();
        }

        chunks.push_back(std::move(c));
    }

    return shex_index != static_cast<std::size_t>(-1);
}

bool extract_words(
    const std::vector<chunk> &chunks,
    std::size_t shex_index,
    std::vector<std::uint32_t> &words) noexcept
{
    if (shex_index >= chunks.size())
        return false;

    const auto &payload = chunks[shex_index].payload;
    if ((payload.size() & 3u) != 0u)
        return false;

    try {
        words.resize(payload.size() / 4u);
    } catch (...) {
        return false;
    }

    for (std::size_t i = 0; i < words.size(); ++i)
        words[i] = read_u32(payload.data() + i * 4u);

    return true;
}

bool rebuild(
    const std::uint8_t *source,
    std::size_t source_size,
    std::vector<chunk> chunks,
    std::size_t shex_index,
    const std::vector<std::uint32_t> &words,
    std::vector<std::uint8_t> &out) noexcept
{
    if (shex_index >= chunks.size())
        return false;

    try {
        auto &code = chunks[shex_index].payload;
        code.resize(words.size() * 4u);
        for (std::size_t i = 0; i < words.size(); ++i)
            write_u32(code.data() + i * 4u, words[i]);

        const std::size_t header_size =
            32u + 4u * chunks.size();

        if (source_size < header_size ||
            header_size > std::numeric_limits<std::uint32_t>::max())
            return false;

        out.assign(source, source + header_size);

        std::vector<std::uint32_t> offsets;
        offsets.reserve(chunks.size());

        for (const auto &c : chunks) {
            if (out.size() >
                std::numeric_limits<std::uint32_t>::max())
                return false;

            offsets.push_back(
                static_cast<std::uint32_t>(out.size()));

            out.insert(
                out.end(),
                reinterpret_cast<const std::uint8_t *>(c.tag.data()),
                reinterpret_cast<const std::uint8_t *>(c.tag.data()) + 4u);

            const auto size_offset = out.size();
            out.resize(size_offset + 4u);
            write_u32(
                out.data() + size_offset,
                static_cast<std::uint32_t>(c.payload.size()));

            out.insert(
                out.end(),
                c.payload.begin(),
                c.payload.end());
        }

        if (out.size() >
            std::numeric_limits<std::uint32_t>::max())
            return false;

        write_u32(
            out.data() + 24u,
            static_cast<std::uint32_t>(out.size()));
        write_u32(
            out.data() + 28u,
            static_cast<std::uint32_t>(chunks.size()));

        for (std::size_t i = 0; i < offsets.size(); ++i)
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

bool hash_matches(
    const std::vector<std::uint8_t> &bytes,
    std::string_view expected) noexcept
{
    if (bytes.empty())
        return false;

    return hashing::matches_hex(
        hashing::sha256(bytes.data(), bytes.size()),
        expected);
}

bool parse_words(
    const std::vector<std::uint8_t> &bytes,
    std::vector<chunk> &chunks,
    std::size_t &shex_index,
    std::vector<std::uint32_t> &words) noexcept
{
    return parse_dxbc(
               bytes.data(),
               bytes.size(),
               chunks,
               shex_index) &&
           extract_words(
               chunks,
               shex_index,
               words);
}

std::size_t code_payload_offset(
    const std::uint8_t *source,
    std::size_t size) noexcept
{
    if (source == nullptr || size < 32u)
        return static_cast<std::size_t>(-1);

    const std::uint32_t count = read_u32(source + 28u);
    if (count == 0u || count > 64u ||
        32ull + 4ull * count > size)
        return static_cast<std::size_t>(-1);

    std::size_t hit = static_cast<std::size_t>(-1);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t off =
            read_u32(source + 32u + i * 4u);
        if (off > size || size - off < 8u)
            return static_cast<std::size_t>(-1);

        const bool code =
            std::memcmp(source + off, "SHEX", 4u) == 0 ||
            std::memcmp(source + off, "SHDR", 4u) == 0;
        if (!code)
            continue;
        if (hit != static_cast<std::size_t>(-1))
            return static_cast<std::size_t>(-1);
        hit = static_cast<std::size_t>(off) + 8u;
    }

    return hit;
}

bool augment_final_rdef_b12(
    std::vector<std::uint8_t> &bytes) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index = 0u;

    if (!parse_dxbc(
            bytes.data(),
            bytes.size(),
            chunks,
            code_index) ||
        !extract_words(
            chunks,
            code_index,
            words))
        return false;

    chunk *rdef = nullptr;
    for (auto &current : chunks) {
        if (std::memcmp(
                current.tag.data(),
                "RDEF",
                4u) != 0)
            continue;

        if (rdef != nullptr)
            return false;

        rdef = &current;
    }

    if (rdef == nullptr ||
        legacy_plan::dxbc::rdef::
            has_constant_buffer_binding(
                rdef->payload,
                12u))
        return false;

    if (!legacy_plan::dxbc::rdef::
            append_constant_buffer_binding(
                rdef->payload,
                "DSRRL_MaterialCarrier",
                12u,
                64u) ||
        !legacy_plan::dxbc::rdef::
            has_constant_buffer_binding(
                rdef->payload,
                12u))
        return false;

    std::vector<std::uint8_t> rebuilt;
    if (!rebuild(
            bytes.data(),
            bytes.size(),
            std::move(chunks),
            code_index,
            words,
            rebuilt))
        return false;

    bytes = std::move(rebuilt);
    return true;
}

bool compose_enabled_a1_islands(
    const core::feature_registry &features,
    const std::uint8_t *stock,
    std::size_t stock_size,
    std::vector<std::uint8_t> &v211,
    core::operator_mask &composed_owners) noexcept
{
    composed_owners = 0u;
    const auto digest = hashing::sha256(stock, stock_size);
    const auto *plan =
        legacy_plan::find_a1_plan_by_exact_digest(
            stock_size,
            digest);

    if (plan == nullptr)
        return true;

    const std::size_t stock_code =
        code_payload_offset(stock, stock_size);
    const std::size_t v211_code =
        code_payload_offset(v211.data(), v211.size());

    if (stock_code == static_cast<std::size_t>(-1) ||
        v211_code == static_cast<std::size_t>(-1))
        return false;

    for (std::size_t i = 0; i < plan->op_count; ++i) {
        const auto &op =
            legacy_plan::generated::k_a1_exact_patch_ops_v1[
                plan->first_op + i];

        // V2.11 already owns this operator locally through the exact
        // c100/domain transform. Applying the generic A1 patch again would
        // double-own the same semantic cut.
        if (op.owner == core::operator_id::diffuse_material_domain)
            continue;

        if (!features.enabled(op.owner))
            continue;

        if (op.byte_offset < stock_code ||
            ((op.byte_offset - stock_code) & 3u) != 0u)
            return false;

        std::size_t word =
            (op.byte_offset - stock_code) / 4u;

        // V29 inserts dcl_constantbuffer b12 at SHEX word 11.
        if (word >= 11u)
            word += 4u;

        const std::size_t target =
            v211_code + word * 4u;

        if (target > v211.size() ||
            v211.size() - target < 4u)
            return false;

        if (read_u32(v211.data() + target) !=
            op.expected_old_word)
            return false;

        write_u32(
            v211.data() + target,
            op.replacement_word);
        composed_owners |= core::operator_bit(op.owner);
    }

    return legacy_plan::dxbc::fix_checksum(
        v211.data(),
        v211.size());
}

} // namespace

v211_materialize_outcome materialize_v211_stable_receiver(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept
{
    v211_materialize_outcome outcome{};
    output.clear();

    if (source == nullptr || size == 0u) {
        outcome.result =
            v211_materialize_result::fail_invalid_dxbc;
        return outcome;
    }

    if (!candidate_size(size)) {
        outcome.result =
            v211_materialize_result::pass_not_candidate;
        return outcome;
    }

    if (!legacy_plan::dxbc::checksum_container_valid(
            source,
            size)) {
        outcome.result =
            v211_materialize_result::fail_invalid_dxbc;
        return outcome;
    }

    const auto *plan = find_plan(source, size);
    if (plan == nullptr) {
        outcome.result =
            v211_materialize_result::pass_unknown_exact_sha;
        return outcome;
    }

    outcome.receiver_id = plan->receiver_id;

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t shex_index = 0u;

    if (!parse_dxbc(
            source,
            size,
            chunks,
            shex_index) ||
        !extract_words(
            chunks,
            shex_index,
            words) ||
        words.size() < 12u ||
        words[1] != words.size()) {
        outcome.result =
            v211_materialize_result::fail_invalid_dxbc;
        return outcome;
    }

    for (const auto site : plan->cb_sites) {
        if (site + 1u >= words.size() ||
            words[site] != 0u ||
            words[site + 1u] != 9u) {
            outcome.result =
                v211_materialize_result::fail_patch_precondition;
            return outcome;
        }
    }

    if (plan->pow_site + 2u >= words.size() ||
        words[plan->pow_site] != 0x400ccccdu ||
        words[plan->pow_site + 1u] != 0x400ccccdu ||
        words[plan->pow_site + 2u] != 0x400ccccdu) {
        outcome.result =
            v211_materialize_result::fail_patch_precondition;
        return outcome;
    }

    for (const auto site : plan->cb_sites) {
        words[site] = 12u;
        words[site + 1u] = 1u;
    }

    words[plan->pow_site] = 0x3f800000u;
    words[plan->pow_site + 1u] = 0x3f800000u;
    words[plan->pow_site + 2u] = 0x3f800000u;

    try {
        words.insert(
            words.begin() + 11,
            k_cb12_decl.begin(),
            k_cb12_decl.end());
    } catch (...) {
        outcome.result =
            v211_materialize_result::fail_rebuild;
        return outcome;
    }
    words[1] += 4u;

    std::vector<std::uint8_t> v29;
    if (!rebuild(
            source,
            size,
            std::move(chunks),
            shex_index,
            words,
            v29)) {
        outcome.result =
            v211_materialize_result::fail_rebuild;
        return outcome;
    }

    if (!hash_matches(v29, plan->v29_sha256)) {
        outcome.result =
            v211_materialize_result::fail_stage_sha;
        return outcome;
    }

    if (!parse_words(
            v29,
            chunks,
            shex_index,
            words)) {
        outcome.result =
            v211_materialize_result::fail_rebuild;
        return outcome;
    }

    for (const auto &patch : plan->v210) {
        if (patch.word >= words.size() ||
            words[patch.word] != patch.old_value) {
            outcome.result =
                v211_materialize_result::fail_patch_precondition;
            return outcome;
        }
        words[patch.word] = patch.new_value;
    }

    std::vector<std::uint8_t> v210;
    if (!rebuild(
            v29.data(),
            v29.size(),
            std::move(chunks),
            shex_index,
            words,
            v210)) {
        outcome.result =
            v211_materialize_result::fail_rebuild;
        return outcome;
    }

    if (!hash_matches(v210, plan->v210_sha256)) {
        outcome.result =
            v211_materialize_result::fail_stage_sha;
        return outcome;
    }

    if (!parse_words(
            v210,
            chunks,
            shex_index,
            words)) {
        outcome.result =
            v211_materialize_result::fail_rebuild;
        return outcome;
    }

    if (plan->v211.word >= words.size() ||
        words[plan->v211.word] !=
            plan->v211.old_value) {
        outcome.result =
            v211_materialize_result::fail_patch_precondition;
        return outcome;
    }

    words[plan->v211.word] =
        plan->v211.new_value;

    if (!rebuild(
            v210.data(),
            v210.size(),
            std::move(chunks),
            shex_index,
            words,
            output)) {
        outcome.result =
            v211_materialize_result::fail_rebuild;
        return outcome;
    }

    if (output.size() != plan->replacement_size ||
        !hash_matches(output, plan->v211_sha256)) {
        output.clear();
        outcome.result =
            v211_materialize_result::fail_stage_sha;
        return outcome;
    }

    if (!compose_enabled_a1_islands(
            features,
            source,
            size,
            output,
            outcome.composed_owners) ||
        !augment_final_rdef_b12(output)) {
        output.clear();
        outcome.result =
            v211_materialize_result::fail_patch_precondition;
        return outcome;
    }

    outcome.result =
        v211_materialize_result::applied;
    return outcome;
}

} // namespace dsrrl::operators::material_response
