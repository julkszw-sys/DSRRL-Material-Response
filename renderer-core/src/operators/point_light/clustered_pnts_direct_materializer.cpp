#include "dsrrl/operators/point_light/clustered_pnts_direct_materializer.hpp"

#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include "dsrrl/operators/point_light/generated_clustered_pnts_direct_v1.hpp"

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

// Historical direct-PTDE clustered journals were authored for the original
// PointLight-specific b12 carrier:
//   b12[0].xyz = raw PTDE c101
//   b12[0].w   = PTDE c102
//   b12[1].yzw = PTDE c100
// Runtime-v2 shares the post-reset MR carrier instead:
//   b12[0].xyz = neutral
//   b12[0].w   = PTDE c102
//   b12[1].xyz = PTDE c100
//   b12[2].xyz = raw PTDE c101
// Keep the immutable historical journal as the first attestation stage, then
// migrate only these exact inserted operands to the current ABI.
constexpr std::uint32_t k_cb_src_xyz = 0x00208246u;
constexpr std::uint32_t k_cb_src_yzw = 0x00208396u;
constexpr std::uint32_t k_cb_src_w = 0x0020803au;
constexpr std::uint32_t k_dsrrl_b12_slot = 12u;

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

bool attest_composed_shader_owners(
    const generated::clustered_pnts_plan_v1 &plan,
    core::operator_mask &owners) noexcept
{
    owners = 0u;
    std::uint32_t diffuse_domain = 0u;
    std::uint32_t attenuation_a = 0u;
    std::uint32_t attenuation_b = 0u;
    std::uint32_t envspec_delete = 0u;
    std::uint32_t terminal_sat = 0u;
    std::uint32_t legacy_c100 = 0u;
    std::uint32_t legacy_c101 = 0u;
    std::uint32_t legacy_c102 = 0u;

    if (plan.first_op >
            generated::k_clustered_pnts_journal_ops_v1.size() ||
        plan.op_count >
            generated::k_clustered_pnts_journal_ops_v1.size() -
                plan.first_op)
        return false;

    const auto token_at =
        [](std::uint32_t offset) noexcept -> std::uint32_t {
            return generated::
                k_clustered_pnts_journal_tokens_v1[offset];
        };

    for (std::uint32_t i=0u;i<plan.op_count;++i) {
        const auto &op =
            generated::k_clustered_pnts_journal_ops_v1[
                plan.first_op+i];

        if (op.old_offset >
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

        for (std::uint32_t n=0u;
             n+2u<op.new_count;
             ++n) {
            const auto token =
                token_at(op.new_offset+n);
            const auto slot =
                token_at(op.new_offset+n+1u);
            const auto row =
                token_at(op.new_offset+n+2u);

            if (slot != k_dsrrl_b12_slot)
                continue;

            if (token == k_cb_src_yzw &&
                row == 1u)
                ++legacy_c100;
            else if (token == k_cb_src_xyz &&
                     row == 0u)
                ++legacy_c101;
            else if (token == k_cb_src_w &&
                     row == 0u)
                ++legacy_c102;
        }

        if (op.old_count==3u && op.new_count==3u &&
            token_at(op.old_offset+0u)==0x400ccccdu &&
            token_at(op.old_offset+1u)==0x400ccccdu &&
            token_at(op.old_offset+2u)==0x400ccccdu &&
            token_at(op.new_offset+0u)==0x3f800000u &&
            token_at(op.new_offset+1u)==0x3f800000u &&
            token_at(op.new_offset+2u)==0x3f800000u) {
            ++diffuse_domain;
            continue;
        }

        if (op.old_count!=1u || op.new_count!=1u)
            continue;

        const auto old_token=token_at(op.old_offset);
        const auto new_token=token_at(op.new_offset);

        if (old_token==0x07000038u &&
            new_token==0x07000031u) {
            ++envspec_delete;
            continue;
        }
        if (old_token==0x07000038u &&
            new_token==0x07000033u) {
            ++attenuation_a;
            continue;
        }
        if (old_token==0x07002038u &&
            new_token==0x07002034u) {
            ++attenuation_b;
            continue;
        }
        if (old_token==0x05000036u &&
            new_token==0x05002036u) {
            ++terminal_sat;
            continue;
        }
    }

    const bool exact =
        diffuse_domain==1u &&
        attenuation_a==1u &&
        attenuation_b==1u &&
        terminal_sat==1u &&
        envspec_delete==(plan.spc ? 0u : 1u) &&
        legacy_c100==1u &&
        legacy_c101==(plan.spc ? 1u : 0u) &&
        legacy_c102==(plan.spc ? 1u : 0u);
    if (!exact)
        return false;

    owners =
        clustered_pnts_required_composed_shader_owners(
            plan.spc);
    return owners!=0u;
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

bool migrate_legacy_pointlight_b12_to_current(
    std::vector<std::uint32_t> &words,
    bool spc) noexcept
{
    std::uint32_t c100_old = 0u;
    std::uint32_t c101_old = 0u;
    std::uint32_t c102_current = 0u;

    for (std::size_t i=2u;
         i+2u<words.size();
         ++i) {
        if (words[i+1u] != k_dsrrl_b12_slot)
            continue;

        if (words[i] == k_cb_src_yzw &&
            words[i+2u] == 1u) {
            ++c100_old;
            words[i] = k_cb_src_xyz;
            continue;
        }

        if (words[i] == k_cb_src_xyz &&
            words[i+2u] == 0u) {
            ++c101_old;
            words[i+2u] = 2u;
            continue;
        }

        if (words[i] == k_cb_src_w &&
            words[i+2u] == 0u)
            ++c102_current;
    }

    if (c100_old != 1u ||
        c101_old != (spc ? 1u : 0u) ||
        c102_current != (spc ? 1u : 0u))
        return false;

    std::uint32_t c100_current = 0u;
    std::uint32_t c101_current = 0u;
    std::uint32_t c100_stale = 0u;
    std::uint32_t c101_stale = 0u;
    std::uint32_t c102_post = 0u;

    for (std::size_t i=2u;
         i+2u<words.size();
         ++i) {
        if (words[i+1u] != k_dsrrl_b12_slot)
            continue;

        if (words[i] == k_cb_src_xyz &&
            words[i+2u] == 1u)
            ++c100_current;
        else if (words[i] == k_cb_src_xyz &&
                 words[i+2u] == 2u)
            ++c101_current;
        else if (words[i] == k_cb_src_yzw &&
                 words[i+2u] == 1u)
            ++c100_stale;
        else if (words[i] == k_cb_src_xyz &&
                 words[i+2u] == 0u)
            ++c101_stale;
        else if (words[i] == k_cb_src_w &&
                 words[i+2u] == 0u)
            ++c102_post;
    }

    return
        c100_current==1u &&
        c101_current==(spc ? 1u : 0u) &&
        c100_stale==0u &&
        c101_stale==0u &&
        c102_post==(spc ? 1u : 0u);
}


bool find_marker_attenuation_journal_starts(
    const generated::clustered_pnts_plan_v1 &plan,
    std::uint32_t &square_start,
    std::uint32_t &cubic_start) noexcept
{
    square_start = 0u;
    cubic_start = 0u;
    std::uint32_t square_hits = 0u;
    std::uint32_t cubic_hits = 0u;

    for (std::uint32_t i = 0u; i < plan.op_count; ++i) {
        const auto &op =
            generated::k_clustered_pnts_journal_ops_v1[
                plan.first_op + i];
        if (op.old_count != 1u ||
            op.new_count != 1u ||
            op.old_offset >=
                generated::k_clustered_pnts_journal_tokens_v1.size() ||
            op.new_offset >=
                generated::k_clustered_pnts_journal_tokens_v1.size())
            continue;

        const auto old_token =
            generated::k_clustered_pnts_journal_tokens_v1[
                op.old_offset];
        const auto new_token =
            generated::k_clustered_pnts_journal_tokens_v1[
                op.new_offset];

        if (old_token == 0x07000038u &&
            new_token == 0x07000033u) {
            square_start = op.start;
            ++square_hits;
        } else if (
            old_token == 0x07002038u &&
            new_token == 0x07002034u) {
            cubic_start = op.start;
            ++cubic_hits;
        }
    }

    return
        square_hits == 1u &&
        cubic_hits == 1u &&
        cubic_start == square_start + 7u;
}

template <std::size_t N>
bool find_unique_instruction_sequence(
    const std::vector<std::uint32_t> &words,
    const std::array<std::uint32_t,N> &sequence,
    std::size_t &position) noexcept
{
    position = 0u;
    std::uint32_t hits = 0u;
    std::size_t i = 2u;

    while (i < words.size()) {
        const auto length =
            static_cast<std::size_t>(
                (words[i] >> 24u) & 0x7fu);
        if (length == 0u ||
            length > words.size() - i)
            return false;

        if (length == N &&
            std::equal(
                sequence.begin(),
                sequence.end(),
                words.begin() +
                    static_cast<std::ptrdiff_t>(i))) {
            position = i;
            ++hits;
        }

        i += length;
    }

    return i == words.size() && hits == 1u;
}

bool find_unique_t18_offset32_load(
    const std::vector<std::uint32_t> &words,
    std::size_t &position) noexcept
{
    position = 0u;
    std::uint32_t hits = 0u;
    std::size_t i = 2u;

    while (i < words.size()) {
        const auto token = words[i];
        const auto length =
            static_cast<std::size_t>(
                (token >> 24u) & 0x7fu);
        if (length == 0u ||
            length > words.size() - i)
            return false;

        if ((token & 0x7ffu) == 0x0a7u &&
            length == 11u &&
            words[i + 7u] == 0x00004001u &&
            words[i + 8u] == 0x00000020u &&
            words[i + 10u] == 18u) {
            position = i;
            ++hits;
        }

        i += length;
    }

    return i == words.size() && hits == 1u;
}

bool find_unique_dcl_temps(
    const std::vector<std::uint32_t> &words,
    std::size_t &position) noexcept
{
    position = 0u;
    std::uint32_t hits = 0u;
    std::size_t i = 2u;

    while (i < words.size()) {
        const auto token = words[i];
        const auto length =
            static_cast<std::size_t>(
                (token >> 24u) & 0x7fu);
        if (length == 0u ||
            length > words.size() - i)
            return false;

        if ((token & 0x7ffu) == 0x068u &&
            length == 2u) {
            position = i;
            ++hits;
        }

        i += length;
    }

    return i == words.size() && hits == 1u;
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

bool migrate_clustered_pnts_legacy_b12_words(
    std::vector<std::uint32_t> &words,
    bool spc) noexcept
{
    return migrate_legacy_pointlight_b12_to_current(
        words,
        spc);
}


clustered_pnts_marker_attenuation_result
identify_clustered_pnts_marker_attenuation(
    const std::uint8_t *source,
    std::size_t size,
    clustered_pnts_marker_attenuation_identity &identity) noexcept
{
    identity = {};

    if (source == nullptr || size == 0u)
        return clustered_pnts_marker_attenuation_result::
            pass_not_candidate;

    if (!candidate_size(size))
        return clustered_pnts_marker_attenuation_result::
            pass_not_candidate;

    if (!legacy_plan::dxbc::checksum_container_valid(
            source,
            size))
        return clustered_pnts_marker_attenuation_result::
            fail_invalid_dxbc;

    core::sha256_digest digest{};
    const auto *plan =
        find_plan(source, size, digest);
    if (plan == nullptr)
        return clustered_pnts_marker_attenuation_result::
            pass_unknown_exact_sha;

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index = 0u;
    if (!parse_dxbc(
            source,
            size,
            chunks,
            code_index,
            words))
        return clustered_pnts_marker_attenuation_result::
            fail_invalid_dxbc;

    std::uint32_t square_start = 0u;
    std::uint32_t cubic_start = 0u;
    if (!find_marker_attenuation_journal_starts(
            *plan,
            square_start,
            cubic_start) ||
        square_start + 7u > words.size() ||
        cubic_start + 7u > words.size() ||
        words[square_start] != 0x07000038u ||
        words[cubic_start] != 0x07002038u)
        return clustered_pnts_marker_attenuation_result::
            fail_identity_precondition;

    std::size_t t18_load = 0u;
    if (!find_unique_t18_offset32_load(
            words,
            t18_load) ||
        t18_load + 11u > words.size() ||
        !(t18_load < square_start &&
          square_start < cubic_start))
        return clustered_pnts_marker_attenuation_result::
            fail_identity_precondition;

    std::copy_n(
        words.begin() +
            static_cast<std::ptrdiff_t>(t18_load),
        identity.t18_offset32_load.size(),
        identity.t18_offset32_load.begin());
    std::copy_n(
        words.begin() +
            static_cast<std::ptrdiff_t>(square_start),
        identity.square_mul.size(),
        identity.square_mul.begin());
    std::copy_n(
        words.begin() +
            static_cast<std::ptrdiff_t>(cubic_start),
        identity.cubic_mul_sat.size(),
        identity.cubic_mul_sat.begin());

    identity.host_sha256 = digest;
    identity.host_size = size;
    identity.representative_shader_index =
        plan->representative_shader_index;
    identity.spc = plan->spc;
    identity.exact = true;

    return clustered_pnts_marker_attenuation_result::applied;
}

clustered_pnts_marker_attenuation_outcome
materialize_clustered_pnts_marker_attenuation(
    const clustered_pnts_marker_attenuation_identity &identity,
    const std::uint8_t *post_a1_source,
    std::size_t post_a1_size,
    std::vector<std::uint8_t> &output) noexcept
{
    clustered_pnts_marker_attenuation_outcome outcome{};
    output.clear();

    outcome.host_sha256 = identity.host_sha256;
    outcome.host_size = identity.host_size;
    outcome.representative_shader_index =
        identity.representative_shader_index;

    if (!identity.exact ||
        post_a1_source == nullptr ||
        post_a1_size == 0u) {
        outcome.result =
            clustered_pnts_marker_attenuation_result::
                fail_identity_precondition;
        return outcome;
    }

    if (!legacy_plan::dxbc::checksum_container_valid(
            post_a1_source,
            post_a1_size)) {
        outcome.result =
            clustered_pnts_marker_attenuation_result::
                fail_invalid_dxbc;
        return outcome;
    }

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index = 0u;
    if (!parse_dxbc(
            post_a1_source,
            post_a1_size,
            chunks,
            code_index,
            words)) {
        outcome.result =
            clustered_pnts_marker_attenuation_result::
                fail_invalid_dxbc;
        return outcome;
    }

    std::size_t t18_load = 0u;
    std::size_t square = 0u;
    std::size_t cubic = 0u;
    std::size_t dcl_temps = 0u;

    auto a1_square = identity.square_mul;
    auto a1_cubic = identity.cubic_mul_sat;
    a1_square[0u] = 0x07000033u; // A1 MIN(x,x)
    a1_cubic[0u] = 0x07002034u;  // A1 MAX_SAT(x,tmp)

    std::size_t stock_square = 0u;
    std::size_t stock_cubic = 0u;
    std::size_t linear_square = 0u;
    std::size_t linear_cubic = 0u;
    const bool stock_pair =
        find_unique_instruction_sequence(
            words,
            identity.square_mul,
            stock_square) &&
        find_unique_instruction_sequence(
            words,
            identity.cubic_mul_sat,
            stock_cubic);
    const bool a1_linear_pair =
        find_unique_instruction_sequence(
            words,
            a1_square,
            linear_square) &&
        find_unique_instruction_sequence(
            words,
            a1_cubic,
            linear_cubic);

    if (stock_pair == a1_linear_pair ||
        !find_unique_instruction_sequence(
            words,
            identity.t18_offset32_load,
            t18_load) ||
        !find_unique_dcl_temps(
            words,
            dcl_temps)) {
        outcome.result =
            clustered_pnts_marker_attenuation_result::
                fail_post_a1_pattern;
        return outcome;
    }

    if (stock_pair) {
        square = stock_square;
        cubic = stock_cubic;
    } else {
        square = linear_square;
        cubic = linear_cubic;

        // A1's global attenuation island is exact SAT(x), but R44 needs a
        // per-light decision so rare DSR-only rows can remain x^3. Restore
        // the two original MUL opcodes first; the marker MOVC below selects
        // the PTDE linear value only for producer-marked lights.
        words[square] = identity.square_mul[0u];
        words[cubic] = identity.cubic_mul_sat[0u];
    }

    if (!(dcl_temps < t18_load &&
          t18_load < square &&
          square < cubic) ||
        cubic + 7u > words.size() ||
        dcl_temps + 1u >= words.size()) {
        outcome.result =
            clustered_pnts_marker_attenuation_result::
                fail_post_a1_pattern;
        return outcome;
    }

    const auto new_temp = words[dcl_temps + 1u];
    if (new_temp == 0u ||
        new_temp >= 0x0000ffffu ||
        identity.t18_offset32_load[7u] != 0x00004001u ||
        identity.t18_offset32_load[8u] != 0x00000020u ||
        identity.t18_offset32_load[10u] != 18u ||
        identity.cubic_mul_sat[1u] == 0u ||
        identity.cubic_mul_sat[3u] == 0u) {
        outcome.result =
            clustered_pnts_marker_attenuation_result::
                fail_post_a1_pattern;
        return outcome;
    }

    // Reserve one fresh temp. The exact PntS bodies use one unique dcl_temps.
    words[dcl_temps + 1u] = new_temp + 1u;

    // Preserve x in the stock destination. Compute SAT(x^3) into rN.y instead.
    const auto original_dest_token =
        words[cubic + 1u];
    const auto original_dest_index =
        words[cubic + 2u];
    const auto linear_source_token =
        words[cubic + 3u];
    const auto linear_source_index =
        words[cubic + 4u];

    words[cubic + 1u] = 0x00100022u; // rN.y destination
    words[cubic + 2u] = new_temp;

    auto marker_load = identity.t18_offset32_load;
    marker_load[3u] = 0x00100012u; // rN.x destination
    marker_load[4u] = new_temp;
    marker_load[8u] = 0x00000028u; // t18 + 0x28 marker

    const std::array<std::uint32_t,9> select = {{
        0x09002037u,             // movc_sat
        original_dest_token,
        original_dest_index,
        0x0010000au, new_temp,   // marker rN.x
        linear_source_token,
        linear_source_index,     // PTDE: x
        0x0010001au, new_temp    // stock DSR: SAT(x^3) in rN.y
    }};

    try {
        words.insert(
            words.begin() +
                static_cast<std::ptrdiff_t>(cubic + 7u),
            select.begin(),
            select.end());
        words.insert(
            words.begin() +
                static_cast<std::ptrdiff_t>(t18_load),
            marker_load.begin(),
            marker_load.end());
    } catch (...) {
        outcome.result =
            clustered_pnts_marker_attenuation_result::
                fail_rebuild;
        return outcome;
    }

    if (words.size() >
            std::numeric_limits<std::uint32_t>::max()) {
        outcome.result =
            clustered_pnts_marker_attenuation_result::
                fail_rebuild;
        return outcome;
    }
    words[1u] =
        static_cast<std::uint32_t>(
            words.size());

    if (!rebuild(
            post_a1_source,
            post_a1_size,
            std::move(chunks),
            words,
            output) ||
        output.empty() ||
        !legacy_plan::dxbc::checksum_container_valid(
            output.data(),
            output.size())) {
        output.clear();
        outcome.result =
            clustered_pnts_marker_attenuation_result::
                fail_rebuild;
        return outcome;
    }

    outcome.replacement_sha256 =
        hashing::sha256(
            output.data(),
            output.size());
    outcome.replacement_size = output.size();
    outcome.result =
        clustered_pnts_marker_attenuation_result::applied;
    return outcome;
}

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

    if (!attest_composed_shader_owners(
            *plan,
            outcome.composed_shader_owners) ||
        outcome.composed_shader_owners !=
            clustered_pnts_required_composed_shader_owners(
                plan->spc)) {
        outcome.result =
            clustered_pnts_direct_materialize_result::
                fail_patch_precondition;
        return outcome;
    }

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

    // First prove that the immutable historical direct-PTDE journal still
    // reconstructs its byte-exact certified replacement. Only after that
    // attestation do we migrate the journal's old PointLight b12 operands to
    // the current Runtime-v2 carrier ABI.
    auto legacy_chunks = chunks;
    std::vector<std::uint8_t> legacy_output;
    if (!strip_rdef(legacy_chunks) ||
        !rebuild(
            source,
            size,
            std::move(legacy_chunks),
            words,
            legacy_output)) {
        outcome.result =
            clustered_pnts_direct_materialize_result::
                fail_rebuild;
        return outcome;
    }

    const auto legacy_digest =
        hashing::sha256(
            legacy_output.data(),
            legacy_output.size());
    if (legacy_output.size() !=
            plan->replacement_size ||
        !hashing::matches_hex(
            legacy_digest,
            plan->replacement_sha256)) {
        outcome.result =
            clustered_pnts_direct_materialize_result::
                fail_final_sha;
        return outcome;
    }

    if (!migrate_legacy_pointlight_b12_to_current(
            words,
            plan->spc)) {
        outcome.result =
            clustered_pnts_direct_materialize_result::
                fail_patch_precondition;
        return outcome;
    }

    outcome.current_b12_abi = true;
    outcome.legacy_specular_complete =
        plan->spc;

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

    const auto replacement_digest =
        hashing::sha256(
            output.data(),
            output.size());

    // The ABI migration is a same-length operand rewrite. Exact original host
    // SHA + byte-exact legacy journal SHA + exact migration cardinality above
    // are the attestation chain for the deterministic current-ABI output.
    if (output.size() !=
            plan->replacement_size ||
        !legacy_plan::dxbc::checksum_container_valid(
            output.data(),
            output.size())) {
        output.clear();
        outcome.result =
            clustered_pnts_direct_materialize_result::
                fail_final_sha;
        return outcome;
    }

    outcome.replacement_sha256 =
        replacement_digest;
    outcome.replacement_size =
        output.size();
    outcome.result =
        clustered_pnts_direct_materialize_result::
            applied;
    return outcome;
}

} // namespace dsrrl::operators::point_light
