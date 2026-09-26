#include "dsrrl/operators/env_spec/pmetal_rgba_lerp_materializer.hpp"

#include "dsrrl/operators/env_spec/generated_pmetal_envspec_hemenvlerp_v1.hpp"
#include "dsrrl/operators/legacy_plan/a1_create_time_materializer.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_rdef_patch.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include "dsrrl/operators/material_response/hemenvlerp_v211_materializer.hpp"
#include "dsrrl/operators/resource_bridges/spec_rgb_consumer_materializer.hpp"
#include "dsrrl/operators/surface/terminal_sat_rgb_patch.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace dsrrl::operators::env_spec {
namespace {

using legacy_plan::dxbc::read_u32;
using legacy_plan::dxbc::write_u32;
namespace hashing = legacy_plan::hashing;
namespace generated_lerp = generated;

struct chunk {
    std::array<char,4> tag{};
    std::vector<std::uint8_t> payload;
};

struct instruction_view {
    std::size_t offset = 0u;
    std::uint32_t opcode = 0u;
    std::size_t length = 0u;
};

bool parse_dxbc(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<chunk> &chunks,
    std::size_t &code_index,
    std::vector<std::uint32_t> &words) noexcept
{
    if (!legacy_plan::dxbc::checksum_container_valid(source, size) ||
        size < 32u)
        return false;

    const auto count = read_u32(source + 28u);
    if (count == 0u || count > 64u ||
        32ull + 4ull * count > size)
        return false;

    chunks.clear();
    code_index = static_cast<std::size_t>(-1);

    try {
        chunks.reserve(count);
        for (std::uint32_t i = 0u; i < count; ++i) {
            const auto off = read_u32(source + 32u + i * 4u);
            if (off > size || size - off < 8u)
                return false;

            const auto payload_size = read_u32(source + off + 4u);
            if (payload_size > size - off - 8u)
                return false;

            chunk current{};
            std::memcpy(current.tag.data(), source + off, 4u);
            current.payload.assign(
                source + off + 8u,
                source + off + 8u + payload_size);

            const bool code =
                std::memcmp(current.tag.data(), "SHEX", 4u) == 0 ||
                std::memcmp(current.tag.data(), "SHDR", 4u) == 0;

            if (code) {
                if (code_index != static_cast<std::size_t>(-1) ||
                    (current.payload.size() & 3u) != 0u)
                    return false;
                code_index = chunks.size();
            }

            chunks.push_back(std::move(current));
        }

        if (code_index == static_cast<std::size_t>(-1))
            return false;

        const auto &payload = chunks[code_index].payload;
        words.resize(payload.size() / 4u);
        for (std::size_t i = 0u; i < words.size(); ++i)
            words[i] = read_u32(payload.data() + i * 4u);
    } catch (...) {
        return false;
    }

    return words.size() >= 2u && words[1] == words.size();
}

bool decode(
    const std::vector<std::uint32_t> &words,
    std::vector<instruction_view> &out) noexcept
{
    out.clear();
    if (words.size() < 2u)
        return false;

    std::size_t cursor = 2u;
    while (cursor < words.size()) {
        const auto length =
            static_cast<std::size_t>((words[cursor] >> 24u) & 0x7fu);

        if (length == 0u || cursor + length > words.size())
            return false;

        out.push_back({
            cursor,
            words[cursor] & 0x7ffu,
            length
        });
        cursor += length;
    }

    return cursor == words.size();
}

bool rebuild(
    const std::uint8_t *source,
    std::size_t source_size,
    std::vector<chunk> chunks,
    std::size_t code_index,
    const std::vector<std::uint32_t> &words,
    std::vector<std::uint8_t> &out) noexcept
{
    if (source == nullptr || code_index >= chunks.size())
        return false;

    try {
        auto &code = chunks[code_index].payload;
        code.resize(words.size() * 4u);

        for (std::size_t i = 0u; i < words.size(); ++i)
            write_u32(code.data() + i * 4u, words[i]);

        const std::size_t header_size = 32u + 4u * chunks.size();
        if (source_size < header_size ||
            header_size > std::numeric_limits<std::uint32_t>::max())
            return false;

        out.assign(source, source + header_size);

        std::vector<std::uint32_t> offsets;
        offsets.reserve(chunks.size());

        for (const auto &current : chunks) {
            if (out.size() > std::numeric_limits<std::uint32_t>::max())
                return false;

            offsets.push_back(static_cast<std::uint32_t>(out.size()));
            out.insert(
                out.end(),
                reinterpret_cast<const std::uint8_t *>(current.tag.data()),
                reinterpret_cast<const std::uint8_t *>(current.tag.data()) + 4u);

            const auto size_at = out.size();
            out.resize(size_at + 4u);
            write_u32(
                out.data() + size_at,
                static_cast<std::uint32_t>(current.payload.size()));
            out.insert(out.end(), current.payload.begin(), current.payload.end());
        }

        if (out.size() > std::numeric_limits<std::uint32_t>::max())
            return false;

        write_u32(out.data() + 24u, static_cast<std::uint32_t>(out.size()));
        write_u32(
            out.data() + 28u,
            static_cast<std::uint32_t>(chunks.size()));

        for (std::size_t i = 0u; i < offsets.size(); ++i)
            write_u32(out.data() + 32u + i * 4u, offsets[i]);

        std::fill(
            out.begin() + 4u,
            out.begin() + 20u,
            std::uint8_t{0});

        return legacy_plan::dxbc::fix_checksum(out.data(), out.size());
    } catch (...) {
        out.clear();
        return false;
    }
}

chunk *unique_rdef(std::vector<chunk> &chunks) noexcept
{
    chunk *rdef = nullptr;

    for (auto &current : chunks) {
        if (std::memcmp(current.tag.data(), "RDEF", 4u) != 0)
            continue;

        if (rdef != nullptr)
            return nullptr;

        rdef = &current;
    }

    return rdef;
}

bool sample_at(
    const std::vector<std::uint32_t> &words,
    std::size_t at,
    std::uint32_t slot) noexcept
{
    if (at + 13u > words.size())
        return false;

    const auto opcode = words[at] & 0x7ffu;
    const auto length = (words[at] >> 24u) & 0x7fu;

    return
        opcode >= 0x45u &&
        opcode <= 0x4au &&
        length == 13u &&
        words[at + 8u] == slot &&
        words[at + 10u] == slot;
}

bool opcode_at(
    const std::vector<std::uint32_t> &words,
    std::size_t at,
    std::uint32_t opcode,
    std::size_t length) noexcept
{
    return
        at < words.size() &&
        (words[at] & 0x7ffu) == opcode &&
        ((words[at] >> 24u) & 0x7fu) == length &&
        at + length <= words.size();
}

std::size_t sample_count(
    const std::vector<std::uint32_t> &words,
    std::uint32_t slot) noexcept
{
    std::vector<instruction_view> instructions;
    if (!decode(words, instructions))
        return std::numeric_limits<std::size_t>::max();

    std::size_t count = 0u;
    for (const auto &ins : instructions) {
        if (ins.opcode < 0x45u ||
            ins.opcode > 0x4au ||
            ins.length < 11u)
            continue;

        if (ins.offset + 10u >= words.size())
            return std::numeric_limits<std::size_t>::max();

        if (words[ins.offset + 8u] == slot)
            ++count;
    }

    return count;
}

bool contains_exact_subsequence(
    const std::vector<std::uint32_t> &haystack,
    const std::vector<std::uint32_t> &needle) noexcept
{
    if (needle.empty() || needle.size() > haystack.size())
        return false;

    const auto first = std::search(
        haystack.begin(),
        haystack.end(),
        needle.begin(),
        needle.end());

    if (first == haystack.end())
        return false;

    const auto second = std::search(
        first + 1,
        haystack.end(),
        needle.begin(),
        needle.end());

    return second == haystack.end();
}

bool terminal_rgb_output_instruction(
    const std::vector<std::uint32_t> &words,
    std::size_t &word) noexcept
{
    word = static_cast<std::size_t>(-1);

    std::vector<instruction_view> instructions;
    if (!decode(words, instructions))
        return false;

    for (const auto &ins : instructions) {
        if (ins.opcode != 0x36u ||
            ins.length != 5u ||
            ins.offset + 4u >= words.size() ||
            words[ins.offset + 1u] != 0x00102072u ||
            words[ins.offset + 2u] != 0u)
            continue;

        if (word != static_cast<std::size_t>(-1))
            return false;

        word = ins.offset;
    }

    return word != static_cast<std::size_t>(-1);
}

bool apply_exact_terminal_rgb_sat(
    std::vector<std::uint32_t> &words,
    const generated_lerp::pmetal_hemenvlerp_site &site) noexcept
{
    const auto word =
        static_cast<std::size_t>(
            site.terminal_rgb_word);

    if (word + 4u >= words.size() ||
        words[word] != 0x05000036u ||
        words[word + 1u] != 0x00102072u ||
        words[word + 2u] != 0u)
        return false;

    words[word] |=
        surface::dxbc_saturate_modifier_bit;

    return words[word] == 0x05002036u;
}

bool terminal_rgb_sat_exact(
    const std::vector<std::uint32_t> &words) noexcept
{
    std::size_t word = 0u;
    return
        terminal_rgb_output_instruction(
            words,
            word) &&
        words[word] == 0x05002036u;
}

constexpr std::array<std::uint32_t,4> k_cb13_decl = {{
    0x04000059u,
    0x00208e46u,
    0x0000000du,
    0x00000008u
}};

constexpr std::array<std::uint32_t,74> k_ptde_rgba_envspec_chain = {{
    0x8d000048u,0x80000182u,0x00155543u,0x001000f2u,0x00000001u,0x00100796u,0x00000000u,0x00107936u,
    0x0000000cu,0x00106000u,0x0000000cu,0x00004001u,0x00000000u,0x0700000eu,0x001000e2u,0x00000001u,
    0x00100e56u,0x00000001u,0x00100006u,0x00000001u,0x08000038u,0x001000e2u,0x00000001u,0x00100e56u,
    0x00000001u,0x00208246u,0x0000000cu,0x00000002u,0x0404001fu,0x0020803au,0x0000000cu,0x00000003u,
    0x8d000048u,0x80000182u,0x00155543u,0x001000f2u,0x0000000cu,0x00100796u,0x00000000u,0x00107936u,
    0x0000000eu,0x00106000u,0x0000000eu,0x00004001u,0x00000000u,0x0700000eu,0x001000e2u,0x0000000cu,
    0x00100e56u,0x0000000cu,0x00100006u,0x0000000cu,0x0b000032u,0x001000e2u,0x0000000cu,0x00100e56u,
    0x0000000cu,0x00208246u,0x0000000cu,0x00000003u,0x80100e56u,0x00000041u,0x00000001u,0x0a000032u,
    0x001000e2u,0x00000001u,0x00100e56u,0x0000000cu,0x0020803au,0x0000000cu,0x00000003u,0x00100e56u,
    0x00000001u,0x01000015u
}};

bool add_bridge_rdef(
    const std::uint8_t *source,
    std::size_t source_size,
    std::vector<chunk> chunks,
    std::size_t code_index,
    const std::vector<std::uint32_t> &words,
    std::vector<std::uint8_t> &output) noexcept
{
    auto *rdef = unique_rdef(chunks);

    if (rdef == nullptr ||
        legacy_plan::dxbc::rdef::has_constant_buffer_binding(
            rdef->payload,
            12u) ||
        legacy_plan::dxbc::rdef::has_constant_buffer_binding(
            rdef->payload,
            13u) ||
        !legacy_plan::dxbc::rdef::append_constant_buffer_binding(
            rdef->payload,
            "DSRRL_MaterialCarrier",
            12u,
            64u) ||
        !legacy_plan::dxbc::rdef::append_constant_buffer_binding(
            rdef->payload,
            "DSRRL_LightBankCarrier",
            13u,
            128u))
        return false;

    return rebuild(
        source,
        source_size,
        std::move(chunks),
        code_index,
        words,
        output);
}

std::size_t adjacent_pair_count(
    const std::vector<std::uint32_t> &words,
    std::uint32_t a,
    std::uint32_t b) noexcept
{
    std::size_t count = 0u;
    for (std::size_t i = 0u; i + 1u < words.size(); ++i)
        if (words[i] == a && words[i + 1u] == b)
            ++count;
    return count;
}

bool final_postcondition(
    const std::vector<std::uint8_t> &bytes,
    const generated_lerp::pmetal_hemenvlerp_site &site,
    const std::vector<std::uint32_t> &preserved_envdiffuse) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index = 0u;

