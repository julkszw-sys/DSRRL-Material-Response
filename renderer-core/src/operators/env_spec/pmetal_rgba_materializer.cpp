#include "dsrrl/operators/env_spec/pmetal_rgba_materializer.hpp"

#include "dsrrl/operators/env_spec/pmetal_rgba_authority.hpp"
#include "dsrrl/operators/lightbank/upper_lower_hemenv_materializer.hpp"
#include "dsrrl/operators/material_response/material_response_diffuse_v1.hpp"
#include "dsrrl/operators/resource_bridges/spec_rgb_consumer_materializer.hpp"
#include "dsrrl/operators/surface/terminal_sat_rgb_patch.hpp"
#include "dsrrl/operators/surface/phn_scene_encoding.hpp"
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
    std::vector<std::uint8_t> &base,
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
    const auto base_code =
        code_payload_offset(
            base.data(),
            base.size());

    if (stock_code ==
            static_cast<std::size_t>(-1) ||
        base_code ==
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

        // Build131 owns the recovered 100-DWORD semantic cut beginning
        // 38 DWORDs before the stock t12 sample. Never compose an A1 owner
        // into a region that the dedicated P_Metal island will subsequently
        // replace.
        if (authority.t12_word < 38u)
            return false;
        const auto build_start =
            authority.t12_word - 38u;
        if (word >= build_start &&
            word < build_start + 100u)
            return false;

        const auto target =
            base_code +
            word * 4u;

        if (target >
                base.size() ||
            base.size() - target <
                4u ||
            read_u32(
                base.data() +
                target) !=
                op.expected_old_word)
            return false;

        write_u32(
            base.data() + target,
            op.replacement_word);

        owners |=
            core::operator_bit(
                op.owner);
    }

    return
        legacy_plan::dxbc::
            fix_checksum(
                base.data(),
                base.size());
}

bool terminal_rgb_output_word(
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

bool terminal_rgb_sat_exact(
    const std::vector<std::uint32_t> &words) noexcept
{
    std::size_t word = 0u;
    return
        terminal_rgb_output_word(words, word) &&
        words[word] == 0x05002036u;
}

bool compose_exact_terminal_rgb_sat(
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

    std::size_t word = 0u;
    if (!terminal_rgb_output_word(words, word))
        return false;

    if (words[word] == 0x05002036u)
        return true;

    if (words[word] != 0x05000036u)
        return false;

    words[word] |=
        surface::dxbc_saturate_modifier_bit;

    if (words[word] != 0x05002036u)
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

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
constexpr surface::phn_scene_encoding_carrier
    k_pmetal_phn_scene_carrier{
        12u,
        0u,
        3u
    };

bool phn_scene_encoding_exact(
    const std::vector<std::uint32_t> &words) noexcept
{
    return surface::unique_phn_scene_encoding_exact(
        words,
        k_pmetal_phn_scene_carrier);
}

bool compose_exact_phn_scene_encoding(
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

    const auto result =
        surface::apply_unique_phn_scene_encoding_words(
            words,
            k_pmetal_phn_scene_carrier);

    if (result !=
            surface::phn_scene_encoding_result::applied &&
        result !=
            surface::phn_scene_encoding_result::already_encoded)
        return false;

    if (!phn_scene_encoding_exact(words))
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
#endif

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
        rdef->payload.size() < 16u)
        return false;

    auto &payload =
        rdef->payload;

    const auto count =
        read_u32(payload.data() + 8u);
    const auto offset =
        read_u32(payload.data() + 12u);

    constexpr std::uint32_t
        k_binding_size = 32u;

    if (count == 0u ||
        count > 256u ||
        offset > payload.size() ||
        static_cast<std::uint64_t>(count) *
                k_binding_size >
            payload.size() - offset)
        return false;

    std::uint32_t sampler9 = 0u;
    std::uint32_t texture9 = 0u;
    std::uint32_t texture1 = 0u;
    std::size_t texture1_at = 0u;

    for (std::uint32_t i = 0u;
         i < count;
         ++i) {
        const auto at =
            offset +
            i * k_binding_size;

        const auto type =
            read_u32(payload.data() + at + 4u);
        const auto dimension =
            read_u32(payload.data() + at + 12u);
        const auto bind =
            read_u32(payload.data() + at + 20u);
        const auto bind_count =
            read_u32(payload.data() + at + 24u);

        // Build131 owns t14 and the dedicated P_Metal final material tail owns
        // t10. Both must be absent on the clean base so we never compose over
        // an already-mutated receiver.
        if (bind == 14u ||
            (type == 2u && bind == 10u))
            return false;

        if (type == 2u &&
            bind == 1u &&
            bind_count == 1u) {
            texture1_at = at;
            ++texture1;
        }

        if (bind != 9u ||
            bind_count != 1u)
            continue;

        if (type == 3u) {
            write_u32(
                payload.data() + at + 20u,
                14u);
            ++sampler9;
        } else if (
            type == 2u &&
            dimension == 4u) {
            write_u32(
                payload.data() + at + 12u,
                9u);
            write_u32(
                payload.data() + at + 20u,
                14u);
            ++texture9;
        }
    }

    if (sampler9 != 1u ||
        texture9 != 1u ||
        texture1 != 1u)
        return false;

    try {
        // Preserve the already-patched table (t9/s9 -> t14/s14) and append one
        // exact clone of the stock SpecTex t1 binding at t10. The dedicated
        // final material tail samples t10 only after the Build131 EnvSpec cut.
        std::vector<std::uint8_t> table(
            payload.data() + offset,
            payload.data() + offset +
                static_cast<std::size_t>(count) *
                    k_binding_size);

        std::array<std::uint8_t,k_binding_size>
            t10{};
        std::memcpy(
            t10.data(),
            payload.data() + texture1_at,
            k_binding_size);

        static constexpr char
            k_name[] =
                "DSRRL_PTDE_SpecRGB";

        const auto name_offset =
            static_cast<std::uint32_t>(
                payload.size());

        payload.insert(
            payload.end(),
            reinterpret_cast<const std::uint8_t *>(
                k_name),
            reinterpret_cast<const std::uint8_t *>(
                k_name) + sizeof(k_name));

        while ((payload.size() & 3u) != 0u)
            payload.push_back(0u);

        const auto new_table =
            static_cast<std::uint32_t>(
                payload.size());

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


#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) || defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
bool patch_v13_native_dsr_no_tail_rdef(
    std::vector<chunk> &chunks) noexcept
{
    auto *rdef = unique_rdef(chunks);
    if (rdef == nullptr || rdef->payload.size() < 16u)
        return false;

    auto &payload = rdef->payload;
    const auto count = read_u32(payload.data() + 8u);
    const auto offset = read_u32(payload.data() + 12u);
    constexpr std::uint32_t k_binding_size = 32u;

    if (count == 0u || count > 256u ||
        offset > payload.size() ||
        static_cast<std::uint64_t>(count) * k_binding_size >
            payload.size() - offset)
        return false;

    std::uint32_t sampler9 = 0u;
    std::uint32_t texture9 = 0u;

    for (std::uint32_t i = 0u; i < count; ++i) {
        const auto at = offset + i * k_binding_size;
        const auto type = read_u32(payload.data() + at + 4u);
        const auto dimension = read_u32(payload.data() + at + 12u);
        const auto bind = read_u32(payload.data() + at + 20u);
        const auto bind_count = read_u32(payload.data() + at + 24u);

        if (bind == 14u)
            return false;

        if (bind != 9u || bind_count != 1u)
            continue;

        if (type == 3u) {
            write_u32(payload.data() + at + 20u, 14u);
            ++sampler9;
        } else if (type == 2u && dimension == 4u) {
            // Historical V13: recycle the dead t9 BRDF-LUT binding as
            // the second native DSR EnvSpec cube endpoint.
            write_u32(payload.data() + at + 12u, 9u);
            write_u32(payload.data() + at + 20u, 14u);
            ++texture9;
        }
    }

    return sampler9 == 1u && texture9 == 1u;
}

constexpr std::array<std::uint32_t,47>
    k_v13_native_dsr_no_tail_chain = {{
        // A = already-sampled native DSR t12 * PTDE source A (b12[2]).
        0x08000038u,0x001000e2u,0x00000001u,0x00100e56u,
        0x00000001u,0x00208246u,0x0000000cu,0x00000002u,
        // if (beta)
        0x0404001fu,0x0020803au,0x0000000cu,0x00000003u,
        // Braw = native DSR t14
        0x8d000048u,0x80000182u,0x00155543u,0x001000e2u,
        0x0000000cu,0x00100796u,0x00000001u,0x00107936u,
        0x0000000eu,0x00106000u,0x0000000eu,0x0010003au,
        0x00000002u,
        // delta = Braw * PTDE source B (b12[3]) - A
        0x0b000032u,0x001000e2u,0x0000000cu,0x00100e56u,
        0x0000000cu,0x00208246u,0x0000000cu,0x00000003u,
        0x80100e56u,0x00000041u,0x00000001u,
        // A = delta * beta + A
        0x0a000032u,0x001000e2u,0x00000001u,0x00100e56u,
        0x0000000cu,0x0020803au,0x0000000cu,0x00000003u,
        0x00100e56u,0x00000001u,
        0x01000015u
    }};

bool apply_v13_native_dsr_no_tail(
    std::vector<std::uint8_t> &bytes,
    const pmetal_rgba_authority::entry &authority) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view> instructions;
    std::size_t code_index = 0u;

    if (!parse_dxbc(bytes.data(), bytes.size(), chunks, code_index) ||
        !extract_words(chunks, code_index, words) ||
        !decode(words, instructions))
        return false;

    std::optional<instruction_view> sampler9;
    std::optional<instruction_view> texture9;
    std::optional<instruction_view> t12_sample;
    std::size_t t9_sample_count = 0u;
    std::size_t t14_decl_count = 0u;

    for (const auto &ins : instructions) {
        if (ins.opcode == 0x5au && ins.length == 3u) {
            const auto slot = words[ins.offset + 2u];
            if (slot == 9u) {
                if (sampler9)
                    return false;
                sampler9 = ins;
            }
            if (slot == 14u)
                ++t14_decl_count;
        }

        if (ins.opcode == 0x58u && ins.length == 4u) {
            const auto slot = words[ins.offset + 2u];
            if (slot == 9u) {
                if (texture9)
                    return false;
                texture9 = ins;
            }
            if (slot == 14u)
                ++t14_decl_count;
        }

        if (ins.opcode >= 0x45u && ins.opcode <= 0x4au &&
            ins.length == 13u) {
            const auto resource = words[ins.offset + 8u];
            const auto sampler = words[ins.offset + 10u];

            if (resource == 12u && sampler == 12u) {
                if (t12_sample)
                    return false;
                t12_sample = ins;
            }

            if (resource == 9u || sampler == 9u)
                ++t9_sample_count;
        }
    }

    constexpr std::array<std::uint32_t,13> k_t12_sample = {{
        0x8d000048u,0x80000182u,0x00155543u,0x001000e2u,
        0x00000001u,0x00100796u,0x00000001u,0x00107936u,
        0x0000000cu,0x00106000u,0x0000000cu,0x0010003au,
        0x00000002u
    }};

    if (!sampler9 || !texture9 || !t12_sample ||
        t14_decl_count != 0u ||
        t12_sample->offset != authority.t12_word ||
        !std::equal(
            k_t12_sample.begin(), k_t12_sample.end(),
            words.begin() + static_cast<std::ptrdiff_t>(t12_sample->offset)))
        return false;

    const auto replace_begin = t12_sample->offset + t12_sample->length;
    const auto merge_word = authority.merge_word;

    if (merge_word <= replace_begin ||
        merge_word - replace_begin != 102u ||
        merge_word >= words.size() ||
        t9_sample_count != 1u)
        return false;

    const auto merge = std::find_if(
        instructions.begin(), instructions.end(),
        [&](const instruction_view &ins) {
            return ins.offset == merge_word;
        });

    if (merge == instructions.end() ||
        merge->opcode != 0x32u ||
        merge->length != 9u)
        return false;

    for (const auto &ins : instructions) {
        if (ins.opcode < 0x45u || ins.opcode > 0x4au ||
            ins.length != 13u)
            continue;

        const auto resource = words[ins.offset + 8u];
        const auto sampler = words[ins.offset + 10u];
        if ((resource == 9u || sampler == 9u) &&
            (ins.offset < replace_begin || ins.offset >= merge_word))
            return false;
    }

    // Historical V13 declaration reuse: t9/s9 -> native cube t14/s14.
    words[sampler9->offset + 2u] = 14u;
    words[texture9->offset] = 0x04003058u;
    words[texture9->offset + 2u] = 14u;

    std::copy(
        k_v13_native_dsr_no_tail_chain.begin(),
        k_v13_native_dsr_no_tail_chain.end(),
        words.begin() + static_cast<std::ptrdiff_t>(replace_begin));

    std::fill(
        words.begin() + static_cast<std::ptrdiff_t>(
            replace_begin + k_v13_native_dsr_no_tail_chain.size()),
        words.begin() + static_cast<std::ptrdiff_t>(merge_word),
        0x0100003au);

    if (!patch_v13_native_dsr_no_tail_rdef(chunks))
        return false;

    std::vector<std::uint8_t> rebuilt;
    if (!rebuild(
            bytes.data(), bytes.size(), std::move(chunks),
            code_index, words, rebuilt))
        return false;

    bytes = std::move(rebuilt);
    return true;
}

bool v13_native_dsr_no_tail_postcondition(
    const std::vector<std::uint8_t> &bytes,
    const pmetal_rgba_authority::entry &authority) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view> instructions;
    std::size_t code_index = 0u;

    if (!parse_dxbc(bytes.data(), bytes.size(), chunks, code_index) ||
        !extract_words(chunks, code_index, words) ||
        !decode(words, instructions))
        return false;

    std::size_t t14_decl = 0u;
    std::size_t s14_decl = 0u;
    std::size_t t14_sample = 0u;
    std::size_t t9_sample = 0u;
    std::size_t t10_sample = 0u;

    for (const auto &ins : instructions) {
        if (ins.opcode == 0x58u && ins.length == 4u &&
            words[ins.offset + 2u] == 14u &&
            words[ins.offset] == 0x04003058u)
            ++t14_decl;

        if (ins.opcode == 0x5au && ins.length == 3u &&
            words[ins.offset + 2u] == 14u)
            ++s14_decl;

        if (ins.opcode >= 0x45u && ins.opcode <= 0x4au) {
            if (ins.length == 13u) {
                const auto resource = words[ins.offset + 8u];
                const auto sampler = words[ins.offset + 10u];
                if (resource == 14u && sampler == 14u)
                    ++t14_sample;
                if (resource == 9u || sampler == 9u)
                    ++t9_sample;
            }
            if (ins.length == 11u &&
                words[ins.offset + 8u] == 10u)
                ++t10_sample;
        }
    }

    const auto merge = std::find_if(
        instructions.begin(), instructions.end(),
        [&](const instruction_view &ins) {
            return ins.offset == authority.merge_word;
        });

    auto *rdef = unique_rdef(chunks);

    return
        rdef != nullptr &&
        legacy_plan::dxbc::rdef::has_constant_buffer_binding(
            rdef->payload, 12u) &&
        merge != instructions.end() &&
        merge->opcode == 0x32u &&
        merge->length == 9u &&
        t14_decl == 1u &&
        s14_decl == 1u &&
        t14_sample == 1u &&
        t9_sample == 0u &&
        t10_sample == 0u;

}

#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
bool append_t10_rdef_from_t1(
    std::vector<chunk> &chunks) noexcept
{
    auto *rdef = unique_rdef(chunks);
    if (rdef == nullptr || rdef->payload.size() < 16u)
        return false;

    auto &payload = rdef->payload;
    const auto count = read_u32(payload.data() + 8u);
    const auto offset = read_u32(payload.data() + 12u);
    constexpr std::uint32_t k_binding_size = 32u;

    if (count == 0u || count > 256u ||
        offset > payload.size() ||
        static_cast<std::uint64_t>(count) * k_binding_size >
            payload.size() - offset)
        return false;

    std::size_t t1_at = 0u;
    std::uint32_t t1_count = 0u;

    for (std::uint32_t i = 0u; i < count; ++i) {
        const auto at =
            static_cast<std::size_t>(offset) +
            static_cast<std::size_t>(i) * k_binding_size;
        const auto type = read_u32(payload.data() + at + 4u);
        const auto bind = read_u32(payload.data() + at + 20u);
        const auto bind_count = read_u32(payload.data() + at + 24u);

        if (type == 2u && bind == 10u)
            return false;

        if (type == 2u && bind == 1u && bind_count == 1u) {
            t1_at = at;
            ++t1_count;
        }
    }

    if (t1_count != 1u)
        return false;

    try {
        static constexpr char k_name[] = "DSRRL_PTDE_SpecRGB";
        const auto name_offset =
            static_cast<std::uint32_t>(payload.size());

        payload.insert(
            payload.end(),
            reinterpret_cast<const std::uint8_t *>(k_name),
            reinterpret_cast<const std::uint8_t *>(k_name) + sizeof(k_name));

        while ((payload.size() & 3u) != 0u)
            payload.push_back(0u);

        const auto new_table =
            static_cast<std::uint32_t>(payload.size());

        std::vector<std::uint8_t> table(
            payload.data() + offset,
            payload.data() + offset +
                static_cast<std::size_t>(count) * k_binding_size);

        std::array<std::uint8_t,k_binding_size> t10{};
        std::memcpy(
            t10.data(),
            payload.data() + t1_at,
            k_binding_size);
        write_u32(t10.data(), name_offset);
        write_u32(t10.data() + 20u, 10u);

        table.insert(table.end(), t10.begin(), t10.end());
        payload.insert(payload.end(), table.begin(), table.end());
        write_u32(payload.data() + 8u, count + 1u);
        write_u32(payload.data() + 12u, new_table);
    } catch (...) {
        return false;
    }

    return true;
}

bool rdef_has_t10_texture(
    const std::vector<std::uint8_t> &payload) noexcept
{
    if (payload.size() < 16u)
        return false;

    const auto count = read_u32(payload.data() + 8u);
    const auto offset = read_u32(payload.data() + 12u);
    constexpr std::uint32_t k_binding_size = 32u;

    if (count == 0u || count > 256u ||
        offset > payload.size() ||
        static_cast<std::uint64_t>(count) * k_binding_size >
            payload.size() - offset)
        return false;

    std::uint32_t hits = 0u;
    for (std::uint32_t i = 0u; i < count; ++i) {
        const auto at = offset + i * k_binding_size;
        const auto type = read_u32(payload.data() + at + 4u);
        const auto bind = read_u32(payload.data() + at + 20u);
        const auto bind_count = read_u32(payload.data() + at + 24u);
        if (type == 2u && bind == 10u && bind_count == 1u)
            ++hits;
    }
    return hits == 1u;
}

bool apply_v13_native_dsr_material_mod_only(
    std::vector<std::uint8_t> &bytes,
    const pmetal_rgba_authority::entry &authority) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view> instructions;
    std::size_t code_index = 0u;

    if (!parse_dxbc(bytes.data(), bytes.size(), chunks, code_index) ||
        !extract_words(chunks, code_index, words) ||
        !decode(words, instructions))
        return false;

    std::optional<instruction_view> t1_decl;
    std::optional<instruction_view> t1_sample;
    std::size_t t10_decl_count = 0u;
    std::size_t t10_sample_count = 0u;

    for (const auto &ins : instructions) {
        if (ins.opcode == 0x58u && ins.length == 4u) {
            const auto slot = words[ins.offset + 2u];
            if (slot == 1u) {
                if (t1_decl)
                    return false;
                t1_decl = ins;
            }
            if (slot == 10u)
                ++t10_decl_count;
        }

        if (ins.opcode >= 0x45u && ins.opcode <= 0x4au &&
            ins.length == 11u) {
            const auto resource = words[ins.offset + 8u];
            if (resource == 1u) {
                if (t1_sample)
                    return false;
                t1_sample = ins;
            }
            if (resource == 10u)
                ++t10_sample_count;
        }
    }

    if (!t1_decl || !t1_sample ||
        t10_decl_count != 0u ||
        t10_sample_count != 0u ||
        t1_decl->offset >= authority.t12_word ||
        t1_sample->offset >= authority.t12_word)
        return false;

    const auto material_at =
        authority.t12_word + 13u +
        k_v13_native_dsr_no_tail_chain.size();
    constexpr std::size_t k_material_words = 33u;

    if (material_at + k_material_words > authority.merge_word ||
        authority.merge_word >= words.size())
        return false;

    for (std::size_t i = material_at;
         i < material_at + k_material_words;
         ++i)
        if (words[i] != 0x0100003au)
            return false;

    const auto merge = std::find_if(
        instructions.begin(), instructions.end(),
        [&](const instruction_view &ins) {
            return ins.offset == authority.merge_word;
        });
    if (merge == instructions.end() ||
        merge->opcode != 0x32u ||
        merge->length != 9u)
        return false;

    std::array<std::uint32_t,4> t10_decl{};
    std::copy_n(
        words.begin() + static_cast<std::ptrdiff_t>(t1_decl->offset),
        4u,
        t10_decl.begin());
    t10_decl[2] = 10u;

    std::array<std::uint32_t,11> fresh_spec{};
    std::copy_n(
        words.begin() + static_cast<std::ptrdiff_t>(t1_sample->offset),
        11u,
        fresh_spec.begin());
    if (fresh_spec[7] != 0x00107936u)
        return false;
    fresh_spec[3] = 0x001000e2u;
    fresh_spec[4] = 12u;
    fresh_spec[8] = 10u;

    const std::array<std::uint32_t,8> c101_mul{{
        0x08000038u,
        0x00100072u,12u,
        0x00100796u,12u,
        0x00208246u,12u,0u
    }};
    const std::array<std::uint32_t,7> color_mul{{
        0x07000038u,
        0x00100072u,12u,
        0x00100246u,12u,
        0x00101246u,authority.color0_register
    }};
    const std::array<std::uint32_t,7> envspec_mul{{
        0x07000038u,
        0x001000e2u,1u,
        0x00100e56u,1u,
        0x00100246u,12u
    }};

    auto out = words.begin() + static_cast<std::ptrdiff_t>(material_at);
    out = std::copy(fresh_spec.begin(), fresh_spec.end(), out);
    out = std::copy(c101_mul.begin(), c101_mul.end(), out);
    out = std::copy(color_mul.begin(), color_mul.end(), out);
    std::copy(envspec_mul.begin(), envspec_mul.end(), out);

    words.insert(
        words.begin() + static_cast<std::ptrdiff_t>(t1_decl->offset + 4u),
        t10_decl.begin(),
        t10_decl.end());
    words[1] = static_cast<std::uint32_t>(words.size());

    if (!append_t10_rdef_from_t1(chunks))
        return false;

    std::vector<std::uint8_t> rebuilt;
    if (!rebuild(
            bytes.data(), bytes.size(), std::move(chunks),
            code_index, words, rebuilt))
        return false;

    bytes = std::move(rebuilt);
    return true;
}

