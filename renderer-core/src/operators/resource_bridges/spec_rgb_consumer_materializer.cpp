#include "dsrrl/operators/resource_bridges/spec_rgb_consumer_materializer.hpp"

#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"

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

struct chunk {
    std::array<char,4> tag{};
    std::vector<std::uint8_t> payload;
};

struct instruction_view {
    std::size_t offset = 0;
    std::uint32_t opcode = 0;
    std::size_t length = 0;
};

bool parse(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<chunk> &chunks,
    std::size_t &code_index,
    std::vector<std::uint32_t> &words) noexcept
{
    if (!legacy_plan::dxbc::checksum_container_valid(
            source,
            size) ||
        size < 32u)
        return false;

    const auto count =
        read_u32(source + 28u);

    if (count == 0u ||
        count > 64u ||
        32ull + 4ull * count > size)
        return false;

    chunks.clear();
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
            read_u32(
                source + off + 4u);

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
                static_cast<std::size_t>(-1))
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

    return
        words.size() >= 2u &&
        words[1] == words.size();
}

bool decode(
    const std::vector<std::uint32_t> &words,
    std::vector<instruction_view> &out) noexcept
{
    out.clear();

    std::size_t i = 2u;
    while (i < words.size()) {
        const auto length =
            static_cast<std::size_t>(
                (words[i] >> 24u) &
                0x7fu);

        if (length == 0u ||
            i + length >
                words.size())
            return false;

        out.push_back({
            i,
            words[i] & 0x7ffu,
            length
        });

        i += length;
    }

    return i == words.size();
}

bool patch_rdef(
    std::vector<chunk> &chunks) noexcept
{
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
        rdef->payload.size() <
            16u)
        return false;

    auto &payload =
        rdef->payload;

    const auto count =
        read_u32(
            payload.data() + 8u);
    const auto offset =
        read_u32(
            payload.data() + 12u);

    constexpr std::uint32_t
        k_binding_size = 32u;

    if (count == 0u ||
        count > 256u ||
        offset > payload.size() ||
        static_cast<std::uint64_t>(
            count) *
            k_binding_size >
        payload.size() - offset)
        return false;

    std::size_t t1_at = 0u;
    std::uint32_t t1_count = 0u;

    for (std::uint32_t i = 0u;
         i < count;
         ++i) {
        const auto at =
            static_cast<std::size_t>(
                offset) +
            static_cast<std::size_t>(i) *
                k_binding_size;

        const auto type =
            read_u32(
                payload.data() +
                at + 4u);
        const auto bind =
            read_u32(
                payload.data() +
                at + 20u);
        const auto bind_count =
            read_u32(
                payload.data() +
                at + 24u);

        if (type == 2u &&
            bind == 10u)
            return false;

        if (type == 2u &&
            bind == 1u &&
            bind_count == 1u) {
            t1_at = at;
            ++t1_count;
        }
    }

    if (t1_count != 1u)
        return false;

    try {
        static constexpr char
            k_name[] =
                "DSRRL_PTDE_SpecRGB";

        const auto name_offset =
            static_cast<std::uint32_t>(
                payload.size());

        payload.insert(
            payload.end(),
            reinterpret_cast<
                const std::uint8_t *>(
                    k_name),
            reinterpret_cast<
                const std::uint8_t *>(
                    k_name) +
                sizeof(k_name));

        while ((payload.size() &
                3u) != 0u)
            payload.push_back(0u);

        const auto new_table =
            static_cast<std::uint32_t>(
                payload.size());

        std::vector<std::uint8_t>
            table(
                payload.data() + offset,
                payload.data() + offset +
                    static_cast<std::size_t>(
                        count) *
                    k_binding_size);

        std::array<
            std::uint8_t,
            k_binding_size> t10{};

        std::memcpy(
            t10.data(),
            payload.data() + t1_at,
            k_binding_size);

        write_u32(
            t10.data(),
            name_offset);
        write_u32(
            t10.data() + 20u,
            10u);

        table.insert(
            table.end(),
            t10.begin(),
            t10.end());

        payload.insert(
            payload.end(),
            table.begin(),
            table.end());

        write_u32(
            payload.data() + 8u,
            count + 1u);
        write_u32(
            payload.data() + 12u,
            new_table);
    } catch (...) {
        return false;
    }

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

        const auto header_size =
            32u + 4u * chunks.size();

        if (source_size < header_size)
            return false;

        out.assign(
            source,
            source + header_size);

        std::vector<std::uint32_t>
            offsets;
        offsets.reserve(
            chunks.size());

        for (const auto &current :
             chunks) {
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

            const auto size_at =
                out.size();
            out.resize(
                size_at + 4u);

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
            legacy_plan::dxbc::
                fix_checksum(
                    out.data(),
                    out.size());
    } catch (...) {
        out.clear();
        return false;
    }
}