    if (!parse_dxbc(
            bytes.data(),
            bytes.size(),
            chunks,
            code_index,
            words))
        return false;

    auto *rdef = unique_rdef(chunks);
    if (rdef == nullptr ||
        !legacy_plan::dxbc::rdef::has_constant_buffer_binding(
            rdef->payload,
            12u) ||
        !legacy_plan::dxbc::rdef::has_constant_buffer_binding(
            rdef->payload,
            13u))
        return false;

    if (site.t11_word <= site.t12_word ||
        site.postblend_word < site.t12_word ||
        site.postblend_word >=
            site.t12_word + k_ptde_rgba_envspec_chain.size() ||
        site.t9_word <
            site.t12_word + k_ptde_rgba_envspec_chain.size() ||
        site.t9_word >= site.t11_word)
        return false;

    auto expected_envspec_cut =
        std::vector<std::uint32_t>(
            k_ptde_rgba_envspec_chain.begin(),
            k_ptde_rgba_envspec_chain.end());

    expected_envspec_cut[6] =
        site.reflection_coord_register;
    expected_envspec_cut[38] =
        site.reflection_coord_register;

    try {
        expected_envspec_cut.resize(
            site.t11_word - site.t12_word,
            0x0100003au);
    } catch (...) {
        return false;
    }