bool v13_native_dsr_material_mod_postcondition(
    const std::vector<std::uint8_t> &bytes,
    const pmetal_rgba_authority::entry &authority) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view> instructions;
    std::size_t code_index = 0u;

    if (!parse_dxbc(bytes.data(), bytes.size(), chunks, code_index) ||
        !extract_words(chunks, code_index, words) ||
        !decode(words, instructions))
        return false;

    std::size_t t10_decl = 0u;
    std::size_t t10_sample = 0u;
    std::size_t t9_sample = 0u;
    std::size_t t14_sample = 0u;
    std::size_t t10_sample_word = static_cast<std::size_t>(-1);

    for (const auto &ins : instructions) {
        if (ins.opcode == 0x58u && ins.length == 4u &&
            words[ins.offset + 2u] == 10u)
            ++t10_decl;

        if (ins.opcode >= 0x45u && ins.opcode <= 0x4au) {
            if (ins.length == 11u && words[ins.offset + 8u] == 10u) {
                ++t10_sample;
                t10_sample_word = ins.offset;
            }
            if (ins.length == 13u) {
                const auto resource = words[ins.offset + 8u];
                const auto sampler = words[ins.offset + 10u];
                if (resource == 9u || sampler == 9u)
                    ++t9_sample;
                if (resource == 14u && sampler == 14u)
                    ++t14_sample;
            }
        }
    }

    const auto shifted_merge = authority.merge_word + 4u;
    const auto merge = std::find_if(
        instructions.begin(), instructions.end(),
        [&](const instruction_view &ins) {
            return ins.offset == shifted_merge;
        });

    const auto expected_t10_word =
        authority.t12_word + 13u +
        k_v13_native_dsr_no_tail_chain.size() + 4u;

    auto *rdef = unique_rdef(chunks);

    if (rdef == nullptr ||
        !legacy_plan::dxbc::rdef::has_constant_buffer_binding(
            rdef->payload,12u) ||
        !rdef_has_t10_texture(rdef->payload) ||
        merge == instructions.end() ||
        merge->opcode != 0x32u ||
        merge->length != 9u ||
        t10_decl != 1u ||
        t10_sample != 1u ||
        t10_sample_word != expected_t10_word ||
        t9_sample != 0u ||
        t14_sample != 1u)
        return false;

    const auto c101_at = expected_t10_word + 11u;
    const auto color_at = c101_at + 8u;
    const auto envspec_at = color_at + 7u;

    if (envspec_at + 7u > words.size())
        return false;

    const std::array<std::uint32_t,8> c101_mul{{
        0x08000038u,
        0x00100072u,12u,
        0x00100796u,12u,
        0x00208246u,12u,0u
    }};
    const std::array<std::uint32_t,7> color_mul{{
        0x07000038u,
        0x00100072u,12u,
        0x00100246u,12u,
        0x00101246u,authority.color0_register
    }};
    const std::array<std::uint32_t,7> envspec_mul{{
        0x07000038u,
        0x001000e2u,1u,
        0x00100e56u,1u,
        0x00100246u,12u
    }};

    return
        std::equal(
            c101_mul.begin(),c101_mul.end(),
            words.begin() + static_cast<std::ptrdiff_t>(c101_at)) &&
        std::equal(
            color_mul.begin(),color_mul.end(),
            words.begin() + static_cast<std::ptrdiff_t>(color_at)) &&
        std::equal(
            envspec_mul.begin(),envspec_mul.end(),
            words.begin() + static_cast<std::ptrdiff_t>(envspec_at));
}
#endif
#endif

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


