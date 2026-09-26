#include "dsrrl/operators/env_spec/pmetal_rgba_materializer.hpp"

#include "dsrrl/operators/env_spec/pmetal_rgba_authority.hpp"
#include "dsrrl/operators/lightbank/upper_lower_hemenv_materializer.hpp"
#include "dsrrl/operators/material_response/material_response_v211_materializer.hpp"
#include "dsrrl/operators/resource_bridges/spec_rgb_consumer_materializer.hpp"
#include "dsrrl/operators/legacy_plan/a1_create_time_materializer.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_rdef_patch.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace dsrrl::operators::env_spec {
namespace {

namespace hashing =
    legacy_plan::hashing;
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

bool parse_dxbc(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<chunk> &chunks,
    std::size_t &code_index) noexcept
{
    if (!legacy_plan::dxbc::
            checksum_container_valid(
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

    return
        words.size() >= 2u &&
        words[1] == words.size();
}

bool decode(
    const std::vector<std::uint32_t> &words,
    std::vector<instruction_view> &out) noexcept
{
    out.clear();

    if (words.size() < 2u)
        return false;

    std::size_t i = 2u;

    while (i < words.size()) {
        const auto length =
            static_cast<std::size_t>(
                (words[i] >> 24u) &
                0x7fu);

        if (length == 0u ||
            i + length > words.size())
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

bool rebuild(
    const std::uint8_t *source,
    std::size_t source_size,
    std::vector<chunk> chunks,
    std::size_t code_index,
    const std::vector<std::uint32_t> &words,
    std::vector<std::uint8_t> &out) noexcept
{
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
                code.data() + i * 4u,
                words[i]);

        const auto header_size =
            32u + 4u * chunks.size();

        if (source_size < header_size ||
            header_size >
                std::numeric_limits<
                    std::uint32_t>::max())
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

std::size_t code_payload_offset(
    const std::uint8_t *source,
    std::size_t size) noexcept
{
    if (source == nullptr ||
        size < 32u)
        return
            static_cast<std::size_t>(-1);

    const auto count =
        read_u32(source + 28u);

    if (count == 0u ||
        count > 64u ||
        32ull + 4ull * count >
            size)
        return
            static_cast<std::size_t>(-1);

    std::size_t hit =
        static_cast<std::size_t>(-1);

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

bool compose_a1(
    const core::feature_registry &features,
    const std::uint8_t *stock,
    std::size_t stock_size,
    std::vector<std::uint8_t> &v211,
    const pmetal_rgba_authority::entry &authority,
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
    const auto v211_code =
        code_payload_offset(
            v211.data(),
            v211.size());

    if (stock_code ==
            static_cast<std::size_t>(-1) ||
        v211_code ==
            static_cast<std::size_t>(-1))
        return false;

    for (std::size_t i = 0u;
         i < plan->op_count;
         ++i) {
        const auto &op =
            legacy_plan::generated::
                k_a1_exact_patch_ops_v1[
                    plan->first_op + i];

        if (!features.enabled(
                op.owner))
            continue;

        if (op.byte_offset <
                stock_code ||
            ((op.byte_offset -
              stock_code) & 3u) != 0u)
            return false;

        std::size_t word =
            (op.byte_offset -
             stock_code) / 4u;

        if (word >= 11u)
            word += 4u;

        // Build131 owns this whole semantic island. Never silently overwrite
        // an enabled create-time operator that lands inside the 100-word cut.
        if (word >=
                authority.t12_word &&
            word <
                authority.t12_word +
                100u)
            return false;

        const auto target =
            v211_code +
            word * 4u;

        if (target >
                v211.size() ||
            v211.size() - target <
                4u ||
            read_u32(
                v211.data() +
                target) !=
                op.expected_old_word)
            return false;

        write_u32(
            v211.data() + target,
            op.replacement_word);

        owners |=
            core::operator_bit(
                op.owner);
    }

    return
        legacy_plan::dxbc::
            fix_checksum(
                v211.data(),
                v211.size());
}

chunk *unique_rdef(
    std::vector<chunk> &chunks) noexcept
{
    chunk *rdef = nullptr;

    for (auto &current :
         chunks) {
        if (std::memcmp(
                current.tag.data(),
                "RDEF",
                4u) != 0)
            continue;

        if (rdef != nullptr)
            return nullptr;

        rdef = &current;
    }

    return rdef;
}

bool patch_build131_rdef(
    std::vector<chunk> &chunks) noexcept
{
    auto *rdef =
        unique_rdef(chunks);

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

    std::uint32_t sampler9 = 0u;
    std::uint32_t texture9 = 0u;

    for (std::uint32_t i = 0u;
         i < count;
         ++i) {
        const auto at =
            offset +
            i * k_binding_size;

        const auto type =
            read_u32(
                payload.data() +
                at + 4u);
        const auto dimension =
            read_u32(
                payload.data() +
                at + 12u);
        const auto bind =
            read_u32(
                payload.data() +
                at + 20u);
        const auto bind_count =
            read_u32(
                payload.data() +
                at + 24u);

        if (bind == 14u)
            return false;

        if (bind != 9u ||
            bind_count != 1u)
            continue;

        if (type == 3u) {
            write_u32(
                payload.data() +
                    at + 20u,
                14u);
            ++sampler9;
        } else if (
            type == 2u &&
            dimension == 4u) {
            write_u32(
                payload.data() +
                    at + 12u,
                9u);
            write_u32(
                payload.data() +
                    at + 20u,
                14u);
            ++texture9;
        }
    }

    return
        sampler9 == 1u &&
        texture9 == 1u;
}

bool add_b12_rdef(
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

    auto *rdef =
        unique_rdef(chunks);

    if (rdef == nullptr ||
        legacy_plan::dxbc::rdef::
            has_constant_buffer_binding(
                rdef->payload,
                12u) ||
        !legacy_plan::dxbc::rdef::
            append_constant_buffer_binding(
                rdef->payload,
                "DSRRL_MaterialCarrier",
                12u,
                64u))
        return false;

    std::vector<std::uint8_t>
        rebuilt;

    if (!rebuild(
            bytes.data(),
            bytes.size(),
            std::move(chunks),
            code_index,
            words,
            rebuilt))
        return false;

    bytes =
        std::move(rebuilt);
    return true;
}

bool patch_spec_rgb_rdef(
    std::vector<chunk> &chunks) noexcept
{
    auto *rdef =
        unique_rdef(chunks);

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

        const auto new_table_offset =
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
            t10.data() + 0u,
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
            new_table_offset);
    } catch (...) {
        return false;
    }

    return true;
}

bool apply_spec_rgb_consumer(
    std::vector<std::uint8_t> &bytes) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view>
        instructions;
    std::size_t code_index = 0u;

    if (!parse_dxbc(
            bytes.data(),
            bytes.size(),
            chunks,
            code_index) ||
        !extract_words(
            chunks,
            code_index,
            words) ||
        !decode(
            words,
            instructions))
        return false;

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
                return false;
            t1_decl = ins;
        }

        if (ins.opcode >= 0x45u &&
            ins.opcode <= 0x4au &&
            ins.length == 11u &&
            words[ins.offset + 8u] ==
                1u) {
            if (t1_sample)
                return false;
            t1_sample = ins;
        }
    }

    if (!t1_decl ||
        !t1_sample ||
        t1_sample->offset <=
            t1_decl->offset ||
        words[t1_sample->offset + 3u] <
            0x10u)
        return false;

    std::array<std::uint32_t,4>
        decl{};
    std::copy_n(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                t1_decl->offset),
        4u,
        decl.begin());
    decl[2] = 10u;

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
        decl.begin(),
        decl.end());

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

    if (!patch_spec_rgb_rdef(
            chunks))
        return false;

    std::vector<std::uint8_t>
        rebuilt;

    if (!rebuild(
            bytes.data(),
            bytes.size(),
            std::move(chunks),
            code_index,
            words,
            rebuilt))
        return false;

    bytes =
        std::move(rebuilt);
    return true;
}