    // The exact PTDE EnvSpec chain plus the NOP-filled remainder up to the
    // independent EnvDiffuse operator must survive every later composition
    // unchanged. This explicitly prevents DSR-only dynamic-LOD/angular/t9
    // logic from being reintroduced inside the EnvSpec semantic cut.
    if (!contains_exact_subsequence(
            words,
            expected_envspec_cut) ||
        !contains_exact_subsequence(
            words,
            preserved_envdiffuse))
        return false;

    return
        terminal_rgb_sat_exact(words) &&
        adjacent_pair_count(words, 0u, 7u) == 0u &&
        adjacent_pair_count(words, 0u, 8u) == 0u &&
        adjacent_pair_count(words, 13u, 6u) == 1u &&
        adjacent_pair_count(words, 13u, 7u) == 2u &&
        sample_count(words, 9u) == 0u &&
        sample_count(words, 10u) == 1u &&
        sample_count(words, 11u) == 1u &&
        sample_count(words, 12u) == 1u &&
        sample_count(words, 13u) == 1u &&
        sample_count(words, 14u) == 1u;
}

} // namespace

pmetal_rgba_lerp_materialize_outcome
materialize_pmetal_rgba_lerp_receiver(
    const std::uint8_t *stock_source,
    std::size_t stock_size,
    std::vector<std::uint8_t> &output) noexcept
{
    pmetal_rgba_lerp_materialize_outcome outcome{};
    output.clear();

    if (stock_source == nullptr || stock_size == 0u) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::fail_invalid_dxbc;
        return outcome;
    }

    // A1 composition is deliberately not inferred for this hidden family.
    // If a future exact A1 plan starts matching one of these stock shaders,
    // this materializer fails open until overlap ordering is independently
    // audited.
    if (legacy_plan::find_a1_plan_by_exact_digest(
            stock_size,
            hashing::sha256(stock_source, stock_size)) != nullptr) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::fail_a1_overlap;
        return outcome;
    }

    std::vector<std::uint8_t> v211;
    const auto mr =
        material_response::materialize_hemenvlerp_v211_certified_stage(
            stock_source,
            stock_size,
            v211);

    using mr_result =
        material_response::hemenvlerp_v211_result;

    if (mr.result == mr_result::pass_not_candidate) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::pass_not_candidate;
        return outcome;
    }

    if (mr.result == mr_result::pass_unknown_exact_sha) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::pass_unknown_exact_sha;
        return outcome;
    }

    if (mr.result != mr_result::applied) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::fail_v211_stage;
        return outcome;
    }

    const auto v211_digest =
        hashing::sha256(v211.data(), v211.size());

    const generated_lerp::pmetal_hemenvlerp_site *site = nullptr;
    for (const auto &candidate :
         generated_lerp::k_pmetal_hemenvlerp_sites) {
        if (!hashing::matches_hex(
                v211_digest,
                candidate.v211_sha256))
            continue;

        if (site != nullptr) {
            outcome.result =
                pmetal_rgba_lerp_materialize_result::
                    pass_unknown_exact_sha;
            return outcome;
        }

        site = &candidate;
    }

    if (site == nullptr) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::pass_unknown_exact_sha;
        return outcome;
    }

    if (site->pair_index != mr.pair_index ||
        site->semantic_receiver_id !=
            24u + static_cast<std::uint32_t>(mr.pair_index)) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::
                fail_operator_precondition;
        return outcome;
    }

    outcome.pair_index = site->pair_index;
    outcome.semantic_receiver_id =
        site->semantic_receiver_id;

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index = 0u;

    if (!parse_dxbc(
            v211.data(),
            v211.size(),
            chunks,
            code_index,
            words)) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::fail_invalid_dxbc;
        return outcome;
    }

    if (site->reflection_coord_register < 5u ||
        site->reflection_coord_register > 7u ||
        site->postblend_word != site->t12_word + 55u ||
        site->t9_word != site->t12_word + 93u ||
        site->t11_word != site->t12_word + 129u ||
        site->merge_word != site->t12_word + 192u ||
        site->t12_word + k_ptde_rgba_envspec_chain.size() >
            site->t11_word ||
        site->merge_word + 9u > words.size() ||
        !sample_at(words, site->t12_word, 12u) ||
        !sample_at(words, site->t14_word, 14u) ||
        !sample_at(words, site->t9_word, 9u) ||
        !sample_at(words, site->t11_word, 11u) ||
        !sample_at(words, site->t13_word, 13u) ||
        !opcode_at(words, site->mul_a_word, 0x38u, 8u) ||
        !opcode_at(words, site->mad_b_minus_a_word, 0x32u, 11u) ||
        !opcode_at(words, site->mad_lerp_word, 0x32u, 10u) ||
        !opcode_at(words, site->merge_word, 0x32u, 9u)) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::
                fail_operator_precondition;
        return outcome;
    }

    const std::array<std::pair<std::uint32_t,std::uint32_t>,3>
        upper_lower_patches{{
            {site->ul_u_slot_word, 7u},
            {site->ul_d_slot_word_0, 8u},
            {site->ul_d_slot_word_1, 8u}
        }};

    for (const auto &[slot_word, source_register] :
         upper_lower_patches) {
        if (slot_word + 1u >= words.size() ||
            words[slot_word] != 0u ||
            words[slot_word + 1u] != source_register) {
            outcome.result =
                pmetal_rgba_lerp_materialize_result::
                    fail_upper_lower_consumer;
            return outcome;
        }
    }

    std::vector<std::uint32_t> preserved_envdiffuse;
    try {
        preserved_envdiffuse.assign(
            words.begin() + static_cast<std::ptrdiff_t>(site->t11_word),
            words.begin() + static_cast<std::ptrdiff_t>(
                site->merge_word + 9u));
    } catch (...) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::fail_rebuild;
        return outcome;
    }

    auto chain = k_ptde_rgba_envspec_chain;
    chain[6] = site->reflection_coord_register;
    chain[38] = site->reflection_coord_register;

    std::copy(
        chain.begin(),
        chain.end(),
        words.begin() + static_cast<std::ptrdiff_t>(site->t12_word));

    std::fill(
        words.begin() + static_cast<std::ptrdiff_t>(
            site->t12_word + chain.size()),
        words.begin() + static_cast<std::ptrdiff_t>(site->t11_word),
        0x0100003au);

    // The independent EnvDiffuse A/B+beta block and its common merge are
    // outside the responsible EnvSpec cut and must remain byte-identical.
    if (!std::equal(
            preserved_envdiffuse.begin(),
            preserved_envdiffuse.end(),
            words.begin() + static_cast<std::ptrdiff_t>(
                site->t11_word))) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::
                fail_operator_precondition;
        return outcome;
    }

    // Upper/Lower is an independent operator, but the exact HemEnvLerp
    // consumer executes it inside the same pixel shader. Rebind only its
    // three verified b0[7]/b0[8] operands to the existing b13 PTDE carrier.
    for (const auto &[slot_word, source_register] :
         upper_lower_patches) {
        words[slot_word] = 13u;
        words[slot_word + 1u] =
            source_register == 7u
                ? 6u
                : 7u;
    }

    // PTDE Phn HemEnv/HemEnvLerp terminates with RGB-only SAT. This is a
    // separate surface operator composed into the exact P_Metal replacement;
    // alpha is untouched and any non-unique/future output shape fails open.
    if (!apply_exact_terminal_rgb_sat(
            words,
            *site)) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::
                fail_terminal_sat;
        return outcome;
    }

    // V2.11 already inserted b12 at words 11..14. Keep that ABI fixed
    // and place the existing eight-lane PTDE LightBank carrier immediately
    // after it, so all verified instruction sites shift uniformly by 4.
    if (words.size() < 15u ||
        words[11] != 0x04000059u ||
        words[12] != 0x00208e46u ||
        words[13] != 12u ||
        words[14] != 4u) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::
                fail_upper_lower_consumer;
        return outcome;
    }

    try {
        words.insert(
            words.begin() + 15,
            k_cb13_decl.begin(),
            k_cb13_decl.end());
    } catch (...) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::
                fail_rebuild;
        return outcome;
    }
    words[1] +=
        static_cast<std::uint32_t>(
            k_cb13_decl.size());

    std::vector<std::uint8_t> envspec_base;
    if (!add_bridge_rdef(
            v211.data(),
            v211.size(),
            std::move(chunks),
            code_index,
            words,
            envspec_base)) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::fail_b12_rdef;
        return outcome;
    }

    std::vector<std::uint8_t> spec_rgb_base;
    if (resource_bridges::materialize_spec_rgb_consumer(
            envspec_base.data(),
            envspec_base.size(),
            spec_rgb_base) !=
        resource_bridges::spec_rgb_consumer_result::applied) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::
                fail_spec_rgb_consumer;
        return outcome;
    }

    if (!final_postcondition(
            spec_rgb_base,
            *site,
            preserved_envdiffuse)) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::fail_postcondition;
        return outcome;
    }

    outcome.envdiffuse_preserved = true;
    outcome.upper_lower_composed = true;
    outcome.terminal_sat_rgb_composed = true;
    outcome.spec_rgb_consumer = true;
    output = std::move(spec_rgb_base);
    outcome.result =
        pmetal_rgba_lerp_materialize_result::applied;
    return outcome;
}

} // namespace dsrrl::operators::env_spec