#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
bool apply_exact_ptde_envdiffuse_consumer(
    std::vector<std::uint8_t> &bytes) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view> instructions;
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

    // Exact stable P_Metal host island:
    //   mul r2.xyz, cb0[3].xyz, cb0[79].x        (9 dwords)
    //   sample_l sample.xyz, coord, t11, s11     (13 dwords)
    //   mul r2.xyz, r2.xyz, sample.xyz           (7 dwords)
    //
    // Replace the complete 29-dword producer window, preserving length:
    //   sample_l r12.xyzw, coord, t11, s11       (13)
    //   div      r12.xyz, r12.xyz, r12.www       (7)
    //   mul      r2.xyz, r12.xyz, cb12[3].xyz    (8)
    //   nop                                         (1)
    //
    // This is the PTDE local operator: filter raw RGBA first, then RGB/A,
    // then multiply by the exact PTDE EnvDiffuse LightBank endpoint. r12 is
    // already a certified scratch temp in the immediately preceding Build131
    // EnvSpec island and is dead at this source boundary.

    constexpr std::array<std::uint32_t,9>
        k_stock_source{{
            0x09000038u,
            0x00100072u,0x00000002u,
            0x00208246u,0x00000000u,0x00000003u,
            0x00208006u,0x00000000u,0x0000004fu
        }};

    std::size_t hits = 0u;
    std::size_t source_word =
        static_cast<std::size_t>(-1);
    std::size_t sample_word =
        static_cast<std::size_t>(-1);
    std::uint32_t original_sample_register = 0u;

    for (std::size_t i = 1u;
         i + 1u < instructions.size();
         ++i) {
        const auto &sample =
            instructions[i];

        if (sample.opcode < 0x45u ||
            sample.opcode > 0x4au ||
            sample.length != 13u ||
            sample.offset + 12u >= words.size() ||
            words[sample.offset + 8u] != 11u ||
            words[sample.offset + 10u] != 11u)
            continue;

        const auto &before =
            instructions[i - 1u];
        const auto &after =
            instructions[i + 1u];

        if (before.length !=
                k_stock_source.size() ||
            before.offset +
                before.length !=
                    sample.offset ||
            sample.offset +
                sample.length !=
                    after.offset ||
            after.opcode != 0x38u ||
            after.length != 7u ||
            before.offset +
                k_stock_source.size() >
                    words.size() ||
            after.offset + after.length >
                words.size() ||
            !std::equal(
                k_stock_source.begin(),
                k_stock_source.end(),
                words.begin() +
                    static_cast<std::ptrdiff_t>(
                        before.offset)))
            return false;

        // Retail DSR rx33/rx34/rx35 all encode the post-t11 multiply as:
        //   mul r2.xyz, r2.xyz, r3.xyz
        // with the xyz source operand token 0x00100246. Earlier PR213 code
        // incorrectly required 0x00100e56 (a different swizzle encoding),
        // causing all three exact receivers to fail before replacement
        // registration. Keep the retail-exact seven-DWORD guard here.
        if (words[after.offset] !=
                0x07000038u ||
            words[after.offset + 1u] !=
                0x00100072u ||
            words[after.offset + 2u] != 2u ||
            words[after.offset + 3u] !=
                0x00100246u ||
            words[after.offset + 4u] != 2u ||
            words[after.offset + 5u] !=
                0x00100246u)
            return false;

        original_sample_register =
            words[after.offset + 6u];

        ++hits;
        source_word = before.offset;
        sample_word = sample.offset;
    }

    if (hits != 1u ||
        source_word ==
            static_cast<std::size_t>(-1) ||
        sample_word ==
            static_cast<std::size_t>(-1))
        return false;

    std::array<std::uint32_t,13>
        ptde_sample{};
    std::copy_n(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                sample_word),
        ptde_sample.size(),
        ptde_sample.begin());

    // Keep the exact coordinate/resource/sampler operands but capture all four
    // filtered channels in r12 so alpha survives until the post-filter divide.
    ptde_sample[3] = 0x001000f2u;
    ptde_sample[4] = 12u;

    const std::array<std::uint32_t,7>
        decode_rgb_over_alpha{{
            0x0700000eu,
            0x001000e2u,12u,
            0x00100e56u,12u,
            0x00100006u,12u
        }};

    const std::array<std::uint32_t,8>
        apply_ptde_endpoint{{
            0x08000038u,
            0x00100072u,2u,
            0x00100e56u,12u,
            0x00208246u,12u,3u
        }};

    std::size_t out = source_word;
    std::copy(
        ptde_sample.begin(),
        ptde_sample.end(),
        words.begin() +
            static_cast<std::ptrdiff_t>(
                out));
    out += ptde_sample.size();

    std::copy(
        decode_rgb_over_alpha.begin(),
        decode_rgb_over_alpha.end(),
        words.begin() +
            static_cast<std::ptrdiff_t>(
                out));
    out += decode_rgb_over_alpha.size();

    std::copy(
        apply_ptde_endpoint.begin(),
        apply_ptde_endpoint.end(),
        words.begin() +
            static_cast<std::ptrdiff_t>(
                out));
    out += apply_ptde_endpoint.size();

    words[out++] = 0x0100003au;

    if (out != source_word + 29u)
        return false;

    // The original sampled temp may now be unused, which is legal. Retain
    // this read as an exact-shape sanity check and avoid accepting an
    // impossible register encoding silently.
    if (original_sample_register >= 0x1000u)
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
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R3)
// R3 closes a DSR-only composition term that survived the R2 full-PTDE
// source replacement. rx33/rx34 carry one additional scalar multiply
// immediately after the first EnvSpec+EnvDiffuse merge; PTDE applies its
// chromatic visibility before material modulation instead. Once the exact
// PTDE EnvSpec and EnvDiffuse terms are active, retaining this host scalar
// double-attenuates the environment surface. Plain rx35 has no such join.
bool remove_dsr_postmerge_visibility_scalar_r3(
    std::vector<std::uint8_t> &bytes,
    const pmetal_rgba_authority::entry &authority) noexcept
{
    if (!authority.remove_visibility_exponent)
        return true;

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index = 0u;

    if (!parse_dxbc(
            bytes.data(), bytes.size(),
            chunks, code_index) ||
        !extract_words(
            chunks, code_index, words))
        return false;

    constexpr std::size_t k_build131_inserted_words = 15u;
    const auto merge_word =
        authority.merge_word +
        k_build131_inserted_words;
    const auto join_word = merge_word + 9u;

    if (join_word + 6u >= words.size())
        return false;

    // First common environment merge remains structurally exact.
    if (words[merge_word] != 0x09000032u ||
        words[merge_word + 1u] != 0x001000e2u ||
        words[merge_word + 2u] != 1u ||
        words[merge_word + 7u] != 0x00100e56u ||
        words[merge_word + 8u] != 1u)
        return false;

    // Exact DSR-only post-merge scalar:
    // mul r1.xyz, r1.xyz, r(reflection_coord_register).x
    if (words[join_word] != 0x07000038u ||
        words[join_word + 1u] != 0x001000e2u ||
        words[join_word + 2u] != 1u ||
        words[join_word + 3u] != 0x00100e56u ||
        words[join_word + 4u] != 1u ||
        words[join_word + 5u] != 0x00100006u ||
        words[join_word + 6u] !=
            authority.reflection_coord_register)
        return false;

    // Preserve instruction length and the merge topology while removing only
    // the host-only second attenuation. This does not alter cubemap content,
    // probe routing, PTDE RGB/A decode, c87/c86, SpecRGB, c101 or COLOR0.
    words[join_word + 5u] = 0x00004001u;
    words[join_word + 6u] = 0x3f800000u;

    std::vector<std::uint8_t> rebuilt;
    if (!rebuild(
            bytes.data(), bytes.size(),
            std::move(chunks),
            code_index, words, rebuilt))
        return false;

    bytes = std::move(rebuilt);
    return true;
}

bool r3_postmerge_visibility_postcondition(
    const std::vector<std::uint8_t> &bytes,
    const pmetal_rgba_authority::entry &authority) noexcept
{
    if (!authority.remove_visibility_exponent)
        return true;

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index = 0u;

    if (!parse_dxbc(
            bytes.data(), bytes.size(),
            chunks, code_index) ||
        !extract_words(
            chunks, code_index, words))
        return false;

    constexpr std::size_t k_build131_inserted_words = 15u;
    const auto merge_word =
        authority.merge_word +
        k_build131_inserted_words;
    const auto join_word = merge_word + 9u;

    return
        join_word + 6u < words.size() &&
        words[join_word] == 0x07000038u &&
        words[join_word + 1u] == 0x001000e2u &&
        words[join_word + 2u] == 1u &&
        words[join_word + 3u] == 0x00100e56u &&
        words[join_word + 4u] == 1u &&
        words[join_word + 5u] == 0x00004001u &&
        words[join_word + 6u] == 0x3f800000u;
}
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R11_COMMON_MERGE)
// PTDE stable Phn HemEnv has no DSR screen-space SAO multiplier after the
// first common environment/material merge. Retail DSR samples t8/s8 and then
// multiplies the already-composed EnvSpec + diffuse*(EnvDiffuse + hemisphere)
// surface by the sampled scalar. R11 removes only that DSR-only whole-surface
// gate. The t8 sample itself is left byte-for-byte intact so this patch adds no
// resource/state routing requirement and changes no callback/runtime gate.
bool remove_dsr_sao_surface_multiplier_r11(
    std::vector<std::uint8_t> &bytes) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view> instructions;
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

    std::size_t hits = 0u;
    std::size_t multiply_word =
        static_cast<std::size_t>(-1);

    for (std::size_t i = 0u;
         i + 1u < instructions.size();
         ++i) {
        const auto &sample =
            instructions[i];

        if (sample.opcode != 0x48u ||
            sample.length != 13u ||
            sample.offset + 12u >= words.size() ||
            words[sample.offset + 8u] != 8u ||
            words[sample.offset + 10u] != 8u)
            continue;

        const auto &mul =
            instructions[i + 1u];

        if (mul.offset !=
                sample.offset +
                    sample.length ||
            mul.opcode != 0x38u ||
            mul.length != 7u ||
            mul.offset + 6u >=
                words.size())
            return false;

        // Exact stable rx33/rx34/rx35 host continuation:
        //   sample_l r2.x, ..., t8, s8
        //   mul      r1.xyz, r1.xyz, r2.x
        //
        // The sample destination register may vary if the compiler layout ever
        // changes, so tie the consumer to the exact sampled scalar instead of
        // hard-coding register 2. Destination/source r1 is invariant across
        // the certified retail stable triple.
        if (words[mul.offset + 1u] !=
                0x001000e2u ||
            words[mul.offset + 2u] != 1u ||
            words[mul.offset + 3u] !=
                0x00100e56u ||
            words[mul.offset + 4u] != 1u ||
            words[mul.offset + 5u] !=
                0x00100006u ||
            words[mul.offset + 6u] !=
                words[sample.offset + 4u])
            return false;

        ++hits;
        multiply_word = mul.offset;
    }

    if (hits != 1u ||
        multiply_word ==
            static_cast<std::size_t>(-1))
        return false;

    // Length-preserving neutralization:
    //   mul r1.xyz, r1.xyz, SAO  ->  mul r1.xyz, r1.xyz, 1.0
    // No instruction is added and the t8 sample remains present/inert.
    words[multiply_word + 5u] =
        0x00004001u;
    words[multiply_word + 6u] =
        0x3f800000u;

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