constexpr std::array<std::uint32_t,100>
    k_build131_window = {{
        0x8d000048u,0x80000182u,0x00155543u,0x001000f2u,0x00000001u,0x00100796u,0x00000000u,0x00107936u,
        0x0000000cu,0x00106000u,0x0000000cu,0x00004001u,0x00000000u,0x0700000eu,0x001000e2u,0x00000001u,
        0x00100e56u,0x00000001u,0x00100006u,0x00000001u,0x08000038u,0x001000e2u,0x00000001u,0x00100e56u,
        0x00000001u,0x00208246u,0x0000000cu,0x00000002u,0x0404001fu,0x0020803au,0x0000000cu,0x00000003u,
        0x8d000048u,0x80000182u,0x00155543u,0x001000f2u,0x0000000cu,0x00100796u,0x00000000u,0x00107936u,
        0x0000000eu,0x00106000u,0x0000000eu,0x00004001u,0x00000000u,0x0700000eu,0x001000e2u,0x0000000cu,
        0x00100e56u,0x0000000cu,0x00100006u,0x0000000cu,0x0b000032u,0x001000e2u,0x0000000cu,0x00100e56u,
        0x0000000cu,0x00208246u,0x0000000cu,0x00000003u,0x80100e56u,0x00000041u,0x00000001u,0x0a000032u,
        0x001000e2u,0x00000001u,0x00100e56u,0x0000000cu,0x0020803au,0x0000000cu,0x00000003u,0x00100e56u,
        0x00000001u,0x01000015u,0x0100003au,0x0100003au,0x0100003au,0x0100003au,0x0100003au,0x0100003au,
        0x0100003au,0x0100003au,0x0100003au,0x0100003au,0x0100003au,0x0100003au,0x0100003au,0x0100003au,
        0x0100003au,0x0100003au,0x0100003au,0x0100003au,0x0100003au,0x0100003au,0x0100003au,0x0100003au,
        0x0100003au,0x0100003au,0x0100003au,0x0100003au
    }};

