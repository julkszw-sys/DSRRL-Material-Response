#include "dsrrl/operators/material_response/material_response_v211_materializer.hpp"
#include "dsrrl/operators/material_response/generated_v211_plans.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

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

} // namespace

v211_materialize_outcome materialize_v211_stable_receiver(
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

    outcome.result =
        v211_materialize_result::applied;
    return outcome;
}

} // namespace dsrrl::operators::material_response