bool r11_common_merge_postcondition(
    const std::vector<std::uint8_t> &bytes) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view> instructions;
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

    std::size_t neutralized = 0u;

    for (std::size_t i = 0u;
         i + 1u < instructions.size();
         ++i) {
        const auto &sample =
            instructions[i];

        if (sample.opcode != 0x48u ||
            sample.length != 13u ||
            sample.offset + 12u >= words.size() ||
            words[sample.offset + 8u] != 8u ||
            words[sample.offset + 10u] != 8u)
            continue;

        const auto &mul =
            instructions[i + 1u];

        if (mul.offset ==
                sample.offset +
                    sample.length &&
            mul.opcode == 0x38u &&
            mul.length == 7u &&
            mul.offset + 6u <
                words.size() &&
            words[mul.offset + 1u] ==
                0x001000e2u &&
            words[mul.offset + 2u] == 1u &&
            words[mul.offset + 3u] ==
                0x00100e56u &&
            words[mul.offset + 4u] == 1u &&
            words[mul.offset + 5u] ==
                0x00004001u &&
            words[mul.offset + 6u] ==
                0x3f800000u)
            ++neutralized;
    }

    return neutralized == 1u;
}
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R15_PTDE_LIVEOUT_PAIR)
// R15 corrects the R14 overbright falsifier by restoring the PTDE downstream
// topology instead of opening the DSR MaterialWorkflow additive path.
//
// DSR stable HemEnv exposes two independent MaterialWorkflow live-outs:
//   (A) a DSR-only additive/PBL carrier:
//         extra *= (1 - SpecTex.a) * cb0[100].w * 10
//       which is added to the common surface later;
//   (B) a later post-merge gate:
//         common *= SpecTex.a
//       (fused as SpecTex.a * common + hemisphere in the plain receiver).
//
// Exact PTDE stable HemEnv has neither downstream operator: the visible result
// is the PTDE common diffuse/environment composition plus the PTDE specular
// term. Therefore R15 zeros only (A) and neutralizes only (B). It preserves
// the rest of gFC_MaterialWorkflow, Build131, PTDE EnvDiffuse/EnvSpec,
// SpecRGB*c101*COLOR0, shadow, SAO removal and terminal scene encoding.
bool apply_ptde_materialworkflow_liveout_pair_r15(
    std::vector<std::uint32_t> &words) noexcept
{
    std::vector<instruction_view> instructions;
    if (!decode(words, instructions))
        return false;

    std::optional<std::uint32_t> spec_sample_register;
    std::size_t material_if = static_cast<std::size_t>(-1);

    for (std::size_t i = 0u; i + 1u < instructions.size(); ++i) {
        const auto &sample = instructions[i];
        const auto &branch = instructions[i + 1u];

        if (sample.opcode < 0x45u || sample.opcode > 0x4au ||
            sample.length != 11u ||
            sample.offset + 10u >= words.size() ||
            words[sample.offset + 7u] != 0x00107936u ||
            words[sample.offset + 8u] != 1u ||
            words[sample.offset + 10u] != 1u ||
            branch.opcode != 0x1fu ||
            branch.length != 4u ||
            branch.offset != sample.offset + sample.length ||
            branch.offset + 3u >= words.size() ||
            words[branch.offset + 1u] != 0x0020800au ||
            words[branch.offset + 2u] != 0u ||
            words[branch.offset + 3u] != 85u)
            continue;

        if (spec_sample_register.has_value())
            return false;

        spec_sample_register = words[sample.offset + 4u];
        material_if = i + 1u;
    }

    if (!spec_sample_register.has_value() ||
        material_if == static_cast<std::size_t>(-1))
        return false;

    std::size_t depth = 0u;
    bool in_else = false;
    std::size_t material_endif = static_cast<std::size_t>(-1);
    std::optional<std::uint32_t> weight_register;
    std::size_t weight_chain_hits = 0u;
    std::size_t additive_gate_word = static_cast<std::size_t>(-1);
    std::size_t additive_gate_hits = 0u;

    for (std::size_t i = material_if + 1u; i < instructions.size(); ++i) {
        const auto &ins = instructions[i];

        if (ins.opcode == 0x1fu) {
            ++depth;
            continue;
        }

        if (ins.opcode == 0x12u && depth == 0u) {
            in_else = true;
            continue;
        }

        if (ins.opcode == 0x15u) {
            if (depth == 0u) {
                material_endif = i;
                break;
            }
            --depth;
            continue;
        }

        if (depth != 0u || in_else)
            continue;

        // Exact DSR scalar producer:
        //   add rW.w, -SpecTex.a, 1
        //   mul rW.w, rW.w, cb0[100].w
        //   mul rW.w, rW.w, 10
        if (ins.opcode == 0x00u &&
            ins.length == 8u &&
            ins.offset + 7u < words.size() &&
            words[ins.offset + 1u] == 0x00100082u &&
            words[ins.offset + 3u] == 0x8010000au &&
            words[ins.offset + 4u] == 0x00000041u &&
            words[ins.offset + 5u] == *spec_sample_register &&
            words[ins.offset + 6u] == 0x00004001u &&
            words[ins.offset + 7u] == 0x3f800000u) {
            if (i + 2u >= instructions.size())
                return false;

            const auto reg = words[ins.offset + 2u];
            const auto &mul_cb = instructions[i + 1u];
            const auto &mul_10 = instructions[i + 2u];

            if (mul_cb.opcode != 0x38u ||
                mul_cb.length != 8u ||
                mul_cb.offset + 7u >= words.size() ||
                words[mul_cb.offset + 1u] != 0x00100082u ||
                words[mul_cb.offset + 2u] != reg ||
                words[mul_cb.offset + 3u] != 0x0010003au ||
                words[mul_cb.offset + 4u] != reg ||
                words[mul_cb.offset + 5u] != 0x0020803au ||
                words[mul_cb.offset + 6u] != 0u ||
                words[mul_cb.offset + 7u] != 100u ||
                mul_10.opcode != 0x38u ||
                mul_10.length != 7u ||
                mul_10.offset + 6u >= words.size() ||
                words[mul_10.offset + 1u] != 0x00100082u ||
                words[mul_10.offset + 2u] != reg ||
                words[mul_10.offset + 3u] != 0x0010003au ||
                words[mul_10.offset + 4u] != reg ||
                words[mul_10.offset + 5u] != 0x00004001u ||
                words[mul_10.offset + 6u] != 0x41200000u)
                return false;

            ++weight_chain_hits;
            weight_register = reg;
            continue;
        }

        if (!weight_register.has_value() ||
            ins.opcode != 0x38u ||
            ins.length != 7u ||
            ins.offset + 6u >= words.size() ||
            words[ins.offset + 1u] != 0x00100072u ||
            words[ins.offset + 3u] != 0x00100ff6u ||
            words[ins.offset + 4u] != *weight_register ||
            words[ins.offset + 5u] != 0x00100246u ||
            words[ins.offset + 6u] != words[ins.offset + 2u])
            continue;

        ++additive_gate_hits;
        additive_gate_word = ins.offset;
    }

    if (material_endif == static_cast<std::size_t>(-1) ||
        weight_chain_hits != 1u ||
        additive_gate_hits != 1u ||
        additive_gate_word == static_cast<std::size_t>(-1))
        return false;

    // DSR-only additive/PBL live-out -> zero.
    words[additive_gate_word + 3u] = 0x00004001u;
    words[additive_gate_word + 4u] = 0x00000000u;

    std::size_t postmerge_gate_word = static_cast<std::size_t>(-1);
    bool postmerge_is_mad = false;
    std::size_t postmerge_gate_hits = 0u;

    for (std::size_t i = material_endif + 1u; i < instructions.size(); ++i) {
        const auto &ins = instructions[i];

        // Csd/Sdw:
        //   mul r1.yzw, r1.yyzw, SpecTex.a
        if (ins.opcode == 0x38u &&
            ins.length == 7u &&
            ins.offset + 6u < words.size() &&
            words[ins.offset + 1u] == 0x001000e2u &&
            words[ins.offset + 3u] == 0x00100e56u &&
            words[ins.offset + 4u] == words[ins.offset + 2u] &&
            words[ins.offset + 5u] == 0x00100006u &&
            words[ins.offset + 6u] == *spec_sample_register) {
            ++postmerge_gate_hits;
            postmerge_gate_word = ins.offset;
            postmerge_is_mad = false;
            continue;
        }

        // Plain:
        //   mad r1.yzw, SpecTex.a, r1.yyzw, r2.xxyz
        if (ins.opcode == 0x32u &&
            ins.length == 9u &&
            ins.offset + 8u < words.size() &&
            words[ins.offset + 1u] == 0x001000e2u &&
            words[ins.offset + 3u] == 0x00100006u &&
            words[ins.offset + 4u] == *spec_sample_register &&
            words[ins.offset + 5u] == 0x00100e56u &&
            words[ins.offset + 6u] == words[ins.offset + 2u] &&
            words[ins.offset + 7u] == 0x00100906u) {
            ++postmerge_gate_hits;
            postmerge_gate_word = ins.offset;
            postmerge_is_mad = true;
        }
    }

    if (postmerge_gate_hits != 1u ||
        postmerge_gate_word == static_cast<std::size_t>(-1))
        return false;

    // rx33/rx34: do not rewrite this MUL here. R3 already owns exactly
    // this merge+9 scalar and neutralizes it after Build131. Rewriting it
    // here destroys R3's exact-stock precondition and makes the replacement
    // fail open.
    //
    // rx35: there is no R3 owner. Its homologous SpecTex.a gate is fused into
    // a MAD, so R15 owns and neutralizes only that plain-HemEnv coefficient.
    if (postmerge_is_mad) {
        const auto coeff_word =
            postmerge_gate_word + 3u;
        words[coeff_word] = 0x00004001u;
        words[coeff_word + 1u] = 0x3f800000u;

        if (words[coeff_word] != 0x00004001u ||
            words[coeff_word + 1u] != 0x3f800000u)
            return false;
    }

    // Suppress the DSR-only additive/PBL live-out on all three stable
    // receivers. Csd/Sdw post-merge gating remains stock until R3 executes.
    return
        words[additive_gate_word + 3u] == 0x00004001u &&
        words[additive_gate_word + 4u] == 0x00000000u;
}
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R16_LEGACY_DIFFUSE_SPLIT)
// R16 restores the PTDE legacy diffuse/common carrier inside the DSR
// MaterialWorkflow true branch.
//
// Exact DSR stable HemEnv computes, before EnvDiffuse/common composition:
//   rDiffuse = SpecTex.a * workflowDiffuse;
//   rDiffuse *= (1 - workflowWeight);
//
// R13 proved that removing SpecTex.a alone is insufficient: the owner pixel
// remains black except rust. R15B then proved that downstream live-outs are
// not the root either. Exact PTDE stable HemEnv has no homologous SpecTex.a
// gate and no homologous metalness/workflow-weight split on its legacy
// DiffuseMaterial carrier. R16 therefore neutralizes BOTH DSR-only factors,
// leaving workflowDiffuse itself as the common diffuse material input.
//
// This is length-preserving and does not touch the separate R15 additive
// live-out or the R3-owned rx33/rx34 post-merge scalar.
bool remove_dsr_materialworkflow_legacy_diffuse_split_r16(
    std::vector<std::uint32_t> &words) noexcept
{
    std::vector<instruction_view> instructions;
    if (!decode(words, instructions))
        return false;

    std::optional<std::uint32_t> spec_sample_register;
    std::size_t material_if = static_cast<std::size_t>(-1);

    for (std::size_t i = 0u; i + 1u < instructions.size(); ++i) {
        const auto &sample = instructions[i];
        const auto &branch = instructions[i + 1u];

        if (sample.opcode < 0x45u || sample.opcode > 0x4au ||
            sample.length != 11u ||
            sample.offset + 10u >= words.size() ||
            words[sample.offset + 7u] != 0x00107936u ||
            words[sample.offset + 8u] != 1u ||
            words[sample.offset + 10u] != 1u ||
            branch.opcode != 0x1fu ||
            branch.length != 4u ||
            branch.offset != sample.offset + sample.length ||
            branch.offset + 3u >= words.size() ||
            words[branch.offset + 1u] != 0x0020800au ||
            words[branch.offset + 2u] != 0u ||
            words[branch.offset + 3u] != 85u)
            continue;

        if (spec_sample_register.has_value())
            return false;

        spec_sample_register = words[sample.offset + 4u];
        material_if = i + 1u;
    }

    if (!spec_sample_register.has_value() ||
        material_if == static_cast<std::size_t>(-1))
        return false;

    std::size_t depth = 0u;
    std::size_t alpha_mul_word = static_cast<std::size_t>(-1);
    std::size_t weight_mul_word = static_cast<std::size_t>(-1);
    std::uint32_t diffuse_register = 0u;
    std::size_t hits = 0u;

    for (std::size_t i = material_if + 1u; i < instructions.size(); ++i) {
        const auto &ins = instructions[i];

        if (ins.opcode == 0x1fu) {
            ++depth;
            continue;
        }
        if (ins.opcode == 0x12u && depth == 0u)
            break;
        if (ins.opcode == 0x15u) {
            if (depth == 0u)
                break;
            --depth;
            continue;
        }

        if (depth != 0u ||
            ins.opcode != 0x38u ||
            ins.length != 7u ||
            ins.offset + 6u >= words.size() ||
            (words[ins.offset] & 0x00002000u) != 0u ||
            words[ins.offset + 1u] != 0x00100072u ||
            words[ins.offset + 3u] != 0x00100006u ||
            words[ins.offset + 4u] != *spec_sample_register ||
            words[ins.offset + 5u] != 0x00100246u)
            continue;

        if (i + 2u >= instructions.size())
            return false;

        const auto &add = instructions[i + 1u];
        const auto &mul2 = instructions[i + 2u];
        const auto dst = words[ins.offset + 2u];

        // Exact continuation:
        //   add rW.w, -workflowWeight, 1
        //   mul rDiffuse.xyz, rW.wwww, rDiffuse.xyz
        if (add.opcode != 0x00u ||
            add.length != 8u ||
            add.offset + 7u >= words.size() ||
            words[add.offset + 1u] != 0x00100082u ||
            words[add.offset + 6u] != 0x00004001u ||
            words[add.offset + 7u] != 0x3f800000u ||
            mul2.opcode != 0x38u ||
            mul2.length != 7u ||
            mul2.offset + 6u >= words.size() ||
            words[mul2.offset + 1u] != 0x00100072u ||
            words[mul2.offset + 2u] != dst ||
            words[mul2.offset + 5u] != 0x00100246u ||
            words[mul2.offset + 6u] != dst)
            continue;

        ++hits;
        alpha_mul_word = ins.offset;
        weight_mul_word = mul2.offset;
        diffuse_register = dst;
    }

    if (hits != 1u ||
        alpha_mul_word == static_cast<std::size_t>(-1) ||
        weight_mul_word == static_cast<std::size_t>(-1))
        return false;

    // 1) rDiffuse = SpecTex.a * workflowDiffuse
    //    -> rDiffuse = 1 * workflowDiffuse
    words[alpha_mul_word + 3u] = 0x00004001u;
    words[alpha_mul_word + 4u] = 0x3f800000u;

    // 2) rDiffuse *= (1 - workflowWeight)
    //    -> rDiffuse *= 1
    words[weight_mul_word + 3u] = 0x00004001u;
    words[weight_mul_word + 4u] = 0x3f800000u;

    return
        words[alpha_mul_word + 1u] == 0x00100072u &&
        words[alpha_mul_word + 2u] == diffuse_register &&
        words[alpha_mul_word + 3u] == 0x00004001u &&
        words[alpha_mul_word + 4u] == 0x3f800000u &&
        words[weight_mul_word + 1u] == 0x00100072u &&
        words[weight_mul_word + 2u] == diffuse_register &&
        words[weight_mul_word + 3u] == 0x00004001u &&
        words[weight_mul_word + 4u] == 0x3f800000u;
}
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R17_PTDE_DIFFUSE_MATERIAL)
// R17 ports the inner PTDE DiffuseMaterial producer instead of tuning the
// Remaster workflow weight.
//
// Exact PTDE stable P_Metal forms the diffuse material directly:
//   DiffuseMaterial = (DiffuseMap + c156) * c100 * COLOR0
// and feeds that material into the common EnvDiffuse/hemisphere term.
//
// In stock DSR's MaterialWorkflow true branch the corresponding material
// factor is instead:
//   delta            = cb0[10] - cb0[9]
//   workflowMaterial = workflowWeight * delta + cb0[9]
//
// The generic MR bridge already remaps only the MAD base cb0[9] operand to
// b12[1] (exact PTDE c100) and removes the diffuse ^2.2 transfer. Therefore,
// before R17, the active true branch is a nonhomologous hybrid:
//   workflowMaterial = b12[1] + W * (cb0[10] - stock_cb0[9])
//
// R16 proved that exposing this hybrid after removing the outer alpha/weight
// gates is overbright. R17 does not fit a gain: it selects the exact PTDE
// producer by forcing the MAD interpolation coefficient to zero, yielding:
//   workflowMaterial = b12[1]
// The existing diffuse texture/COLOR0 carrier and downstream PTDE common
// composition remain untouched.
bool select_ptde_diffuse_material_factor_r17(
    std::vector<std::uint32_t> &words) noexcept
{
    std::vector<instruction_view> instructions;
    if (!decode(words, instructions))
        return false;

    std::size_t material_if = static_cast<std::size_t>(-1);
    std::size_t sample_hits = 0u;

    for (std::size_t i = 0u; i + 1u < instructions.size(); ++i) {
        const auto &sample = instructions[i];
        const auto &branch = instructions[i + 1u];

        if (sample.opcode < 0x45u || sample.opcode > 0x4au ||
            sample.length != 11u ||
            sample.offset + 10u >= words.size() ||
            words[sample.offset + 7u] != 0x00107936u ||
            words[sample.offset + 8u] != 1u ||
            words[sample.offset + 10u] != 1u ||
            branch.opcode != 0x1fu ||
            branch.length != 4u ||
            branch.offset != sample.offset + sample.length ||
            branch.offset + 3u >= words.size() ||
            words[branch.offset + 1u] != 0x0020800au ||
            words[branch.offset + 2u] != 0u ||
            words[branch.offset + 3u] != 85u)
            continue;

        ++sample_hits;
        material_if = i + 1u;
    }

    if (sample_hits != 1u ||
        material_if == static_cast<std::size_t>(-1))
        return false;

    std::size_t depth = 0u;
    std::size_t mad_word = static_cast<std::size_t>(-1);
    std::uint32_t material_register = 0u;
    std::uint32_t diffuse_carrier_register = 0u;
    std::size_t hits = 0u;

    for (std::size_t i = material_if + 1u; i < instructions.size(); ++i) {
        const auto &add = instructions[i];

        if (add.opcode == 0x1fu) {
            ++depth;
            continue;
        }
        if (add.opcode == 0x12u && depth == 0u)
            break;
        if (add.opcode == 0x15u) {
            if (depth == 0u)
                break;
            --depth;
            continue;
        }

        if (depth != 0u ||
            add.opcode != 0x00u ||
            add.length != 10u ||
            add.offset + 9u >= words.size() ||
            words[add.offset + 1u] != 0x00100072u ||
            // -cb0[9].xyz
            words[add.offset + 3u] != 0x80208246u ||
            words[add.offset + 4u] != 0x00000041u ||
            words[add.offset + 5u] != 0u ||
            words[add.offset + 6u] != 9u ||
            // +cb0[10].xyz
            words[add.offset + 7u] != 0x00208246u ||
            words[add.offset + 8u] != 0u ||
            words[add.offset + 9u] != 10u)
            continue;

        if (i + 2u >= instructions.size())
            return false;

        const auto &mad = instructions[i + 1u];
        const auto &mul = instructions[i + 2u];
        const auto dst = words[add.offset + 2u];

        if (mad.opcode != 0x32u ||
            mad.length != 10u ||
            mad.offset + 9u >= words.size() ||
            words[mad.offset + 1u] != 0x00100072u ||
            words[mad.offset + 2u] != dst ||
            // workflowWeight scalar register (yyyy); register id varies
            // across rx33/rx34/rx35 but the operand encoding is identical.
            words[mad.offset + 3u] != 0x00100556u ||
            // delta/material register
            words[mad.offset + 5u] != 0x00100246u ||
            words[mad.offset + 6u] != dst ||
            // Generic MR's exact PTDE c100 carrier: b12[1].xyz
            words[mad.offset + 7u] != 0x00208246u ||
            words[mad.offset + 8u] != 12u ||
            words[mad.offset + 9u] != 1u ||
            mul.opcode != 0x38u ||
            mul.length != 7u ||
            mul.offset + 6u >= words.size() ||
            words[mul.offset + 1u] != 0x00100072u ||
            words[mul.offset + 2u] != dst ||
            words[mul.offset + 3u] != 0x00100246u ||
            words[mul.offset + 5u] != 0x00100246u ||
            words[mul.offset + 6u] != dst)
            continue;

        ++hits;
        mad_word = mad.offset;
        material_register = dst;
        diffuse_carrier_register = words[mul.offset + 4u];
    }

    if (hits != 1u ||
        mad_word == static_cast<std::size_t>(-1))
        return false;

    // mad rMat.xyz, W, rMat.xyz, b12[1].xyz
    // -> mad rMat.xyz, 0.0, rMat.xyz, b12[1].xyz
    // -> rMat.xyz = b12[1].xyz
    //
    // Scalar register and scalar immediate are both two-DWORD operands, so
    // the DXBC layout and every downstream instruction offset stay unchanged.
    words[mad_word + 3u] = 0x00004001u;
    words[mad_word + 4u] = 0x00000000u;

    return
        words[mad_word + 1u] == 0x00100072u &&
        words[mad_word + 2u] == material_register &&
        words[mad_word + 3u] == 0x00004001u &&
        words[mad_word + 4u] == 0x00000000u &&
        words[mad_word + 5u] == 0x00100246u &&
        words[mad_word + 6u] == material_register &&
        words[mad_word + 7u] == 0x00208246u &&
        words[mad_word + 8u] == 12u &&
        words[mad_word + 9u] == 1u &&
        diffuse_carrier_register != material_register;
}
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R5)
// PTDE stable Phn HemEnv reconstructs the tangent frame per pixel. DSR HemEnv
// changed that spatial operator in two non-equivalent ways before both
// environment consumers: SV_IsFrontFace can flip the geometric normal and
// TEXCOORD5 carries a separately interpolated bitangent instead of PTDE's
// per-pixel cross. The DSR gFC_LightProbeParam.z normal MAD is deliberately
// preserved: the recovered ordinary GI/legacy providers upload z=1 exactly, so
// that operation is algebraically identity. R5 restores only the active N/T/B
// producer differences. The resulting N continues into the already-owned PTDE
// t11 EnvDiffuse path and
// the homologous R = 2*dot(N,V)*N - V path used by the PTDE t12 EnvSpec island.
bool apply_exact_ptde_normal_basis_r5(
    std::vector<std::uint8_t> &bytes) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view> instructions;
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

    std::size_t frontface_word =
        static_cast<std::size_t>(-1);
    std::size_t lightprobe_word =
        static_cast<std::size_t>(-1);
    std::size_t frontface_hits = 0u;
    std::size_t lightprobe_hits = 0u;

    constexpr std::array<std::uint32_t,13>
        k_dsr_lightprobe_normal_mad{{
            0x0d000032u,
            0x00100072u,0x00000004u,
            0x00208aa6u,0x00000000u,0x0000004fu,
            0x00100246u,0x00000004u,
            0x00004002u,
            0x00000000u,0x00000000u,
            0x3f800000u,0x00000000u
        }};

    for (const auto &ins : instructions) {
        if (ins.offset + ins.length >
                words.size())
            return false;

        // Exact DSR HemEnv front-face branch:
        //   movc r2.xyz, SV_IsFrontFace, TEXCOORD2, -TEXCOORD2
        // PTDE has no face-sign branch in the homologous Phn HemEnv path.
        if (ins.opcode == 0x37u &&
            ins.length == 10u &&
            words[ins.offset + 1u] ==
                0x00100072u &&
            words[ins.offset + 2u] == 2u &&
            words[ins.offset + 3u] ==
                0x00101006u &&
            words[ins.offset + 5u] ==
                0x00101246u &&
            words[ins.offset + 7u] ==
                0x80101246u &&
            words[ins.offset + 8u] ==
                0x00000041u &&
            words[ins.offset + 6u] ==
                words[ins.offset + 9u]) {
            ++frontface_hits;
            frontface_word = ins.offset;
        }

        if (ins.length ==
                k_dsr_lightprobe_normal_mad.size() &&
            std::equal(
                k_dsr_lightprobe_normal_mad.begin(),
                k_dsr_lightprobe_normal_mad.end(),
                words.begin() +
                    static_cast<std::ptrdiff_t>(
                        ins.offset))) {
            ++lightprobe_hits;
            lightprobe_word = ins.offset;
        }
    }

    if (frontface_hits != 1u ||
        lightprobe_hits != 1u ||
        frontface_word ==
            static_cast<std::size_t>(-1) ||
        lightprobe_word <
            48u)
        return false;

    const auto normal_input =
        words[frontface_word + 6u];

    // Immediately before the DSR-only LightProbeParam MAD are the exact
    // normal-map "-1" ADD and the independently interpolated bitangent
    // normalization. One tangent normalization block precedes that.
    const auto minus_one_word =
        lightprobe_word - 10u;
    const auto bitangent_word =
        minus_one_word - 19u;
    const auto tangent_word =
        bitangent_word - 19u;

    constexpr std::array<std::uint32_t,10>
        k_minus_one{{
            0x0a000000u,
            0x00100072u,0x00000004u,
            0x00100246u,0x00000004u,
            0x00004002u,
            0xbf800000u,0xbf800000u,
            0xbf800000u,0x00000000u
        }};

    if (minus_one_word +
            k_minus_one.size() >
                words.size() ||
        !std::equal(
            k_minus_one.begin(),
            k_minus_one.end(),
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    minus_one_word)))
        return false;

    // DSR tangent normalization:
    //   dp3 r1.w, vT.xyz, vT.xyz
    //   rsq r1.w, r1.w
    //   mul r5.xyz, r1.w, vT.yzx
    if (tangent_word + 19u >
            words.size() ||
        words[tangent_word] !=
            0x07000010u ||
        words[tangent_word + 1u] !=
            0x00100082u ||
        words[tangent_word + 2u] != 1u ||
        words[tangent_word + 3u] !=
            0x00101246u ||
        words[tangent_word + 5u] !=
            0x00101246u ||
        words[tangent_word + 4u] !=
            words[tangent_word + 6u] ||
        words[tangent_word + 7u] !=
            0x05000044u ||
        words[tangent_word + 8u] !=
            0x00100082u ||
        words[tangent_word + 9u] != 1u ||
        words[tangent_word + 10u] !=
            0x0010003au ||
        words[tangent_word + 11u] != 1u ||
        words[tangent_word + 12u] !=
            0x07000038u ||
        words[tangent_word + 13u] !=
            0x00100072u ||
        words[tangent_word + 14u] != 5u ||
        words[tangent_word + 15u] !=
            0x00100ff6u ||
        words[tangent_word + 16u] != 1u ||
        words[tangent_word + 17u] !=
            0x00101496u ||
        words[tangent_word + 18u] !=
            words[tangent_word + 4u])
        return false;

    const auto tangent_input =
        words[tangent_word + 4u];

    // DSR HemEnv-only interpolated bitangent:
    //   dp3 r1.w, vB.xyz, vB.xyz
    //   rsq r1.w, r1.w
    //   mul r6.xyz, r1.w, vB.xyz
    if (bitangent_word + 19u >
            words.size() ||
        words[bitangent_word] !=
            0x07000010u ||
        words[bitangent_word + 1u] !=
            0x00100082u ||
        words[bitangent_word + 2u] != 1u ||
        words[bitangent_word + 3u] !=
            0x00101246u ||
        words[bitangent_word + 5u] !=
            0x00101246u ||
        words[bitangent_word + 4u] !=
            words[bitangent_word + 6u] ||
        words[bitangent_word + 7u] !=
            0x05000044u ||
        words[bitangent_word + 8u] !=
            0x00100082u ||
        words[bitangent_word + 9u] != 1u ||
        words[bitangent_word + 10u] !=
            0x0010003au ||
        words[bitangent_word + 11u] != 1u ||
        words[bitangent_word + 12u] !=
            0x07000038u ||
        words[bitangent_word + 13u] !=
            0x00100072u ||
        words[bitangent_word + 14u] != 6u ||
        words[bitangent_word + 15u] !=
            0x00100ff6u ||
        words[bitangent_word + 16u] != 1u ||
        words[bitangent_word + 17u] !=
            0x00101246u ||
        words[bitangent_word + 18u] !=
            words[bitangent_word + 4u])
        return false;

    const auto bitangent_input =
        words[bitangent_word + 4u];

    if (normal_input == tangent_input ||
        normal_input == bitangent_input ||
        tangent_input == bitangent_input)
        return false;

    // Exact legacy/PTDE bitangent construction, using the raw interpolated
    // geometric normal and tangent plus tangent.w handedness:
    //   B = cross(N0, T) * handedness
    //   B = normalize(B)
    //
    // Use r6 and r1.w, which are exactly the scratch locations already owned
    // by the DSR bitangent-normalization window being replaced.
    const std::array<std::uint32_t,43>
        ptde_bitangent{{
            0x07000038u,
            0x00100072u,0x00000006u,
            0x00101926u,normal_input,
            0x00101496u,tangent_input,

            0x0a000032u,
            0x00100072u,0x00000006u,
            0x00101496u,normal_input,
            0x00101926u,tangent_input,
            0x80100246u,0x00000041u,
            0x00000006u,

            0x07000038u,
            0x00100072u,0x00000006u,
            0x00100246u,0x00000006u,
            0x00101ff6u,tangent_input,

            0x07000010u,
            0x00100082u,0x00000001u,
            0x00100246u,0x00000006u,
            0x00100246u,0x00000006u,

            0x05000044u,
            0x00100082u,0x00000001u,
            0x0010003au,0x00000001u,

            0x07000038u,
            0x00100072u,0x00000006u,
            0x00100ff6u,0x00000001u,
            0x00100246u,0x00000006u
        }};

    // Neutralize only the DSR-only face sign while retaining the exact
    // instruction footprint: the false source keeps its extended operand but
    // changes NEG -> NONE, so both branches are raw TEXCOORD2.
    words[frontface_word + 8u] =
        0x00000001u;

    // Preserve the DSR LightProbeParam.z MAD byte-for-byte. The recovered
    // ordinary HemEnv providers set z=1 exactly, so the sequence reduces to
    // the same decoded tangent normal as PTDE and is not an active mismatch.

    // Replace the 19-DWORD interpolated-B normalization with the 43-DWORD
    // PTDE per-pixel cross. This is the only size-changing R5 edit.
    try {
        words.erase(
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    bitangent_word),
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    bitangent_word + 19u));
        words.insert(
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    bitangent_word),
            ptde_bitangent.begin(),
            ptde_bitangent.end());
    } catch (...) {
        return false;
    }

    words[1] =
        static_cast<std::uint32_t>(
            words.size());

    // Exact postconditions. The size-changing edit lies before the unchanged
    // LightProbeParam identity sequence, so its verified anchor shifts +24.
    constexpr std::size_t
        k_bitangent_growth = 43u - 19u;
    const auto shifted_lightprobe =
        lightprobe_word + k_bitangent_growth;

    if (words[frontface_word + 8u] !=
            0x00000001u ||
        bitangent_word +
                ptde_bitangent.size() >
            words.size() ||
        !std::equal(
            ptde_bitangent.begin(),
            ptde_bitangent.end(),
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    bitangent_word)) ||
        shifted_lightprobe +
                k_dsr_lightprobe_normal_mad.size() >
            words.size() ||
        !std::equal(
            k_dsr_lightprobe_normal_mad.begin(),
            k_dsr_lightprobe_normal_mad.end(),
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    shifted_lightprobe)))
        return false;

    std::vector<instruction_view>
        patched_instructions;
    if (!decode(
            words,
            patched_instructions))
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
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW)
// R7 restores the remaining upstream PTDE Visibility_P shadow producer on the
// exact stable Csd/Sdw P_Metal receivers. DSR rx33/rx34 replaced the legacy
// packed-RGB 4x4/16-tap kernel with a comparison-sampler 3x3/9-SAMPLE_C
// kernel. Fixed-light shaders in the same FRPG_Phn_DifSpcBmp family retain
// the PTDE-style producer and provide the exact DXBC semantics used here.
//
// This patch owns only the shadow sampling/comparison island. Projection,
// normal-bias, distance fade, ShadowColor and the already-recovered downstream
// visibility/common-merge remain unchanged. Plain rx35 has no Csd/Sdw shadow
// branch and is deliberately left byte-identical at this operator cut.
bool patch_ptde_shadow_r7_rdef(
    std::vector<chunk> &chunks) noexcept
{
    auto *rdef = unique_rdef(chunks);
    if (rdef == nullptr ||
        rdef->payload.size() < 16u)
        return false;

    auto &payload = rdef->payload;
    const auto count =
        read_u32(payload.data() + 8u);
    const auto offset =
        read_u32(payload.data() + 12u);
    constexpr std::uint32_t
        k_binding_size = 32u;

    if (count == 0u ||
        count > 256u ||
        offset > payload.size() ||
        static_cast<std::uint64_t>(count) *
                k_binding_size >
            payload.size() - offset)
        return false;

    std::size_t sampler7_hits = 0u;
    std::size_t texture7_hits = 0u;

    for (std::uint32_t i = 0u;
         i < count;
         ++i) {
        const auto at =
            offset +
            static_cast<std::size_t>(i) *
                k_binding_size;
        const auto type =
            read_u32(payload.data() + at + 4u);
        const auto dimension =
            read_u32(payload.data() + at + 12u);
        const auto bind =
            read_u32(payload.data() + at + 20u);
        const auto bind_count =
            read_u32(payload.data() + at + 24u);
        const auto flags =
            read_u32(payload.data() + at + 28u);

        if (bind != 7u ||
            bind_count != 1u)
            continue;

        if (type == 3u) {
            // Retail no-point/PntS stable Csd/Sdw declares s7 as
            // USERPACKED|COMPARISON_SAMPLER (flags=3). R7's manual SAMPLE
            // kernel requires the legacy regular sampler declaration (flags=1).
            if (flags != 3u)
                return false;
            write_u32(
                payload.data() + at + 28u,
                1u);
            ++sampler7_hits;
        } else if (
            type == 2u &&
            dimension == 4u &&
            flags == 13u) {
            // The logical t7 2D resource ABI is invariant across DSR's
            // comparison and legacy/manual branches; do not rewrite it.
            ++texture7_hits;
        }
    }

    return
        sampler7_hits == 1u &&
        texture7_hits == 1u;
}