bool apply_build131(
    std::vector<std::uint8_t> &bytes,
    const pmetal_rgba_authority::entry
        &authority) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view>
        instructions;
    std::size_t code_index = 0u;

    if (!parse_dxbc(
            bytes.data(),
            bytes.size(),
            chunks,
            code_index) ||
        !extract_words(
            chunks,
            code_index,
            words) ||
        !decode(
            words,
            instructions))
        return false;

    std::optional<instruction_view>
        sampler9;
    std::optional<instruction_view>
        texture9;
    std::optional<instruction_view>
        t12_sample;
    std::size_t t9_samples = 0u;
    std::size_t slot14_decls = 0u;

    for (const auto &ins :
         instructions) {
        if (ins.opcode == 0x5au &&
            ins.length == 3u) {
            const auto slot =
                words[ins.offset + 2u];

            if (slot == 9u) {
                if (sampler9)
                    return false;
                sampler9 = ins;
            }

            if (slot == 14u)
                ++slot14_decls;
        }

        if (ins.opcode == 0x58u &&
            ins.length == 4u) {
            const auto slot =
                words[ins.offset + 2u];

            if (slot == 9u) {
                if (texture9)
                    return false;
                texture9 = ins;
            }

            if (slot == 14u)
                ++slot14_decls;
        }

        if (ins.opcode >= 0x45u &&
            ins.opcode <= 0x4au &&
            ins.length == 13u) {
            const auto resource =
                words[ins.offset + 8u];
            const auto sampler =
                words[ins.offset + 10u];

            if (resource == 12u &&
                sampler == 12u) {
                if (t12_sample)
                    return false;
                t12_sample = ins;
            }

            if (resource == 9u ||
                sampler == 9u)
                ++t9_samples;
        }
    }

    constexpr std::array<std::uint32_t,13>
        k_t12_sample = {{
            0x8d000048u,0x80000182u,0x00155543u,0x001000e2u,
            0x00000001u,0x00100796u,0x00000001u,0x00107936u,
            0x0000000cu,0x00106000u,0x0000000cu,0x0010003au,
            0x00000002u
        }};

    if (!sampler9 ||
        !texture9 ||
        !t12_sample ||
        slot14_decls != 0u ||
        t9_samples != 1u ||
        t12_sample->offset !=
            authority.t12_word ||
        authority.merge_word <=
            authority.t12_word ||
        authority.merge_word -
            authority.t12_word !=
            115u ||
        authority.t12_word +
            k_build131_window.size() >
            words.size() ||
        !std::equal(
            k_t12_sample.begin(),
            k_t12_sample.end(),
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    t12_sample->offset)))
        return false;

    const auto merge =
        std::find_if(
            instructions.begin(),
            instructions.end(),
            [&](const instruction_view &i) {
                return i.offset ==
                    authority.merge_word;
            });

    if (merge ==
            instructions.end() ||
        merge->opcode != 0x32u ||
        merge->length != 9u ||
        words[sampler9->offset] !=
            0x0300005au ||
        words[sampler9->offset + 1u] !=
            0x00106000u ||
        words[sampler9->offset + 2u] !=
            9u ||
        words[texture9->offset] !=
            0x04001858u ||
        words[texture9->offset + 1u] !=
            0x00107000u ||
        words[texture9->offset + 2u] !=
            9u ||
        words[texture9->offset + 3u] !=
            0x00005555u)
        return false;

    for (const auto &ins :
         instructions) {
        if (ins.opcode < 0x45u ||
            ins.opcode > 0x4au ||
            ins.length != 13u)
            continue;

        const auto resource =
            words[ins.offset + 8u];
        const auto sampler =
            words[ins.offset + 10u];

        if ((resource == 9u ||
             sampler == 9u) &&
            (ins.offset <
                 authority.t12_word ||
             ins.offset >=
                 authority.t12_word +
                 100u))
            return false;
    }

    auto window =
        k_build131_window;

    window[6] =
        authority.reflection_coord_register;
    window[38] =
        authority.reflection_coord_register;

    words[sampler9->offset + 2u] =
        14u;
    words[texture9->offset] =
        0x04003058u;
    words[texture9->offset + 2u] =
        14u;

    std::copy(
        window.begin(),
        window.end(),
        words.begin() +
            static_cast<std::ptrdiff_t>(
                authority.t12_word));

    if (!patch_build131_rdef(
            chunks))
        return false;

    std::vector<std::uint8_t>
        rebuilt;

    if (!rebuild(
            bytes.data(),
            bytes.size(),
            std::move(chunks),
            code_index,
            words,
            rebuilt))
        return false;

    bytes =
        std::move(rebuilt);
    return true;
}

