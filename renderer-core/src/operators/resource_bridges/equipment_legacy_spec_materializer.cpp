#include "dsrrl/operators/resource_bridges/equipment_legacy_spec_materializer.hpp"

#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/resource_bridges/spec_rgb_consumer_materializer.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <optional>
#include <vector>

namespace dsrrl::operators::resource_bridges {
namespace {

using legacy_plan::dxbc::read_u32;
using legacy_plan::dxbc::write_u32;

constexpr std::uint32_t k_nop = 0x0100003au;
constexpr std::uint32_t k_float_1 = 0x3f800000u;
constexpr std::uint32_t k_float_1_3 = 0x3fa66666u;
constexpr std::uint32_t k_float_2_2 = 0x400ccccdu;

struct chunk {
    std::array<char,4> tag{};
    std::vector<std::uint8_t> payload;
};

struct instruction_view {
    std::size_t offset = 0u;
    std::uint32_t opcode = 0u;
    std::size_t length = 0u;
};

bool parse(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<chunk> &chunks,
    std::size_t &code_index,
    std::vector<std::uint32_t> &words) noexcept
{
    if (!legacy_plan::dxbc::checksum_container_valid(source,size) ||
        source == nullptr || size < 32u)
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
        const auto off = read_u32(source + 32u + i * 4u);
        if (off > size || size - off < 8u)
            return false;

        const auto payload_size = read_u32(source + off + 4u);
        if (payload_size > size - off - 8u)
            return false;

        chunk current{};
        std::memcpy(current.tag.data(),source + off,4u);

        try {
            current.payload.assign(
                source + off + 8u,
                source + off + 8u + payload_size);
        } catch (...) {
            return false;
        }

        const bool is_code =
            std::memcmp(current.tag.data(),"SHEX",4u) == 0 ||
            std::memcmp(current.tag.data(),"SHDR",4u) == 0;

        if (is_code) {
            if (code_index != static_cast<std::size_t>(-1))
                return false;
            code_index = chunks.size();
        }

        chunks.push_back(std::move(current));
    }

    if (code_index == static_cast<std::size_t>(-1))
        return false;

    const auto &payload = chunks[code_index].payload;
    if ((payload.size() & 3u) != 0u)
        return false;

    try {
        words.resize(payload.size() / 4u);
    } catch (...) {
        return false;
    }

    for (std::size_t i = 0u; i < words.size(); ++i)
        words[i] = read_u32(payload.data() + i * 4u);

    return words.size() >= 2u && words[1] == words.size();
}

bool decode(
    const std::vector<std::uint32_t> &words,
    std::vector<instruction_view> &out) noexcept
{
    out.clear();

    std::size_t i = 2u;
    while (i < words.size()) {
        const auto length =
            static_cast<std::size_t>((words[i] >> 24u) & 0x7fu);
        if (length == 0u || i + length > words.size())
            return false;

        out.push_back({i,words[i] & 0x7ffu,length});
        i += length;
    }

    return i == words.size();
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
            write_u32(code.data() + i * 4u,words[i]);

        const auto header_size = 32u + 4u * chunks.size();
        if (source_size < header_size)
            return false;

        out.assign(source,source + header_size);

        std::vector<std::uint32_t> offsets;
        offsets.reserve(chunks.size());

        for (const auto &current : chunks) {
            if (out.size() >
                std::numeric_limits<std::uint32_t>::max())
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

            out.insert(
                out.end(),
                current.payload.begin(),
                current.payload.end());
        }

        if (out.size() >
            std::numeric_limits<std::uint32_t>::max())
            return false;

        write_u32(out.data() + 24u,static_cast<std::uint32_t>(out.size()));
        write_u32(out.data() + 28u,static_cast<std::uint32_t>(chunks.size()));

        for (std::size_t i = 0u; i < offsets.size(); ++i)
            write_u32(out.data() + 32u + i * 4u,offsets[i]);

        std::fill(
            out.begin() + 4u,
            out.begin() + 20u,
            std::uint8_t{0});

        return legacy_plan::dxbc::fix_checksum(out.data(),out.size());
    } catch (...) {
        out.clear();
        return false;
    }
}