void append_ptde_shadow_r7_row(
    std::vector<std::uint32_t> &out,
    std::uint32_t base_register,
    std::uint32_t y,
    std::uint32_t row_index)
{
    constexpr std::uint32_t k_coord = 13u;
    constexpr std::uint32_t k_depth = 14u;
    constexpr std::uint32_t k_sample = 15u;
    constexpr std::uint32_t k_accum = 16u;
    constexpr std::uint32_t k_row = 17u;

    constexpr std::uint32_t k_neg_1_5_over_2048 = 0xba400000u;
    constexpr std::uint32_t k_neg_0_5_over_2048 = 0xb9800000u;
    constexpr std::uint32_t k_pos_0_5_over_2048 = 0x39800000u;
    constexpr std::uint32_t k_pos_1_5_over_2048 = 0x3a400000u;

    const auto add_coords =
        [&](std::uint32_t x0,
            std::uint32_t x1) {
            const std::array<std::uint32_t,10> words{{
                0x0a000000u,
                0x001000f2u,k_coord,
                0x00100446u,base_register,
                0x00004002u,
                x0,y,x1,y
            }};
            out.insert(
                out.end(),
                words.begin(),
                words.end());
        };

    const auto sample =
        [&](std::uint32_t coord_token) {
            const std::array<std::uint32_t,11> words{{
                0x8b000045u,
                0x800000c2u,0x00155543u,
                0x00100072u,k_sample,
                coord_token,k_coord,
                0x00107e46u,7u,
                0x00106000u,7u
            }};
            out.insert(
                out.end(),
                words.begin(),
                words.end());
        };

    const auto decode_depth =
        [&](std::uint32_t destination_token) {
            const std::array<std::uint32_t,10> words{{
                0x0a000010u,
                destination_token,k_depth,
                0x00100246u,k_sample,
                0x00004002u,
                0x3f7f0000u,
                0x3b7f0000u,
                0x377f0000u,
                0x00000000u
            }};
            out.insert(
                out.end(),
                words.begin(),
                words.end());
        };

    add_coords(
        k_neg_1_5_over_2048,
        k_neg_0_5_over_2048);
    sample(0x00100046u);
    decode_depth(0x00100012u);
    sample(0x00100ae6u);
    decode_depth(0x00100022u);

    add_coords(
        k_pos_0_5_over_2048,
        k_pos_1_5_over_2048);
    sample(0x00100046u);
    decode_depth(0x00100042u);
    sample(0x00100ae6u);
    decode_depth(0x00100082u);

    const std::array<std::uint32_t,7> lt{{
        0x07000031u,
        0x001000f2u,k_coord,
        0x00100e46u,k_depth,
        0x00100aa6u,base_register
    }};
    out.insert(
        out.end(),
        lt.begin(),
        lt.end());

    const std::array<std::uint32_t,10> and_one{{
        0x0a000001u,
        0x001000f2u,k_coord,
        0x00100e46u,k_coord,
        0x00004002u,
        0x3f800000u,0x3f800000u,
        0x3f800000u,0x3f800000u
    }};
    out.insert(
        out.end(),
        and_one.begin(),
        and_one.end());

    const auto row_destination =
        row_index == 0u
            ? k_accum
            : k_row;
    const std::array<std::uint32_t,10> dp4{{
        0x0a000011u,
        0x00100082u,row_destination,
        0x00100e46u,k_coord,
        0x00004002u,
        0x3d800000u,0x3d800000u,
        0x3d800000u,0x3d800000u
    }};
    out.insert(
        out.end(),
        dp4.begin(),
        dp4.end());

    if (row_index != 0u) {
        const std::array<std::uint32_t,7> add_row{{
            0x07000000u,
            0x00100082u,k_accum,
            0x0010003au,k_accum,
            0x0010003au,k_row
        }};
        out.insert(
            out.end(),
            add_row.begin(),
            add_row.end());
    }
}