bool postcondition(
    const std::uint8_t *source,
    std::size_t size) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view>
        instructions;
    std::size_t code = 0u;

    if (!parse(
            source,
            size,
            chunks,
            code,
            words) ||
        !decode(
            words,
            instructions))
        return false;

    std::size_t dcl = 0u;
    std::size_t sample = 0u;

    for (const auto &ins :
         instructions) {
        if (ins.opcode == 0x58u &&
            ins.length == 4u &&
            words[ins.offset + 2u] ==
                10u)
            ++dcl;

        if (ins.opcode >= 0x45u &&
            ins.opcode <= 0x4au &&
            ins.length == 11u &&
            words[ins.offset + 8u] ==
                10u)
            ++sample;
    }

    return
        dcl == 1u &&
        sample == 1u;
}

} // namespace

spec_rgb_consumer_result
materialize_spec_rgb_consumer(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept
{
    output.clear();

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view>
        instructions;
    std::size_t code_index = 0u;

    if (!parse(
            source,
            size,
            chunks,
            code_index,
            words) ||
        !decode(
            words,
            instructions))
        return
            spec_rgb_consumer_result::
                fail_invalid_dxbc;

    std::optional<instruction_view>
        t1_decl;
    std::optional<instruction_view>
        t1_sample;

    for (const auto &ins :
         instructions) {
        if (ins.opcode == 0x58u &&
            ins.length == 4u &&
            words[ins.offset + 2u] ==
                1u) {
            if (t1_decl)
                return
                    spec_rgb_consumer_result::
                        fail_declaration_identity;
            t1_decl = ins;
        }

        if (ins.opcode >= 0x45u &&
            ins.opcode <= 0x4au &&
            ins.length == 11u &&
            words[ins.offset + 8u] ==
                1u) {
            if (t1_sample)
                return
                    spec_rgb_consumer_result::
                        fail_sample_identity;
            t1_sample = ins;
        }
    }

    if (!t1_decl ||
        !t1_sample ||
        t1_sample->offset <=
            t1_decl->offset ||
        words[t1_sample->offset + 3u] <
            0x10u)
        return
            spec_rgb_consumer_result::
                fail_sample_identity;

    std::array<std::uint32_t,4>
        dcl{};
    std::copy_n(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                t1_decl->offset),
        4u,
        dcl.begin());
    dcl[2] = 10u;

    std::array<std::uint32_t,11>
        sample{};
    std::copy_n(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                t1_sample->offset),
        11u,
        sample.begin());

    sample[3] -= 0x10u;
    sample[8] = 10u;

    words.insert(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                t1_decl->offset + 4u),
        dcl.begin(),
        dcl.end());

    const auto shifted_sample =
        t1_sample->offset + 4u;

    words.insert(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                shifted_sample + 11u),
        sample.begin(),
        sample.end());

    words[1] =
        static_cast<std::uint32_t>(
            words.size());

    if (!patch_rdef(chunks))
        return
            spec_rgb_consumer_result::
                fail_rdef;

    if (!rebuild(
            source,
            size,
            std::move(chunks),
            code_index,
            words,
            output))
        return
            spec_rgb_consumer_result::
                fail_rebuild;

    if (!postcondition(
            output.data(),
            output.size())) {
        output.clear();
        return
            spec_rgb_consumer_result::
                fail_postcondition;
    }

    return
        spec_rgb_consumer_result::applied;
}

} // namespace dsrrl::operators::resource_bridges