bool find_color0_register(
    const std::vector<chunk> &chunks,
    std::uint32_t &reg) noexcept
{
    reg = 0u;
    const chunk *isgn = nullptr;

    for (const auto &current : chunks) {
        if (std::memcmp(current.tag.data(),"ISGN",4u) != 0)
            continue;
        if (isgn != nullptr)
            return false;
        isgn = &current;
    }

    if (isgn == nullptr || isgn->payload.size() < 8u)
        return false;

    const auto count = read_u32(isgn->payload.data());
    constexpr std::size_t k_record_size = 24u;
    if (count == 0u ||
        8ull + static_cast<std::uint64_t>(count) * k_record_size >
            isgn->payload.size())
        return false;

    std::uint32_t hits = 0u;
    for (std::uint32_t i = 0u; i < count; ++i) {
        const auto at = 8u + static_cast<std::size_t>(i) * k_record_size;
        const auto name_off = read_u32(isgn->payload.data() + at);
        const auto semantic_index = read_u32(isgn->payload.data() + at + 4u);
        const auto input_reg = read_u32(isgn->payload.data() + at + 16u);

        if (name_off >= isgn->payload.size())
            return false;

        const char *name = reinterpret_cast<const char *>(
            isgn->payload.data() + name_off);
        const auto remain = isgn->payload.size() - name_off;
        const void *end = std::memchr(name,'\0',remain);
        if (end == nullptr)
            return false;

        if (semantic_index == 0u && std::strcmp(name,"COLOR") == 0) {
            reg = input_reg;
            ++hits;
        }
    }

    return hits == 1u;
}

bool has_immediate(
    const std::vector<std::uint32_t> &words,
    const instruction_view &ins,
    std::uint32_t value,
    std::size_t minimum_hits = 1u) noexcept
{
    if (ins.offset + ins.length > words.size())
        return false;

    std::size_t hits = 0u;
    for (std::size_t i = ins.offset; i < ins.offset + ins.length; ++i)
        if (words[i] == value)
            ++hits;
    return hits >= minimum_hits;
}

std::optional<std::size_t> find_t10_sample_instruction(
    const std::vector<std::uint32_t> &words,
    const std::vector<instruction_view> &instructions) noexcept
{
    std::optional<std::size_t> found;
    for (std::size_t i = 0u; i < instructions.size(); ++i) {
        const auto &ins = instructions[i];
        if (ins.opcode < 0x45u || ins.opcode > 0x4au ||
            ins.length != 11u ||
            ins.offset + 9u > words.size() ||
            words[ins.offset + 8u] != 10u)
            continue;

        if (found)
            return std::nullopt;
        found = i;
    }
    return found;
}

std::optional<std::size_t> find_t9_split_sum_instruction(
    const std::vector<std::uint32_t> &words,
    const std::vector<instruction_view> &instructions) noexcept
{
    std::optional<std::size_t> found;

    for (std::size_t i = 0u; i + 2u < instructions.size(); ++i) {
        const auto &sample = instructions[i];
        const auto &mul = instructions[i + 1u];
        const auto &mad = instructions[i + 2u];

        if (sample.opcode != 0x48u ||
            sample.length != 13u ||
            sample.offset + 11u > words.size())
            continue;

        const bool t9 =
            words[sample.offset + 8u] == 9u &&
            words[sample.offset + 10u] == 9u;
        if (!t9)
            continue;

        if (mul.opcode != 0x38u || mul.length != 7u ||
            mad.opcode != 0x32u || mad.length != 9u ||
            mul.offset != sample.offset + sample.length ||
            mad.offset != mul.offset + mul.length)
            continue;

        if (found)
            return std::nullopt;
        found = i;
    }

    return found;
}

std::optional<std::size_t> find_spec_pow_chain(
    const std::vector<std::uint32_t> &words,
    const std::vector<instruction_view> &instructions,
    std::size_t after_word,
    std::size_t before_word) noexcept
{
    std::optional<std::size_t> best;

    for (std::size_t i = 0u; i + 2u < instructions.size(); ++i) {
        const auto &log = instructions[i];
        const auto &mul = instructions[i + 1u];
        const auto &exp = instructions[i + 2u];

        if (log.offset <= after_word ||
            log.offset >= before_word ||
            log.opcode != 0x2fu ||
            log.length != 5u ||
            mul.opcode != 0x38u ||
            mul.length != 10u ||
            exp.opcode != 0x19u ||
            exp.length != 5u ||
            mul.offset != log.offset + log.length ||
            exp.offset != mul.offset + mul.length ||
            !has_immediate(words,mul,k_float_2_2,3u))
            continue;

        if (!best || log.offset < instructions[*best].offset)
            best = i;
    }

    return best;
}