bool apply_exact_ptde_shadow_visibility_r7(
    std::vector<std::uint8_t> &bytes,
    const pmetal_rgba_authority::entry
        &authority) noexcept
{
    if (authority.receiver_id == 35u)
        return true;

    if (authority.receiver_id != 33u &&
        authority.receiver_id != 34u)
        return false;

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view> instructions;
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

    std::size_t sampler_decl =
        static_cast<std::size_t>(-1);
    std::size_t sampler_decl_hits = 0u;
    std::size_t temps_decl =
        static_cast<std::size_t>(-1);
    std::size_t temps_decl_hits = 0u;
    std::vector<std::size_t>
        sample_c_words;
    std::uint32_t base_register =
        std::numeric_limits<std::uint32_t>::
            max();

    for (const auto &ins : instructions) {
        if (ins.opcode == 0x5au &&
            ins.length == 3u &&
            ins.offset + 2u < words.size() &&
            words[ins.offset + 2u] == 7u) {
            if (words[ins.offset] !=
                    0x0300085au ||
                words[ins.offset + 1u] !=
                    0x00106000u)
                return false;
            ++sampler_decl_hits;
            sampler_decl = ins.offset;
        }

        if (ins.opcode == 0x68u &&
            ins.length == 2u) {
            ++temps_decl_hits;
            temps_decl = ins.offset;
        }

        if (ins.opcode != 0x46u ||
            ins.length != 14u ||
            ins.offset + 13u >=
                words.size())
            continue;

        if (words[ins.offset + 8u] !=
                0x00107006u ||
            words[ins.offset + 9u] != 7u ||
            words[ins.offset + 10u] !=
                0x00106000u ||
            words[ins.offset + 11u] != 7u)
            continue;

        if (words[ins.offset + 6u] !=
                0x00100046u ||
            words[ins.offset + 12u] !=
                0x0010002au ||
            words[ins.offset + 7u] !=
                words[ins.offset + 13u])
            return false;

        const auto this_base =
            words[ins.offset + 7u];

        if (base_register ==
                std::numeric_limits<
                    std::uint32_t>::max())
            base_register = this_base;
        else if (base_register !=
                    this_base)
            return false;

        sample_c_words.push_back(
            ins.offset);
    }

    if (sampler_decl_hits != 1u ||
        temps_decl_hits != 1u ||
        sampler_decl ==
            static_cast<std::size_t>(-1) ||
        temps_decl ==
            static_cast<std::size_t>(-1) ||
        sample_c_words.size() != 9u ||
        base_register ==
            std::numeric_limits<
                std::uint32_t>::max() ||
        (authority.receiver_id == 33u &&
         base_register != 6u) ||
        (authority.receiver_id == 34u &&
         base_register != 5u) ||
        words[temps_decl + 1u] != 13u)
        return false;

    const auto first_sample =
        sample_c_words.front();
    const auto last_sample =
        sample_c_words.back();

    std::size_t terminal_min =
        static_cast<std::size_t>(-1);
    std::size_t terminal_min_hits = 0u;

    for (const auto &ins : instructions) {
        if (ins.offset <= last_sample ||
            ins.offset >
                last_sample + 64u ||
            ins.opcode != 0x33u ||
            ins.length != 7u ||
            ins.offset + 6u >= words.size())
            continue;

        if (words[ins.offset] ==
                0x07000033u &&
            words[ins.offset + 1u] ==
                0x00100082u &&
            words[ins.offset + 2u] == 2u &&
            words[ins.offset + 3u] ==
                0x0010003au &&
            words[ins.offset + 4u] == 2u &&
            words[ins.offset + 5u] ==
                0x00004001u &&
            words[ins.offset + 6u] ==
                0x3f800000u) {
            ++terminal_min_hits;
            terminal_min = ins.offset;
        }
    }

    if (terminal_min_hits != 1u ||
        terminal_min <
            first_sample)
        return false;

    std::vector<std::uint32_t>
        replacement;

    try {
        replacement.reserve(559u);

        constexpr std::array<std::uint32_t,4>
            k_y{{
                0xba400000u,
                0xb9800000u,
                0x39800000u,
                0x3a400000u
            }};

        for (std::uint32_t row = 0u;
             row < k_y.size();
             ++row)
            append_ptde_shadow_r7_row(
                replacement,
                base_register,
                k_y[row],
                row);

        const std::array<std::uint32_t,7>
            add_visibility{{
                0x07000000u,
                0x00100082u,2u,
                0x0010003au,2u,
                0x0010003au,16u
            }};
        replacement.insert(
            replacement.end(),
            add_visibility.begin(),
            add_visibility.end());

        const std::array<std::uint32_t,7>
            sat_visibility{{
                0x07000033u,
                0x00100082u,2u,
                0x0010003au,2u,
                0x00004001u,0x3f800000u
            }};
        replacement.insert(
            replacement.end(),
            sat_visibility.begin(),
            sat_visibility.end());
    } catch (...) {
        return false;
    }

    if (replacement.size() != 559u)
        return false;

    // Switch the shader declaration itself from comparison to regular s7 and
    // expose five fresh scratch registers r13..r17. These registers are above
    // the stock/R6B dcl_temps=13 range and therefore cannot alias live values.
    words[sampler_decl] =
        0x0300005au;
    words[temps_decl + 1u] =
        18u;

    try {
        words.erase(
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    first_sample),
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    terminal_min + 7u));
        words.insert(
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    first_sample),
            replacement.begin(),
            replacement.end());
    } catch (...) {
        return false;
    }

    if (words.size() >
            std::numeric_limits<
                std::uint32_t>::max())
        return false;

    words[1] =
        static_cast<std::uint32_t>(
            words.size());

    if (!patch_ptde_shadow_r7_rdef(
            chunks))
        return false;

    std::vector<instruction_view>
        patched;
    if (!decode(
            words,
            patched))
        return false;

    std::size_t regular_s7_decl = 0u;
    std::size_t t7_sample = 0u;
    std::size_t t7_sample_c = 0u;
    std::size_t temps18 = 0u;

    for (const auto &ins : patched) {
        if (ins.opcode == 0x5au &&
            ins.length == 3u &&
            ins.offset + 2u < words.size() &&
            words[ins.offset + 2u] == 7u &&
            words[ins.offset] ==
                0x0300005au)
            ++regular_s7_decl;

        if (ins.opcode == 0x68u &&
            ins.length == 2u &&
            words[ins.offset + 1u] >= 18u)
            ++temps18;

        if (ins.opcode == 0x45u &&
            ins.length == 11u &&
            ins.offset + 10u < words.size() &&
            words[ins.offset + 7u] ==
                0x00107e46u &&
            words[ins.offset + 8u] == 7u &&
            words[ins.offset + 9u] ==
                0x00106000u &&
            words[ins.offset + 10u] == 7u)
            ++t7_sample;

        if (ins.opcode == 0x46u &&
            ins.length == 14u &&
            ins.offset + 11u < words.size() &&
            words[ins.offset + 8u] ==
                0x00107006u &&
            words[ins.offset + 9u] == 7u &&
            words[ins.offset + 10u] ==
                0x00106000u &&
            words[ins.offset + 11u] == 7u)
            ++t7_sample_c;
    }

    if (regular_s7_decl != 1u ||
        temps18 != 1u ||
        t7_sample != 16u ||
        t7_sample_c != 0u ||
        std::count(
            words.begin(),
            words.end(),
            0x3f7f0000u) < 16 ||
        std::count(
            words.begin(),
            words.end(),
            0x3b7f0000u) < 16 ||
        std::count(
            words.begin(),
            words.end(),
            0x377f0000u) < 16)
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

    bytes =
        std::move(rebuilt);
    return true;
}
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R10E_ATMOS_DOMAIN)
template <std::size_t N>
bool r10e_unique_sequence_offset(
    const std::vector<std::uint32_t> &words,
    const std::array<std::uint32_t,N> &pattern,
    std::size_t &offset) noexcept
{
    offset = static_cast<std::size_t>(-1);
    if (words.size() < N)
        return false;

    for (std::size_t i = 0u;
         i + N <= words.size();
         ++i) {
        if (!std::equal(
                pattern.begin(),
                pattern.end(),
                words.begin() +
                    static_cast<std::ptrdiff_t>(i)))
            continue;

        if (offset !=
                static_cast<std::size_t>(-1))
            return false;

        offset = i;
    }

    return
        offset !=
            static_cast<std::size_t>(-1);
}

// Exact stock-DSR stable HemEnv atmosphere-domain sandwich shared by
// rx33/rx34/rx35. DSR converts the surface to root-domain before Fog, then
// returns the result through a 2.2 continuation after LightScattering.
// PTDE keeps the common PHN surface in its legacy domain through both stages.
// R10E therefore recovers linear/legacy FogRGB from stock cb0[12].rgb,
// preserves the existing Fog weight + LightScattering kernel, and removes
// only the nonhomologous post-LightScattering 2.2 continuation.
//
// Crucially, this uses the stock DSR Fog carrier cb0[12] directly. It does NOT
// alias the cumulative P_Metal b12 ABI, whose row0 carries c101/k135.
constexpr std::array<std::uint32_t,21>
    k_r10e_stock_prefog{{
        0x0600002fu,
        0x00100072u,0x00000001u,
        0x80100246u,0x00000081u,0x00000001u,
        0x0a000038u,
        0x00100072u,0x00000001u,
        0x00100246u,0x00000001u,
        0x00004002u,
        0x3ee8ba2fu,0x3ee8ba2fu,0x3ee8ba2fu,0x00000000u,
        0x05000019u,
        0x00100072u,0x00000001u,
        0x00100246u,0x00000001u
    }};

constexpr std::array<std::uint32_t,21>
    k_r10e_ptde_prefog{{
        0x0600002fu,
        0x00100072u,0x00000002u,
        0x00208246u,0x00000000u,0x0000000cu,
        0x0a000038u,
        0x00100072u,0x00000002u,
        0x00100246u,0x00000002u,
        0x00004002u,
        0x3ee8ba2fu,0x3ee8ba2fu,0x3ee8ba2fu,0x00000000u,
        0x05000019u,
        0x00100072u,0x00000002u,
        0x00100246u,0x00000002u
    }};

constexpr std::array<std::uint32_t,9>
    k_r10e_stock_fog_delta{{
        0x09000000u,
        0x00100072u,0x00000002u,
        0x80100246u,0x00000041u,0x00000001u,
        0x00208246u,0x00000000u,0x0000000cu
    }};

constexpr std::array<std::uint32_t,9>
    k_r10e_ptde_fog_delta{{
        0x09000000u,
        0x00100072u,0x00000002u,
        0x80100246u,0x00000041u,0x00000001u,
        0x80100246u,0x00000081u,0x00000002u
    }};

constexpr std::array<std::uint32_t,21>
    k_r10e_stock_post_ls_pow{{
        0x0600002fu,
        0x00100072u,0x00000000u,
        0x80100246u,0x00000081u,0x00000000u,
        0x0a000038u,
        0x00100072u,0x00000000u,
        0x00100246u,0x00000000u,
        0x00004002u,
        0x400ccccdu,0x400ccccdu,0x400ccccdu,0x00000000u,
        0x05000019u,
        0x00100072u,0x00000000u,
        0x00100246u,0x00000000u
    }};

bool r10e_atmosphere_domain_exact(
    const std::vector<std::uint32_t> &words) noexcept
{
    std::size_t prefog = 0u;
    std::size_t fog_delta = 0u;
    std::size_t stock_prefog = 0u;
    std::size_t stock_fog_delta = 0u;
    std::size_t stock_post = 0u;

    const bool ptde_prefog =
        r10e_unique_sequence_offset(
            words,
            k_r10e_ptde_prefog,
            prefog);
    const bool ptde_fog_delta =
        r10e_unique_sequence_offset(
            words,
            k_r10e_ptde_fog_delta,
            fog_delta);

    const bool has_stock_prefog =
        r10e_unique_sequence_offset(
            words,
            k_r10e_stock_prefog,
            stock_prefog);
    const bool has_stock_fog_delta =
        r10e_unique_sequence_offset(
            words,
            k_r10e_stock_fog_delta,
            stock_fog_delta);
    const bool has_stock_post =
        r10e_unique_sequence_offset(
            words,
            k_r10e_stock_post_ls_pow,
            stock_post);

    return
        ptde_prefog &&
        ptde_fog_delta &&
        prefog < fog_delta &&
        !has_stock_prefog &&
        !has_stock_fog_delta &&
        !has_stock_post;
}

bool apply_exact_ptde_atmosphere_domain_r10e(
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

    std::size_t prefog = 0u;
    std::size_t fog_delta = 0u;
    std::size_t post_ls_pow = 0u;

    if (!r10e_unique_sequence_offset(
            words,
            k_r10e_stock_prefog,
            prefog) ||
        !r10e_unique_sequence_offset(
            words,
            k_r10e_stock_fog_delta,
            fog_delta) ||
        !r10e_unique_sequence_offset(
            words,
            k_r10e_stock_post_ls_pow,
            post_ls_pow) ||
        !(prefog < fog_delta &&
          fog_delta < post_ls_pow))
        return false;

    std::copy(
        k_r10e_ptde_prefog.begin(),
        k_r10e_ptde_prefog.end(),
        words.begin() +
            static_cast<std::ptrdiff_t>(prefog));
    std::copy(
        k_r10e_ptde_fog_delta.begin(),
        k_r10e_ptde_fog_delta.end(),
        words.begin() +
            static_cast<std::ptrdiff_t>(fog_delta));
    std::fill_n(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                post_ls_pow),
        k_r10e_stock_post_ls_pow.size(),
        0x0100003au);

    if (!r10e_atmosphere_domain_exact(
            words))
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
#endif

