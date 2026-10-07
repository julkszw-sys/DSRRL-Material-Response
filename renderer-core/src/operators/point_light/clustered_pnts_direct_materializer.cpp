#include "dsrrl/operators/point_light/clustered_pnts_direct_materializer.hpp"

#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/a1_create_time_materializer.hpp"
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

clustered_pnts_rowaware_attenuation_outcome
materialize_clustered_pnts_rowaware_attenuation(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept
{
    clustered_pnts_rowaware_attenuation_outcome outcome{};
    output.clear();

    if (source == nullptr || size == 0u) {
        outcome.result =
            clustered_pnts_rowaware_attenuation_result::
                pass_not_candidate;
        return outcome;
    }

    if (!candidate_size(size)) {
        outcome.result =
            clustered_pnts_rowaware_attenuation_result::
                pass_not_candidate;
        return outcome;
    }

    if (!legacy_plan::dxbc::checksum_container_valid(
            source,
            size)) {
        outcome.result =
            clustered_pnts_rowaware_attenuation_result::
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
            clustered_pnts_rowaware_attenuation_result::
                pass_unknown_exact_sha;
        return outcome;
    }

    outcome.host_sha256 = host_digest;
    outcome.host_size = size;
    outcome.representative_shader_index =
        plan->representative_shader_index;

    // The 12 NoSpc hosts are also exact A1 plans. Preserve every enabled
    // non-attenuation A1 island first, but deliberately leave the stock cubic
    // attenuation words untouched. The row-aware marker patch below then owns
    // attenuation for all 36 exact PntS hosts. This is what makes the ten
    // DSR-only PointLight rows genuinely stock on NoSpc receivers too.
    std::vector<std::uint8_t> composed_basis;
    const std::uint8_t *basis = source;

    if (!plan->spc) {
        const auto *a1_plan =
            legacy_plan::find_a1_plan_by_exact_digest(
                size,
                host_digest);
        if (a1_plan == nullptr) {
            outcome.result =
                clustered_pnts_rowaware_attenuation_result::
                    fail_patch_precondition;
            return outcome;
        }

        core::feature_registry filtered_features;
        constexpr std::array<core::operator_id,4> preserve_owners{{
            core::operator_id::terminal_sat_rgb,
            core::operator_id::diffuse_material_domain,
            core::operator_id::envspec_nospc_delete,
            core::operator_id::fixed_postfog_identity
        }};
        for (const auto owner : preserve_owners) {
            if (!filtered_features.set(
                    owner,
                    features.enabled(owner))) {
                outcome.result =
                    clustered_pnts_rowaware_attenuation_result::
                        fail_patch_precondition;
                return outcome;
            }
        }
        if (!filtered_features.set(
                core::operator_id::pointlight_pnts_attenuation,
                false)) {
            outcome.result =
                clustered_pnts_rowaware_attenuation_result::
                    fail_patch_precondition;
            return outcome;
        }

        const auto a1 =
            legacy_plan::materialize_verified_a1_plan(
                filtered_features,
                *a1_plan,
                source,
                size,
                composed_basis);

        using a1_result =
            legacy_plan::a1_create_time_result;
        if (a1.result == a1_result::applied) {
            if (composed_basis.size() != size) {
                outcome.result =
                    clustered_pnts_rowaware_attenuation_result::
                        fail_rebuild;
                return outcome;
            }
            basis = composed_basis.data();
        } else if (
            a1.result !=
                a1_result::pass_through_no_enabled_owner) {
            outcome.result =
                clustered_pnts_rowaware_attenuation_result::
                    fail_patch_precondition;
            return outcome;
        }
    }

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index = 0u;
    if (!parse_dxbc(
            basis,
            size,
            chunks,
            code_index,
            words)) {
        outcome.result =
            clustered_pnts_rowaware_attenuation_result::
                fail_invalid_dxbc;
        return outcome;
    }
    (void)code_index;

    const generated::clustered_pnts_journal_op_v1 *square_op = nullptr;
    const generated::clustered_pnts_journal_op_v1 *terminal_op = nullptr;

    for (std::uint32_t i = 0u; i < plan->op_count; ++i) {
        const auto &op =
            generated::k_clustered_pnts_journal_ops_v1[
                plan->first_op + i];

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
            if (square_op != nullptr) {
                outcome.result =
                    clustered_pnts_rowaware_attenuation_result::
                        fail_patch_precondition;
                return outcome;
            }
            square_op = &op;
        } else if (
            old_token == 0x07002038u &&
            new_token == 0x07002034u) {
            if (terminal_op != nullptr) {
                outcome.result =
                    clustered_pnts_rowaware_attenuation_result::
                        fail_patch_precondition;
                return outcome;
            }
            terminal_op = &op;
        }
    }

    if (square_op == nullptr ||
        terminal_op == nullptr ||
        square_op->start >= words.size() ||
        terminal_op->start + 7u > words.size() ||
        words[square_op->start] != 0x07000038u ||
        words[terminal_op->start] != 0x07002038u) {
        outcome.result =
            clustered_pnts_rowaware_attenuation_result::
                fail_patch_precondition;
        return outcome;
    }

    // Locate the exact t18 +0x20 structured load already present in the
    // attested stock host and the single dcl_temps declaration. The new
    // marker load clones this ABI exactly and changes only destination and
    // byte offset to the producer-owned padding lane t18+0x28.
    std::size_t temp_decl = static_cast<std::size_t>(-1);
    std::size_t t18_load20 = static_cast<std::size_t>(-1);
    for (std::size_t pos = 2u; pos < words.size();) {
        const auto token = words[pos];
        const auto length =
            static_cast<std::size_t>((token >> 24u) & 0x7Fu);
        const auto opcode = token & 0x7FFu;
        if (length == 0u || pos + length > words.size()) {
            outcome.result =
                clustered_pnts_rowaware_attenuation_result::
                    fail_patch_precondition;
            return outcome;
        }

        if (opcode == 104u && length == 2u) {
            if (temp_decl != static_cast<std::size_t>(-1)) {
                outcome.result =
                    clustered_pnts_rowaware_attenuation_result::
                        fail_patch_precondition;
                return outcome;
            }
            temp_decl = pos;
        }

        if (opcode == 167u &&
            length == 11u &&
            words[pos + 8u] == 0x00000020u &&
            words[pos + 9u] == 0x00107006u &&
            words[pos + 10u] == 18u) {
            if (t18_load20 != static_cast<std::size_t>(-1)) {
                outcome.result =
                    clustered_pnts_rowaware_attenuation_result::
                        fail_patch_precondition;
                return outcome;
            }
            t18_load20 = pos;
        }

        pos += length;
    }

    if (temp_decl == static_cast<std::size_t>(-1) ||
        t18_load20 == static_cast<std::size_t>(-1) ||
        t18_load20 + 11u > square_op->start ||
        words[temp_decl + 1u] >= 0x1000u) {
        outcome.result =
            clustered_pnts_rowaware_attenuation_result::
                fail_patch_precondition;
        return outcome;
    }

    const std::uint32_t scratch_reg =
        words[temp_decl + 1u];
    words[temp_decl + 1u] = scratch_reg + 1u;

    // The cubic terminal is in-place in every exact journal host:
    //   mul_sat dst, dst, square
    // Verify destination component/register against its first source so the
    // post-op can safely read the freshly written cubic value.
    const auto terminal = terminal_op->start;
    const auto dst_token = words[terminal + 1u];
    const auto dst_reg = words[terminal + 2u];
    const auto x_token = words[terminal + 3u];
    const auto x_reg = words[terminal + 4u];

    std::uint32_t expected_dst_token = 0u;
    switch (x_token) {
    case 0x0010000Au: expected_dst_token = 0x00100012u; break;
    case 0x0010001Au: expected_dst_token = 0x00100022u; break;
    case 0x0010002Au: expected_dst_token = 0x00100042u; break;
    case 0x0010003Au: expected_dst_token = 0x00100082u; break;
    default:
        outcome.result =
            clustered_pnts_rowaware_attenuation_result::
                fail_patch_precondition;
        return outcome;
    }

    if (dst_token != expected_dst_token ||
        dst_reg != x_reg) {
        outcome.result =
            clustered_pnts_rowaware_attenuation_result::
                fail_patch_precondition;
        return outcome;
    }

    std::array<std::uint32_t,11> marker_load{};
    for (std::size_t i = 0u; i < marker_load.size(); ++i)
        marker_load[i] = words[t18_load20 + i];

    marker_load[3] = 0x00100012u; // scratch.x
    marker_load[4] = scratch_reg;
    marker_load[8] = 0x00000028u; // t18 record padding marker

    // Preserve the stock cubic result, then select PTDE linear attenuation
    // without a dynamic branch:
    //   linear_candidate = marker * x
    //   A = SAT(max(x^3, linear_candidate))
    // marker=0.0 => exact stock x^3
    // marker=1.0 => exact PTDE x for x in [0,1].
    const std::array<std::uint32_t,14> rowaware_post{{
        0x07000038u,
        0x00100012u, scratch_reg,
        0x0010000Au, scratch_reg,
        x_token, x_reg,
        0x07002034u,
        dst_token, dst_reg,
        x_token, x_reg,
        0x0010000Au, scratch_reg
    }};

    const auto marker_insert = t18_load20 + 11u;
    const auto post_insert = terminal + 7u;

    std::vector<std::uint32_t> patched;
    try {
        patched.reserve(words.size() +
                        marker_load.size() +
                        rowaware_post.size());
        for (std::size_t i = 0u; i <= words.size(); ++i) {
            if (i == marker_insert)
                patched.insert(
                    patched.end(),
                    marker_load.begin(),
                    marker_load.end());

            if (i == post_insert)
                patched.insert(
                    patched.end(),
                    rowaware_post.begin(),
                    rowaware_post.end());

            if (i < words.size())
                patched.push_back(words[i]);
        }
    } catch (...) {
        outcome.result =
            clustered_pnts_rowaware_attenuation_result::
                fail_rebuild;
        return outcome;
    }

    if (patched.size() != words.size() + 25u ||
        patched.size() >
            std::numeric_limits<std::uint32_t>::max()) {
        outcome.result =
            clustered_pnts_rowaware_attenuation_result::
                fail_patch_precondition;
        return outcome;
    }

    patched[1] =
        static_cast<std::uint32_t>(patched.size());

    if (!rebuild(
            basis,
            size,
            std::move(chunks),
            patched,
            output)) {
        output.clear();
        outcome.result =
            clustered_pnts_rowaware_attenuation_result::
                fail_rebuild;
        return outcome;
    }

    if (output.size() != size + 100u ||
        !legacy_plan::dxbc::checksum_container_valid(
            output.data(),
            output.size())) {
        output.clear();
        outcome.result =
            clustered_pnts_rowaware_attenuation_result::
                fail_final;
        return outcome;
    }

    outcome.replacement_sha256 =
        hashing::sha256(
            output.data(),
            output.size());
    outcome.replacement_size = output.size();
    outcome.result =
        clustered_pnts_rowaware_attenuation_result::applied;
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