bool binding_exists(
    const std::vector<std::uint8_t> &payload,
    std::uint32_t type,
    std::uint32_t bind_point) noexcept
{
    if (payload.size() < 16u)
        return false;

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

    for (std::uint32_t i = 0u;
         i < count;
         ++i) {
        const auto at =
            offset +
            i * k_binding_size;

        if (read_u32(
                payload.data() +
                at + 4u) == type &&
            read_u32(
                payload.data() +
                at + 20u) ==
                bind_point &&
            read_u32(
                payload.data() +
                at + 24u) != 0u)
            return true;
    }

    return false;
}

bool final_postcondition(
    const std::vector<std::uint8_t> &bytes,
    bool with_upper_lower) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view>
        instructions;
    std::size_t code_index = 0u;

    if (!parse_dxbc(
            bytes.data(),
            bytes.size(),
            chunks,
            code_index) ||
        !extract_words(
            chunks,
            code_index,
            words) ||
        !decode(
            words,
            instructions))
        return false;

    std::size_t t10_decl = 0u;
    std::size_t t10_sample = 0u;
    std::size_t t14_decl = 0u;
    std::size_t s14_decl = 0u;
    std::size_t t14_sample = 0u;
    std::size_t t9_sample = 0u;
    std::size_t b13_decl = 0u;

    for (const auto &ins :
         instructions) {
        if (ins.opcode == 0x58u &&
            ins.length == 4u) {
            const auto slot =
                words[ins.offset + 2u];

            if (slot == 10u)
                ++t10_decl;

            if (slot == 14u &&
                words[ins.offset] ==
                    0x04003058u)
                ++t14_decl;
        }

        if (ins.opcode == 0x5au &&
            ins.length == 3u &&
            words[ins.offset + 2u] ==
                14u)
            ++s14_decl;

        if (ins.opcode == 0x59u &&
            ins.length == 4u &&
            words[ins.offset + 2u] ==
                13u &&
            words[ins.offset + 3u] ==
                8u)
            ++b13_decl;

        if (ins.opcode >= 0x45u &&
            ins.opcode <= 0x4au) {
            if (ins.length == 11u &&
                words[ins.offset + 8u] ==
                    10u)
                ++t10_sample;

            if (ins.length == 13u) {
                const auto resource =
                    words[ins.offset + 8u];
                const auto sampler =
                    words[ins.offset + 10u];

                if (resource == 14u &&
                    sampler == 14u)
                    ++t14_sample;

                if (resource == 9u ||
                    sampler == 9u)
                    ++t9_sample;
            }
        }
    }

    auto *rdef =
        unique_rdef(chunks);

    if (rdef == nullptr ||
        !legacy_plan::dxbc::rdef::
            has_constant_buffer_binding(
                rdef->payload,
                12u) ||
        !binding_exists(
            rdef->payload,
            2u,
            10u) ||
        !binding_exists(
            rdef->payload,
            2u,
            14u) ||
        !binding_exists(
            rdef->payload,
            3u,
            14u))
        return false;

    if (with_upper_lower) {
        if (!legacy_plan::dxbc::rdef::
                has_constant_buffer_binding(
                    rdef->payload,
                    13u) ||
            b13_decl != 1u)
            return false;
    } else if (b13_decl != 0u) {
        return false;
    }

    return
        t10_decl == 1u &&
        t10_sample == 1u &&
        t14_decl == 1u &&
        s14_decl == 1u &&
        t14_sample == 1u &&
        t9_sample == 0u;
}

} // namespace