std::optional<std::size_t> find_angular_chain(
    const std::vector<std::uint32_t> &words,
    const std::vector<instruction_view> &instructions,
    std::size_t after_word,
    std::size_t before_word) noexcept
{
    std::optional<std::size_t> found;

    for (std::size_t i = 0u; i + 2u < instructions.size(); ++i) {
        const auto &mad = instructions[i];
        const auto &square = instructions[i + 1u];
        const auto &apply = instructions[i + 2u];

        if (mad.offset <= after_word ||
            mad.offset >= before_word ||
            mad.opcode != 0x32u ||
            mad.length != 9u ||
            square.opcode != 0x38u ||
            square.length != 7u ||
            apply.opcode != 0x38u ||
            apply.length != 7u ||
            square.offset != mad.offset + mad.length ||
            apply.offset != square.offset + square.length ||
            !has_immediate(words,mad,k_float_1_3) ||
            !has_immediate(words,mad,k_float_1))
            continue;

        if (square.offset + 7u > words.size())
            return std::nullopt;

        // Historical V2.28B identity: scalar square is x*x in-place, and the
        // following MUL consumes that scalar. This prevents matching unrelated
        // MAD/MUL/MUL triplets elsewhere in the shader.
        if (words[square.offset + 2u] != words[square.offset + 4u] ||
            words[square.offset + 4u] != words[square.offset + 6u] ||
            words[apply.offset + 6u] != words[square.offset + 2u])
            continue;

        if (found)
            return std::nullopt;
        found = i;
    }

    return found;
}

bool postcondition(
    const std::vector<std::uint8_t> &bytes,
    std::uint32_t color0_register) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view> instructions;
    std::size_t code_index = 0u;

    if (!parse(
            bytes.data(),bytes.size(),
            chunks,code_index,words) ||
        !decode(words,instructions))
        return false;

    std::size_t t10_samples = 0u;
    std::size_t t9_samples = 0u;
    std::size_t color_mul = 0u;

    for (const auto &ins : instructions) {
        if (ins.opcode >= 0x45u && ins.opcode <= 0x4au) {
            if (ins.length == 11u &&
                words[ins.offset + 8u] == 10u)
                ++t10_samples;
            if (ins.length == 13u &&
                (words[ins.offset + 8u] == 9u ||
                 words[ins.offset + 10u] == 9u))
                ++t9_samples;
        }

        if (ins.opcode == 0x38u &&
            ins.length == 7u &&
            ins.offset + 7u <= words.size() &&
            words[ins.offset + 5u] == 0x00101246u &&
            words[ins.offset + 6u] == color0_register)
            ++color_mul;
    }

    return t10_samples == 1u &&
        t9_samples == 0u &&
        color_mul == 1u;
}

} // namespace