bool apply_linear_envdiffuse_consumer_diag(
    std::vector<std::uint8_t> &bytes) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::vector<instruction_view> instructions;
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

    // Exact DSR stable Phn HemEnv EnvDiffuse source island on rx33/34/35:
    //   mul r2.xyz, cb0[3].xyz, cb0[79].x
    //   sample_l ..., t11, s11
    //   mul r2.xyz, r2.xyz, sampled_envdiffuse.xyz
    //
    // Diagnostic replacement keeps the native DSR t11 field and downstream
    // composition but feeds the endpoint recovered at the exact profile
    // packer pre-draw-gain cut through b12[3].xyz:
    //   mov r2.xyz, b12[3].xyz
    //
    // The 9-DWORD window is length-preserving; any topology mismatch fails
    // open instead of guessing another EnvDiffuse producer.
    constexpr std::array<std::uint32_t,9>
        k_stock_source{{
            0x09000038u,
            0x00100072u,0x00000002u,
            0x00208246u,0x00000000u,0x00000003u,
            0x00208006u,0x00000000u,0x0000004fu
        }};

    constexpr std::array<std::uint32_t,9>
        k_linear_source{{
            0x06000036u,
            0x00100072u,0x00000002u,
            0x00208246u,0x0000000cu,0x00000003u,
            0x0100003au,0x0100003au,0x0100003au
        }};

    std::size_t t11_hits = 0u;
    std::size_t source_word =
        static_cast<std::size_t>(-1);

    for (std::size_t i = 0u;
         i < instructions.size();
         ++i) {
        const auto &ins =
            instructions[i];

        if (ins.opcode < 0x45u ||
            ins.opcode > 0x4au ||
            ins.length != 13u ||
            ins.offset + 12u >= words.size() ||
            words[ins.offset + 8u] != 11u ||
            words[ins.offset + 10u] != 11u)
            continue;

        ++t11_hits;

        if (i == 0u)
            return false;

        const auto &previous =
            instructions[i - 1u];

        if (previous.offset +
                previous.length !=
            ins.offset ||
            previous.length !=
                k_stock_source.size() ||
            previous.offset +
                k_stock_source.size() >
                    words.size() ||
            !std::equal(
                k_stock_source.begin(),
                k_stock_source.end(),
                words.begin() +
                    static_cast<std::ptrdiff_t>(
                        previous.offset)))
            return false;

        source_word =
            previous.offset;
    }

    if (t11_hits != 1u ||
        source_word ==
            static_cast<std::size_t>(-1))
        return false;

    std::copy(
        k_linear_source.begin(),
        k_linear_source.end(),
        words.begin() +
            static_cast<std::ptrdiff_t>(
                source_word));

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

// R8 restores the exact previously recovered P_Metal PRE receiver before
// Build131: V2.28A removes the DSR SPEC-domain pow(2.2) transfer and
// V2.28B removes the DSR-only angular/horizon response. These are upstream
// material-response operators and must precede the C/D/F final-tail rewrite.
bool apply_pmetal_pre_ab(
    std::vector<std::uint32_t> &words,
    const pmetal_rgba_authority::entry
        &authority) noexcept
{
    constexpr std::uint32_t k_nop =
        0x0100003au;
    constexpr std::uint32_t k_f22 =
        0x400ccccdu;
    constexpr std::uint32_t k_f13 =
        0x3fa66666u;
    constexpr std::uint32_t k_f1 =
        0x3f800000u;

    // V2.28A: exact P_Metal SPEC-domain pow(2.2) -> identity.
    // Preserve the pre-PBL SpecTex*cb10 carrier and replace only
    // LOG -> MUL(2.2) -> EXP with MOV at the original EXP cut.
    const auto log_word =
        authority.spec_log_word;
    const auto gamma_word =
        authority.spec_gamma_mul_word;
    const auto exp_word =
        authority.spec_exp_word;

    if (log_word == 0u ||
        gamma_word <= log_word ||
        exp_word <= gamma_word ||
        exp_word + 5u > words.size() ||
        (words[log_word] & 0x7ffu) != 0x2fu ||
        ((words[log_word] >> 24u) & 0x7fu) != 5u ||
        (words[gamma_word] & 0x7ffu) != 0x38u ||
        ((words[gamma_word] >> 24u) & 0x7fu) != 10u ||
        words[gamma_word + 6u] != k_f22 ||
        words[gamma_word + 7u] != k_f22 ||
        words[gamma_word + 8u] != k_f22 ||
        (words[exp_word] & 0x7ffu) != 0x19u ||
        ((words[exp_word] >> 24u) & 0x7fu) != 5u)
        return false;

    std::array<std::uint32_t,5>
        exp_to_mov{};
    std::copy_n(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                exp_word),
        exp_to_mov.size(),
        exp_to_mov.begin());

    std::fill(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                log_word),
        words.begin() +
            static_cast<std::ptrdiff_t>(
                exp_word),
        k_nop);

    exp_to_mov[0] =
        (exp_to_mov[0] & ~0x7ffu) |
        0x36u;

    std::copy(
        exp_to_mov.begin(),
        exp_to_mov.end(),
        words.begin() +
            static_cast<std::ptrdiff_t>(
                exp_word));

    // V2.28B: remove only the DSR angular/horizon response.
    // Recover the exact MAD_SAT(1 + 1.3*d) -> square -> apply topology
    // and replace the square result with scalar 1. The downstream apply
    // instruction is preserved byte-for-byte.
    std::vector<instruction_view>
        instructions;

    if (!decode(
            words,
            instructions))
        return false;

    std::size_t angular_matches = 0u;
    std::size_t angular_square = 0u;

    for (const auto &ins : instructions) {
        if (ins.opcode != 0x32u ||
            ins.length != 9u)
            continue;

        const auto begin =
            words.begin() +
            static_cast<std::ptrdiff_t>(
                ins.offset);
        const auto end =
            begin +
            static_cast<std::ptrdiff_t>(
                ins.length);

        if (std::find(
                begin,
                end,
                k_f13) == end ||
            std::find(
                begin,
                end,
                k_f1) == end)
            continue;

        const auto square_offset =
            ins.offset +
            ins.length;

        const auto square =
            std::find_if(
                instructions.begin(),
                instructions.end(),
                [&](const instruction_view &i) {
                    return
                        i.offset ==
                            square_offset &&
                        i.opcode == 0x38u &&
                        i.length == 7u;
                });

        if (square ==
            instructions.end())
            continue;

        const auto apply_offset =
            square_offset +
            square->length;

        const auto apply =
            std::find_if(
                instructions.begin(),
                instructions.end(),
                [&](const instruction_view &i) {
                    return
                        i.offset ==
                            apply_offset &&
                        i.opcode == 0x38u &&
                        i.length == 7u;
                });

        if (apply ==
            instructions.end())
            continue;

        if (words[square_offset + 4u] !=
                words[square_offset + 6u] ||
            words[square_offset + 2u] !=
                words[square_offset + 4u] ||
            words[apply_offset + 6u] !=
                words[square_offset + 2u])
            continue;

        ++angular_matches;
        angular_square =
            square_offset;
    }

    if (angular_matches != 1u)
        return false;

    const std::array<std::uint32_t,5>
        angular_mov = {{
            0x05000036u,
            words[angular_square + 1u],
            words[angular_square + 2u],
            0x00004001u,
            k_f1
        }};

    std::copy(
        angular_mov.begin(),
        angular_mov.end(),
        words.begin() +
            static_cast<std::ptrdiff_t>(
                angular_square));

    words[angular_square + 5u] =
        k_nop;
    words[angular_square + 6u] =
        k_nop;

    return decode(
        words,
        instructions);
}

// Preserve the stock r1.x live-out across the dedicated EnvSpec cut.
// Retail t12 writes only r1.yzw; r1.x is consumed downstream before its
// next write on rx33/rx34/rx35. Decode PTDE RGBA in dead scratch r12, then
// write only RGB/A into r1.yzw so activating EnvSpec cannot clobber that
// independent downstream carrier.
constexpr std::array<std::uint32_t,100>
    k_build131_window = {{
        0x8d000048u,0x80000182u,0x00155543u,0x001000f2u,0x0000000cu,0x00100796u,0x00000000u,0x00107936u,
        0x0000000cu,0x00106000u,0x0000000cu,0x00004001u,0x00000000u,0x0700000eu,0x001000e2u,0x00000001u,
        0x00100e56u,0x0000000cu,0x00100006u,0x0000000cu,0x08000038u,0x001000e2u,0x00000001u,0x00100e56u,
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

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R8_PRE_AB)
    // R8: recover the complete legacy PRE receiver before applying the
    // existing Build131 C/D/F material/resource island.
    if (!apply_pmetal_pre_ab(
            words,
            authority) ||
        !decode(
            words,
            instructions))
        return false;
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R15_PTDE_LIVEOUT_PAIR)
    if (!apply_ptde_materialworkflow_liveout_pair_r15(words) ||
        !decode(words, instructions))
        return false;
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R16_LEGACY_DIFFUSE_SPLIT)
    if (!remove_dsr_materialworkflow_legacy_diffuse_split_r16(words) ||
        !decode(words, instructions))
        return false;
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R17_PTDE_DIFFUSE_MATERIAL)
    if (!select_ptde_diffuse_material_factor_r17(words) ||
        !decode(words, instructions))
        return false;
#endif

    std::optional<instruction_view> sampler9;
    std::optional<instruction_view> texture9;
    std::optional<instruction_view> t1_decl;
    std::optional<instruction_view> t1_sample;
    std::optional<instruction_view> t12_sample;
    std::optional<instruction_view> split_sample;
    std::optional<instruction_view> split_helper;
    std::optional<instruction_view> split_mad;
    std::size_t slot10_decls = 0u;
    std::size_t slot10_samples = 0u;
    std::size_t slot14_decls = 0u;

    for (const auto &ins : instructions) {
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

            if (slot == 1u) {
                if (t1_decl)
                    return false;
                t1_decl = ins;
            }

            if (slot == 9u) {
                if (texture9)
                    return false;
                texture9 = ins;
            }

            if (slot == 10u)
                ++slot10_decls;

            if (slot == 14u)
                ++slot14_decls;
        }

        if (ins.opcode >= 0x45u &&
            ins.opcode <= 0x4au) {
            if (ins.length == 11u) {
                const auto resource =
                    words[ins.offset + 8u];

                if (resource == 1u) {
                    if (t1_sample)
                        return false;
                    t1_sample = ins;
                }

                if (resource == 10u)
                    ++slot10_samples;
            }

            if (ins.length == 13u) {
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

                // Recovered V2.28C PRE topology: the DSR t9 SAMPLE_L is
                // followed immediately by a helper MUL and split-sum MAD.
                if (resource == 9u &&
                    sampler == 9u) {
                    const auto helper_offset =
                        ins.offset + ins.length;
                    const auto helper =
                        std::find_if(
                            instructions.begin(),
                            instructions.end(),
                            [&](const instruction_view &i) {
                                return i.offset ==
                                    helper_offset;
                            });
                    if (helper ==
                            instructions.end() ||
                        helper->opcode != 0x38u ||
                        helper->length != 7u)
                        return false;

                    const auto mad_offset =
                        helper->offset +
                        helper->length;
                    const auto mad =
                        std::find_if(
                            instructions.begin(),
                            instructions.end(),
                            [&](const instruction_view &i) {
                                return i.offset ==
                                    mad_offset;
                            });
                    if (mad ==
                            instructions.end() ||
                        mad->opcode != 0x32u ||
                        mad->length != 9u ||
                        split_sample)
                        return false;

                    split_sample = ins;
                    split_helper = *helper;
                    split_mad = *mad;
                }
            }
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
        !t1_decl ||
        !t1_sample ||
        !t12_sample ||
        !split_sample ||
        !split_helper ||
        !split_mad ||
        slot10_decls != 0u ||
        slot10_samples != 0u ||
        slot14_decls != 0u ||
        t12_sample->offset !=
            authority.t12_word ||
        authority.t12_word < 38u ||
        authority.merge_word <=
            authority.t12_word ||
        authority.merge_word -
            authority.t12_word != 115u ||
        t1_decl->offset >=
            authority.t12_word ||
        !std::equal(
            k_t12_sample.begin(),
            k_t12_sample.end(),
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    t12_sample->offset)))
        return false;

    const auto build_start =
        authority.t12_word - 38u;
    const auto build_end =
        build_start +
        k_build131_window.size();

    // On the actual generic-MR base, the historical V2.28C/D/F material
    // cut is recovered structurally from the t9 SAMPLE_L -> MUL -> MAD chain.
    // Removing the split-sum yields eight final padding DWORDs immediately
    // before the material stage; Build131 ends at exactly that cut for all
    // three exact P_Metal receivers.
    if (split_mad->offset < 8u ||
        split_sample->offset < build_start ||
        split_sample->offset >= build_end ||
        split_helper->offset !=
            split_sample->offset +
                split_sample->length ||
        split_mad->offset !=
            split_helper->offset +
                split_helper->length ||
        split_mad->offset - 8u !=
            build_end ||
        build_end >= words.size())
        return false;

    const auto merge =
        std::find_if(
            instructions.begin(),
            instructions.end(),
            [&](const instruction_view &i) {
                return i.offset ==
                    authority.merge_word;
            });

    if (merge == instructions.end() ||
        merge->opcode != 0x32u ||
        merge->length != 9u ||
        words[split_mad->offset + 1u] !=
            0x00100072u ||
        words[split_mad->offset + 2u] !=
            2u ||
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

    // Csd/Sdw: find the exact DSR-only visibility exponent island by
    // instruction topology on the MR base rather than by stale stock word
    // coordinates. Plain HemEnv has no such island.
    std::optional<std::size_t>
        visibility_start;
    for (const auto &ins : instructions) {
        if (ins.opcode != 0x2fu ||
            ins.length != 6u)
            continue;

        const auto mul =
            std::find_if(
                instructions.begin(),
                instructions.end(),
                [&](const instruction_view &i) {
                    return i.offset ==
                        ins.offset + 6u;
                });
        if (mul == instructions.end() ||
            mul->opcode != 0x38u ||
            mul->length != 8u)
            continue;

        const auto exp =
            std::find_if(
                instructions.begin(),
                instructions.end(),
                [&](const instruction_view &i) {
                    return i.offset ==
                        mul->offset + 8u;
                });
        if (exp == instructions.end() ||
            exp->opcode != 0x19u ||
            exp->length != 5u)
            continue;

        if (visibility_start)
            return false;
        visibility_start =
            ins.offset;
    }

    if (authority.remove_visibility_exponent !=
            visibility_start.has_value())
        return false;

    if (visibility_start) {
        std::fill(
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    *visibility_start),
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    *visibility_start + 19u),
            0x0100003au);
    }

    // V2.28C/D/F final material normalization on the generic MR base:
    // remove the t9 sample/helper split-sum, place raw PTDE c101 at the
    // recovered Build131 end cut, then multiply by semantic COLOR0.
    std::fill(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                split_sample->offset),
        words.begin() +
            static_cast<std::ptrdiff_t>(
                split_mad->offset),
        0x0100003au);

    const std::array<std::uint32_t,8>
        c101_mul{{
            0x08000038u,
            0x00100072u,
            0x00000002u,
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R4)
            // PTDE 728/741/754 sample the exact SpecMap directly as RGB:
            //   texld s1 -> mul xyz,c101 -> mul xyz,COLOR0.
            // Once t10 is an exact PTDE SpecRGB sidecar, the stock DSR
            // SpecTex wxyz remap is no longer part of this consumer.
            0x00100246u,
#else
            // Historical Build131 compatibility for stock-encoded t1:
            // recover logical SpecRGB from sampled yzw.
            0x00100796u,
#endif
            0x00000002u,
            0x00208246u,
            0x0000000cu,
            0x00000000u
        }};

    const std::array<std::uint32_t,7>
        color_mul{{
            0x07000038u,
            0x00100072u,
            0x00000002u,
            0x00100246u,
            0x00000002u,
            0x00101246u,
            authority.color0_register
        }};

    std::copy(
        c101_mul.begin(),
        c101_mul.end(),
        words.begin() +
            static_cast<std::ptrdiff_t>(
                build_end));

    std::copy(
        color_mul.begin(),
        color_mul.end(),
        words.begin() +
            static_cast<std::ptrdiff_t>(
                split_mad->offset));
    words[split_mad->offset + 7u] =
        0x0100003au;
    words[split_mad->offset + 8u] =
        0x0100003au;

    // Dedicated t10 declaration/sample. Fresh PTDE SpecRGB is reacquired
    // only at the final material cut, never beside the stock t1 sample.
    std::array<std::uint32_t,4>
        t10_decl{};
    std::copy_n(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                t1_decl->offset),
        4u,
        t10_decl.begin());
    t10_decl[2] = 10u;

    std::array<std::uint32_t,11>
        fresh_spec{};
    std::copy_n(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                t1_sample->offset),
        11u,
        fresh_spec.begin());
    // The stock DSR t1 SAMPLE operand is wxyz (swizzle byte 0x93).
    // That remap belongs to the stock DSR SpecTex encoding, not to the exact
    // PTDE SpecRGB sidecar bound at t10. PTDE stable HemEnv 728/741/754 uses
    // direct texld s1 RGB. R4 therefore changes only the dedicated t10
    // consumer to identity xyzw (swizzle byte 0xE4) and writes RGB to r2.xyz.
    if (fresh_spec[7] !=
            0x00107936u)
        return false;
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R4)
    fresh_spec[3] =
        0x00100072u;
    fresh_spec[7] =
        0x00107e46u;