pmetal_rgba_materialize_outcome
materialize_pmetal_rgba_receiver(
    const core::feature_registry &features,
    const std::uint8_t *stock_source,
    std::size_t stock_size,
    bool compose_upper_lower,
    std::vector<std::uint8_t> &output) noexcept
{
    pmetal_rgba_materialize_outcome
        outcome{};
    output.clear();

    if (stock_source == nullptr ||
        stock_size == 0u) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_invalid_dxbc;
        return outcome;
    }

    std::vector<std::uint8_t> v211;
    const auto mr =
        material_response::
            materialize_v211_certified_stage(
                stock_source,
                stock_size,
                v211);

    using mr_result =
        material_response::
            v211_materialize_result;

    if (mr.result ==
            mr_result::pass_not_candidate ||
        mr.result ==
            mr_result::
                pass_unknown_exact_sha) {
        outcome.result =
            pmetal_rgba_materialize_result::
                pass_not_candidate;
        return outcome;
    }

    if (mr.result !=
            mr_result::applied) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_v211_stage;
        return outcome;
    }

    const auto digest =
        hashing::sha256(
            v211.data(),
            v211.size());

    const pmetal_rgba_authority::entry
        *authority = nullptr;

    for (const auto &candidate :
         pmetal_rgba_authority::
             k_entries) {
        if (hashing::matches_hex(
                digest,
                candidate.input_v211_sha256)) {
            authority = &candidate;
            break;
        }
    }

    if (authority == nullptr ||
        authority->receiver_id !=
            mr.receiver_id) {
        outcome.result =
            pmetal_rgba_materialize_result::
                pass_unknown_exact_sha;
        return outcome;
    }

    outcome.receiver_id =
        mr.receiver_id;

    if (!compose_a1(
            features,
            stock_source,
            stock_size,
            v211,
            *authority,
            outcome.composed_owners)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_a1_composition;
        return outcome;
    }

    if (!apply_build131(
            v211,
            *authority)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_build131_precondition;
        return outcome;
    }

    if (!add_b12_rdef(v211)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_b12_rdef;
        return outcome;
    }

    std::vector<std::uint8_t> base =
        std::move(v211);

    if (compose_upper_lower) {
        std::vector<std::uint8_t> ul;
        const auto ul_result =
            lightbank::
                augment_upper_lower_hemenv_verified_base(
                    stock_source,
                    stock_size,
                    base.data(),
                    base.size(),
                    4u,
                    ul);

        if (ul_result.result !=
                lightbank::
                    upper_lower_hemenv_materialize_result::
                        applied ||
            ul_result.stratum !=
                lightbank::
                    upper_lower_hemenv_stratum::
                        spc ||
            ul_result.stable_receiver_id !=
                outcome.receiver_id) {
            outcome.result =
                pmetal_rgba_materialize_result::
                    fail_upper_lower_composition;
            return outcome;
        }

        base =
            std::move(ul);
        outcome.upper_lower_composed =
            true;
    }

    std::vector<std::uint8_t>
        spec_rgb_base;

    if (resource_bridges::
            materialize_spec_rgb_consumer(
                base.data(),
                base.size(),
                spec_rgb_base) !=
        resource_bridges::
            spec_rgb_consumer_result::applied) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_spec_rgb_consumer;
        return outcome;
    }

    base =
        std::move(spec_rgb_base);
    outcome.spec_rgb_consumer =
        true;

    if (!final_postcondition(
            base,
            compose_upper_lower)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_postcondition;
        return outcome;
    }

    output =
        std::move(base);
    outcome.result =
        pmetal_rgba_materialize_result::
            applied;
    return outcome;
}

} // namespace dsrrl::operators::env_spec