equipment_legacy_spec_result
materialize_equipment_legacy_spec_response(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept
{
    output.clear();

    if (source == nullptr || size == 0u)
        return equipment_legacy_spec_result::pass_not_candidate;

    // First build the exact split-resource consumer: native t1 remains
    // available for alpha/roughness, while fresh t10 supplies PTDE SpecRGB
    // and is multiplied once by raw PTDE c101 from b12[2].xyz.
    std::vector<std::uint8_t> spec_base;
    const auto spec =
        materialize_spec_rgb_consumer(
            source,size,spec_base,true);

    if (spec != spec_rgb_consumer_result::applied)
        return equipment_legacy_spec_result::fail_spec_rgb_base;

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view> instructions;
    std::size_t code_index = 0u;

    if (!parse(
            spec_base.data(),spec_base.size(),
            chunks,code_index,words) ||
        !decode(words,instructions))
        return equipment_legacy_spec_result::fail_invalid_dxbc;

    std::uint32_t color0_register = 0u;
    if (!find_color0_register(chunks,color0_register))
        return equipment_legacy_spec_result::fail_color0;

    const auto t10_index =
        find_t10_sample_instruction(words,instructions);
    const auto t9_index =
        find_t9_split_sum_instruction(words,instructions);

    if (!t10_index)
        return equipment_legacy_spec_result::fail_spec_rgb_base;
    if (!t9_index)
        return equipment_legacy_spec_result::fail_t9_split_sum;

    const auto t10_word = instructions[*t10_index].offset;
    const auto t9_word = instructions[*t9_index].offset;

    const auto spec_pow =
        find_spec_pow_chain(
            words,instructions,
            t10_word,t9_word);
    if (!spec_pow)
        return equipment_legacy_spec_result::fail_spec_pow;

    const auto angular =
        find_angular_chain(
            words,instructions,
            instructions[*spec_pow].offset,
            t9_word);
    if (!angular)
        return equipment_legacy_spec_result::fail_angular;

    // V2.28A: SPEC-domain pow(2.2) -> identity. Keep the EXP destination and
    // source operands but turn EXP into MOV; NOP the LOG+2.2 MUL.
    {
        const auto &log = instructions[*spec_pow];
        const auto &mul = instructions[*spec_pow + 1u];
        const auto &exp = instructions[*spec_pow + 2u];

        std::fill(
            words.begin() + static_cast<std::ptrdiff_t>(log.offset),
            words.begin() + static_cast<std::ptrdiff_t>(exp.offset),
            k_nop);

        words[exp.offset] =
            (words[exp.offset] & ~0x7ffu) | 0x36u;

        if (mul.offset != log.offset + log.length ||
            exp.offset != mul.offset + mul.length)
            return equipment_legacy_spec_result::fail_spec_pow;
    }

    // V2.28B: DSR angular/horizon scalar -> 1 immediately before it is
    // applied to the EnvSpec contribution.
    {
        const auto &square = instructions[*angular + 1u];
        const std::array<std::uint32_t,5> mov_one{{
            0x05000036u,
            words[square.offset + 1u],
            words[square.offset + 2u],
            0x00004001u,
            k_float_1
        }};

        std::copy(
            mov_one.begin(),mov_one.end(),
            words.begin() + static_cast<std::ptrdiff_t>(square.offset));
        words[square.offset + 5u] = k_nop;
        words[square.offset + 6u] = k_nop;
    }

    // V2.28C/F/G, but with the modern split PTDE SpecRGB resource already
    // feeding the linear material carrier: delete the t9 BRDF LUT helper and
    // replace its split-sum MAD by one COLOR0 multiplication. c101 was already
    // applied exactly once immediately after the fresh t10 sample.
    {
        const auto &sample = instructions[*t9_index];
        const auto &mul = instructions[*t9_index + 1u];
        const auto &mad = instructions[*t9_index + 2u];

        if (mad.offset + 9u > words.size())
            return equipment_legacy_spec_result::fail_t9_split_sum;

        const std::array<std::uint32_t,7> color_mul{{
            0x07000038u,
            words[mad.offset + 1u],
            words[mad.offset + 2u],
            words[mad.offset + 3u],
            words[mad.offset + 4u],
            0x00101246u,
            color0_register
        }};

        std::fill(
            words.begin() + static_cast<std::ptrdiff_t>(sample.offset),
            words.begin() + static_cast<std::ptrdiff_t>(mad.offset),
            k_nop);

        std::copy(
            color_mul.begin(),color_mul.end(),
            words.begin() + static_cast<std::ptrdiff_t>(mad.offset));
        words[mad.offset + 7u] = k_nop;
        words[mad.offset + 8u] = k_nop;

        if (mul.offset != sample.offset + sample.length ||
            mad.offset != mul.offset + mul.length)
            return equipment_legacy_spec_result::fail_t9_split_sum;
    }

    std::vector<std::uint8_t> rebuilt;
    if (!rebuild(
            spec_base.data(),spec_base.size(),
            std::move(chunks),code_index,words,rebuilt))
        return equipment_legacy_spec_result::fail_rebuild;

    if (!postcondition(rebuilt,color0_register))
        return equipment_legacy_spec_result::fail_postcondition;

    output = std::move(rebuilt);
    return equipment_legacy_spec_result::applied;
}

} // namespace dsrrl::operators::resource_bridges