#else
    fresh_spec[3] =
        0x001000e2u;
#endif
    fresh_spec[4] = 2u;
    fresh_spec[8] = 10u;

    // Re-home the obsolete t9 declaration pair as the dedicated PTDE B cube.
    words[sampler9->offset + 2u] =
        14u;
    words[texture9->offset] =
        0x04003058u;
    words[texture9->offset + 2u] =
        14u;

    const auto decl_insert =
        t1_decl->offset + 4u;
    words.insert(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                decl_insert),
        t10_decl.begin(),
        t10_decl.end());

    // All semantic cuts are below the declaration insertion.
    const auto shifted_build_start =
        build_start + 4u;
    const auto shifted_build_end =
        build_end + 4u;

    auto window =
        k_build131_window;
    window[6] =
        authority.reflection_coord_register;
    window[38] =
        authority.reflection_coord_register;

    std::copy(
        window.begin(),
        window.end(),
        words.begin() +
            static_cast<std::ptrdiff_t>(
                shifted_build_start));

    // Build131 ends exactly at the dedicated final material cut. Insert the
    // fresh t10 sample there; the c101 and COLOR0 operations already prepared
    // immediately downstream shift intact by 11 DWORDs.
    words.insert(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                shifted_build_end),
        fresh_spec.begin(),
        fresh_spec.end());

    words[1] =
        static_cast<std::uint32_t>(
            words.size());

    std::vector<instruction_view>
        patched_instructions;
    if (!decode(
            words,
            patched_instructions))
        return false;

    // Exact final-tail postcondition before rebuilding/RDEF mutation.
    const auto c101_at =
        shifted_build_end +
        fresh_spec.size();
    const auto color_at =
        c101_at +
        c101_mul.size();

    if (c101_at +
            c101_mul.size() >
            words.size() ||
        color_at +
            color_mul.size() >
            words.size() ||
        !std::equal(
            c101_mul.begin(),
            c101_mul.end(),
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    c101_at)) ||
        !std::equal(
            color_mul.begin(),
            color_mul.end(),
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    color_at)))
        return false;

    if (!patch_build131_rdef(chunks))
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
    bool with_upper_lower,
    bool require_terminal_sat = true) noexcept
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
    std::size_t t10_material_sample = 0u;
    std::size_t t12_sample = 0u;
    std::size_t t12_scratch_rgba_sample = 0u;
    std::size_t t12_decode_preserve_r1x = 0u;
    std::size_t c101_material_mul = 0u;
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
                    10u) {
                ++t10_sample;
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R4)
                if (words[ins.offset + 3u] ==
                        0x00100072u &&
                    words[ins.offset + 4u] ==
                        2u &&
                    words[ins.offset + 7u] ==
                        0x00107e46u)
                    ++t10_material_sample;
#else
                if (words[ins.offset + 3u] ==
                        0x001000e2u &&
                    words[ins.offset + 4u] ==
                        2u &&
                    words[ins.offset + 7u] ==
                        0x00107936u)
                    ++t10_material_sample;
#endif
            }

            if (ins.length == 13u) {
                const auto resource =
                    words[ins.offset + 8u];
                const auto sampler =
                    words[ins.offset + 10u];

                if (resource == 12u &&
                    sampler == 12u) {
                    ++t12_sample;
                    if (words[ins.offset + 3u] ==
                            0x001000f2u &&
                        words[ins.offset + 4u] ==
                            12u &&
                        words[ins.offset + 7u] ==
                            0x00107936u)
                        ++t12_scratch_rgba_sample;
                }

                if (resource == 14u &&
                    sampler == 14u)
                    ++t14_sample;

                if (resource == 9u ||
                    sampler == 9u)
                    ++t9_sample;
            }
        }

        // Build131 must preserve retail r1.x. The first PTDE cube sample is
        // decoded in r12 and only its RGB/A result is written to r1.yzw.
        if (ins.opcode == 0x0eu &&
            ins.length == 7u &&
            ins.offset + 6u < words.size() &&
            words[ins.offset + 1u] ==
                0x001000e2u &&
            words[ins.offset + 2u] ==
                1u &&
            words[ins.offset + 3u] ==
                0x00100e56u &&
            words[ins.offset + 4u] ==
                12u &&
            words[ins.offset + 5u] ==
                0x00100006u &&
            words[ins.offset + 6u] ==
                12u)
            ++t12_decode_preserve_r1x;

        if (ins.opcode == 0x38u &&
            ins.length == 8u &&
            ins.offset + 7u < words.size() &&
            words[ins.offset + 1u] ==
                0x00100072u &&
            words[ins.offset + 2u] ==
                2u &&
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R4)
            words[ins.offset + 3u] ==
                0x00100246u &&
#else
            words[ins.offset + 3u] ==
                0x00100796u &&
#endif
            words[ins.offset + 4u] ==
                2u &&
            words[ins.offset + 5u] ==
                0x00208246u &&
            words[ins.offset + 6u] ==
                12u &&
            words[ins.offset + 7u] ==
                0u)
            ++c101_material_mul;
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
        t10_material_sample == 1u &&
        t12_sample == 1u &&
        t12_scratch_rgba_sample == 1u &&
        t12_decode_preserve_r1x == 1u &&
        c101_material_mul == 1u &&
        t14_decl == 1u &&
        s14_decl == 1u &&
        t14_sample == 1u &&
        t9_sample == 0u &&
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R10E_ATMOS_DOMAIN)
        r10e_atmosphere_domain_exact(words) &&
#endif
        (!require_terminal_sat ||
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
         phn_scene_encoding_exact(words)
#else
         terminal_rgb_sat_exact(words)
#endif
        );
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

    std::vector<std::uint8_t> base;
    const auto mr =
        material_response::
            materialize_ptde_diffuse_response_v1(
                features,
                stock_source,
                stock_size,
                base,
                true);

    using mr_result =
        material_response::
            diffuse_v1_result;

    if (mr.result ==
            mr_result::pass_not_candidate ||
        mr.result ==
            mr_result::pass_unknown_exact_sha) {
        outcome.result =
            pmetal_rgba_materialize_result::
                pass_not_candidate;
        return outcome;
    }

    if (mr.result !=
            mr_result::applied ||
        mr.family !=
            material_response::
                diffuse_v1_family::
                    stable_hemenv) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_diffuse_base;
        return outcome;
    }

    const auto stock_digest =
        hashing::sha256(
            stock_source,
            stock_size);

    const pmetal_rgba_authority::entry
        *authority = nullptr;

    for (const auto &candidate :
         pmetal_rgba_authority::
             k_entries) {
        if (!hashing::matches_hex(
                stock_digest,
                candidate.stock_sha256))
            continue;

        if (authority != nullptr) {
            outcome.result =
                pmetal_rgba_materialize_result::
                    pass_unknown_exact_sha;
            return outcome;
        }

        authority = &candidate;
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

    // Compose only independent A1 owners that do not overlap the EnvSpec
    // semantic cut. The clean diffuse base intentionally deferred them.
    if (!compose_a1(
            features,
            stock_source,
            stock_size,
            base,
            *authority,
            outcome.composed_owners)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_a1_composition;
        return outcome;
    }

#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
#if defined(DSRRL_PMETAL_FORCE_PTDE_PACKEDGI)
    // PR208 corrected stable path: PTDE PackedGI is raw RGBA and therefore
    // must use the already-recovered Build131 post-filter RGB/A decode.
    // Reuse that exact EnvSpec+material operator, but stop here before the
    // later full-island EnvDiffuse translation, U/L composition and terminal
    // SAT so this remains an operator-local carrier/consumer diagnostic.
    if (compose_upper_lower ||
        !apply_build131(
            base,
            *authority) ||
        !final_postcondition(
            base,
            false,
            false)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_build131_precondition;
        return outcome;
    }

    outcome.spec_rgb_consumer = true;
    outcome.envdiffuse_linear_consumer_diag = false;
    output = std::move(base);
    outcome.result =
        pmetal_rgba_materialize_result::applied;
    return outcome;
#else
    // PR203/PR204 native-DSR successor: native BC6H does not use PTDE
    // RGBA decode. Retain the historical V13 cut and restore only material
    // modulation on the EnvSpec accumulator.
    if (compose_upper_lower ||
        !apply_v13_native_dsr_no_tail(
            base,
            *authority) ||
        !apply_v13_native_dsr_material_mod_only(
            base,
            *authority) ||
        !v13_native_dsr_material_mod_postcondition(
            base,
            *authority)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_build131_precondition;
        return outcome;
    }

    outcome.spec_rgb_consumer = true;
    outcome.envdiffuse_linear_consumer_diag = false;
    output = std::move(base);
    outcome.result =
        pmetal_rgba_materialize_result::applied;
    return outcome;
#endif
#elif defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG)
    // Exact historical V13-style diagnostic: native DSR cubemap carrier,
    // PTDE A/B+beta source law, no modern SpecRGB*c101*COLOR0 final tail.
    // The first common EnvSpec+EnvDiffuse merge remains byte-identical.
    if (compose_upper_lower ||
        !apply_v13_native_dsr_no_tail(
            base,
            *authority) ||
        !v13_native_dsr_no_tail_postcondition(
            base,
            *authority)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_build131_precondition;
        return outcome;
    }

    outcome.spec_rgb_consumer = false;
    outcome.envdiffuse_linear_consumer_diag = false;
    output = std::move(base);
    outcome.result =
        pmetal_rgba_materialize_result::applied;
    return outcome;
#else
    // Replace the complete DSR EnvSpec/PBL window with the PTDE legacy
    // RGB/A + direct-R + raw-c101 material operator.
    if (!apply_build131(
            base,
            *authority)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_build131_precondition;
        return outcome;
    }

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    // Complete stable-HemEnv PTDE EnvDiffuse island: exact raw PTDE t11
    // resource, filter RGBA, post-filter RGB/A decode and exact PTDE c86
    // endpoint carried in b12[3].xyz.
    if (!apply_exact_ptde_envdiffuse_consumer(
            base)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_postcondition;
        return outcome;
    }
#else
    // Legacy diagnostic-only translation: native DSR t11 field with a
    // recovered linear endpoint.
    if (!apply_linear_envdiffuse_consumer_diag(
            base)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_postcondition;
        return outcome;
    }
#endif
    outcome.envdiffuse_linear_consumer_diag = true;

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R3)
    if (!remove_dsr_postmerge_visibility_scalar_r3(
            base,
            *authority) ||
        !r3_postmerge_visibility_postcondition(
            base,
            *authority)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_postcondition;
        return outcome;
    }
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R5)
    // R5 restores the PTDE/legacy HemEnv spatial normal basis before either
    // environment consumer. It intentionally leaves V, reflection algebra,
    // PTDE cubemap resources, EnvDiffuse/EnvSpec endpoints and material tail
    // unchanged, so this is one operator-local semantic cut.
    if (!apply_exact_ptde_normal_basis_r5(
            base)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_postcondition;
        return outcome;
    }
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R10E_ATMOS_DOMAIN)
    // Exact stock PS preimage is independently checked. If another owner has
    // modified Fog/LS domain instructions, this shader fails open to stock DSR.
    if (!apply_exact_ptde_atmosphere_domain_r10e(base)) {
        outcome.result = pmetal_rgba_materialize_result::fail_postcondition;
        return outcome;
    }
    outcome.atmosphere_domain_composed = true;
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW)
    if (!apply_exact_ptde_shadow_visibility_r7(
            base,
            *authority)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_postcondition;
        return outcome;
    }

    outcome.shadow_visibility_kernel_composed =
        outcome.receiver_id == 33u ||
        outcome.receiver_id == 34u;
#endif

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R11_COMMON_MERGE)
    if (!remove_dsr_sao_surface_multiplier_r11(
            base) ||
        !r11_common_merge_postcondition(
            base)) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_postcondition;
        return outcome;
    }
#endif

// R10E is an exact-scoped PTDE atmosphere island when enabled, otherwise
    // retain stock DSR Fog/LS domain behavior without hidden compensation.

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

    // Build131 now owns the dedicated P_Metal SpecRGB consumer itself:
    // fresh t10 is sampled only at the final material cut and feeds the
    // attested material MUL there. Do not run the generic early SpecRGB
    // materializer here; that would reintroduce PTDE SpecRGB ahead of the
    // surviving DSR PBL body and recreate the hybrid that produced the
    // owner-visible white P_Metal failure.
    outcome.spec_rgb_consumer = true;

    // PTDE PHN terminal is scene encoding k135=c135.x/c135.y followed by
    // RGB SAT. R6 installs the exact consumer at b12[0].w. Until its dynamic
    // DrawEnv producer is independently routed, the runtime carrier is unity,
    // preserving R5 output rather than guessing a global 0.5.
    if (!features.enabled(
            core::operator_id::terminal_sat_rgb) ||
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
        !compose_exact_phn_scene_encoding(base)
#else
        !compose_exact_terminal_rgb_sat(base)
#endif
        ) {
        outcome.result =
            pmetal_rgba_materialize_result::
                fail_postcondition;
        return outcome;
    }

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
    outcome.phn_scene_encoding_composed = true;
#endif
    outcome.composed_owners |=
        core::operator_bit(
            core::operator_id::terminal_sat_rgb);

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
#endif
}

} // namespace dsrrl::operators::env_spec
