#include "dsrrl/operators/env_spec/pmetal_rgba_lerp_materializer.hpp"

#include "dsrrl/operators/env_spec/generated_pmetal_envspec_hemenvlerp_v1.hpp"
#include "dsrrl/operators/legacy_plan/a1_create_time_materializer.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_rdef_patch.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include "dsrrl/operators/material_response/material_response_diffuse_v1.hpp"
#include "dsrrl/operators/resource_bridges/spec_rgb_consumer_materializer.hpp"
#include "dsrrl/operators/surface/terminal_sat_rgb_patch.hpp"
#include "dsrrl/operators/surface/phn_scene_encoding.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <optional>
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

bool parse_dxbc(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<chunk> &chunks,
    std::size_t &code_index) noexcept
{
    std::vector<std::uint32_t> ignored;
    return parse_dxbc(
        source,
        size,
        chunks,
        code_index,
        ignored);
}

bool extract_words(
    const std::vector<chunk> &chunks,
    std::size_t code_index,
    std::vector<std::uint32_t> &words) noexcept
{
    if (code_index >= chunks.size() ||
        (chunks[code_index].payload.size() & 3u) != 0u)
        return false;

    const auto &payload = chunks[code_index].payload;
    try {
        words.resize(payload.size() / 4u);
    } catch (...) {
        return false;
    }

    for (std::size_t i = 0u; i < words.size(); ++i)
        words[i] = read_u32(payload.data() + i * 4u);

    return
        words.size() >= 12u &&
        words[1] == words.size();
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

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
constexpr surface::phn_scene_encoding_carrier
    k_pmetal_phn_scene_carrier{
        12u,
        0u,
        3u
    };

bool apply_exact_phn_scene_encoding(
    std::vector<std::uint32_t> &words,
    const generated_lerp::pmetal_hemenvlerp_site &site) noexcept
{
    const auto word =
        static_cast<std::size_t>(
            site.terminal_rgb_word);

    if (word + 4u >= words.size() ||
        (words[word] != 0x05000036u &&
         words[word] != 0x05002036u) ||
        words[word + 1u] != 0x00102072u ||
        words[word + 2u] != 0u)
        return false;

    const auto encoded =
        surface::apply_unique_phn_scene_encoding_words(
            words,
            k_pmetal_phn_scene_carrier);

    return
        (encoded ==
             surface::phn_scene_encoding_result::applied ||
         encoded ==
             surface::phn_scene_encoding_result::already_encoded) &&
        surface::unique_phn_scene_encoding_exact(
            words,
            k_pmetal_phn_scene_carrier);
}

bool phn_scene_encoding_exact(
    const std::vector<std::uint32_t> &words) noexcept
{
    return surface::unique_phn_scene_encoding_exact(
        words,
        k_pmetal_phn_scene_carrier);
}
#else
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
#endif

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
    bool compose_upper_lower,
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
            64u))
        return false;

    if (compose_upper_lower &&
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

// R18: exact stable-R17 operator parity helpers for HemEnvLerp.

#if defined(DSRRL_PMETAL_HEMENVLERP_R18_R17_PARITY) && defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R5)
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

#if defined(DSRRL_PMETAL_HEMENVLERP_R18_R17_PARITY) && defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW)
// R7 restores the remaining upstream PTDE Visibility_P shadow producer on the
// exact HemEnvLerp Csd/Sdw P_Metal receivers. DSR rx33/rx34 replaced the legacy
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
    std::uint32_t receiver_id) noexcept
{
    if (receiver_id == 35u)
        return true;

    if (receiver_id != 33u &&
        receiver_id != 34u)
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
        (receiver_id == 33u &&
         base_register != 6u) ||
        (receiver_id == 34u &&
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

#if defined(DSRRL_PMETAL_HEMENVLERP_R18_R17_PARITY) && defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R11_COMMON_MERGE)
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

#if defined(DSRRL_PMETAL_HEMENVLERP_R18_R17_PARITY) && defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R15_PTDE_LIVEOUT_PAIR)
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

#if defined(DSRRL_PMETAL_HEMENVLERP_R18_R17_PARITY) && defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R16_LEGACY_DIFFUSE_SPLIT)
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

#if defined(DSRRL_PMETAL_HEMENVLERP_R18_R17_PARITY) && defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R17_PTDE_DIFFUSE_MATERIAL)
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

bool final_postcondition(
    const std::vector<std::uint8_t> &bytes,
    const generated_lerp::pmetal_hemenvlerp_site &site,
    const std::vector<std::uint32_t> &preserved_envdiffuse,
    bool upper_lower_composed) noexcept
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
            12u))
        return false;

    const bool has_cb13 =
        legacy_plan::dxbc::rdef::has_constant_buffer_binding(
            rdef->payload,
            13u);
    if (has_cb13 != upper_lower_composed)
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

    const bool upper_lower_shape_ok =
        upper_lower_composed
            ? (adjacent_pair_count(words, 0u, 7u) == 0u &&
               adjacent_pair_count(words, 0u, 8u) == 0u &&
               adjacent_pair_count(words, 13u, 6u) == 1u &&
               adjacent_pair_count(words, 13u, 7u) == 2u)
            : (adjacent_pair_count(words, 0u, 7u) == 1u &&
               adjacent_pair_count(words, 0u, 8u) == 2u &&
               adjacent_pair_count(words, 13u, 6u) == 0u &&
               adjacent_pair_count(words, 13u, 7u) == 0u);

    return
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
        phn_scene_encoding_exact(words) &&
#else
        terminal_rgb_sat_exact(words) &&
#endif
        upper_lower_shape_ok &&
        sample_count(words, 9u) == 0u &&
        sample_count(words, 10u) == 1u &&
        sample_count(words, 11u) == 1u &&
        sample_count(words, 12u) == 1u &&
        sample_count(words, 13u) == 1u &&
        sample_count(words, 14u) == 1u;
}


#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) || defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
bool v13_native_dsr_no_tail_lerp_postcondition(
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
        legacy_plan::dxbc::rdef::has_constant_buffer_binding(
            rdef->payload,
            13u))
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

    // No modern material tail is allowed in this diagnostic. The EnvSpec
    // semantic cut ends before the stock EnvDiffuse block and common merge.
    return
        contains_exact_subsequence(
            words,
            expected_envspec_cut) &&
        contains_exact_subsequence(
            words,
            preserved_envdiffuse) &&
        adjacent_pair_count(words, 0u, 7u) == 1u &&
        adjacent_pair_count(words, 0u, 8u) == 2u &&
        adjacent_pair_count(words, 13u, 6u) == 0u &&
        adjacent_pair_count(words, 13u, 7u) == 0u &&
        sample_count(words, 9u) == 0u &&
        sample_count(words, 10u) == 0u &&
        sample_count(words, 11u) == 1u &&
        sample_count(words, 12u) == 1u &&
        sample_count(words, 13u) == 1u &&
        sample_count(words, 14u) == 1u;

}

#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
bool append_t10_rdef_from_t1_lerp(
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

bool apply_v13_lerp_material_mod_only(
    const std::uint8_t *source,
    std::size_t source_size,
    const generated_lerp::pmetal_hemenvlerp_site &site,
    std::vector<std::uint8_t> &output) noexcept
{
    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index = 0u;

    if (!parse_dxbc(
            source,
            source_size,
            chunks,
            code_index,
            words))
        return false;

    std::vector<instruction_view> instructions;
    if (!decode(words,instructions))
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
        t1_decl->offset >= site.t12_word ||
        t1_sample->offset >= site.t12_word)
        return false;

    const auto material_at =
        static_cast<std::size_t>(site.t12_word) +
        k_ptde_rgba_envspec_chain.size();
    constexpr std::size_t k_material_words = 33u;

    if (material_at + k_material_words >
            static_cast<std::size_t>(site.t11_word) ||
        site.t11_word >= words.size())
        return false;

    for (std::size_t i = material_at;
         i < material_at + k_material_words;
         ++i)
        if (words[i] != 0x0100003au)
            return false;

    const auto color0 = site.color0_register;

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
        0x00101246u,color0
    }};
    const std::array<std::uint32_t,7> envspec_mul{{
        0x07000038u,
        0x001000e2u,1u,
        0x00100e56u,1u,
        0x00100246u,12u
    }};

    auto out = words.begin() + static_cast<std::ptrdiff_t>(material_at);
    out = std::copy(fresh_spec.begin(),fresh_spec.end(),out);
    out = std::copy(c101_mul.begin(),c101_mul.end(),out);
    out = std::copy(color_mul.begin(),color_mul.end(),out);
    std::copy(envspec_mul.begin(),envspec_mul.end(),out);

    words.insert(
        words.begin() + static_cast<std::ptrdiff_t>(t1_decl->offset + 4u),
        t10_decl.begin(),
        t10_decl.end());
    words[1] = static_cast<std::uint32_t>(words.size());

    if (!append_t10_rdef_from_t1_lerp(chunks))
        return false;

    return rebuild(
        source,
        source_size,
        std::move(chunks),
        code_index,
        words,
        output);
}

bool v13_lerp_material_mod_postcondition(
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

    std::vector<instruction_view> instructions;
    if (!decode(words,instructions))
        return false;

    std::size_t t10_decl = 0u;
    std::size_t t10_sample = 0u;
    std::size_t t10_word = static_cast<std::size_t>(-1);

    for (const auto &ins : instructions) {
        if (ins.opcode == 0x58u && ins.length == 4u &&
            words[ins.offset + 2u] == 10u)
            ++t10_decl;
        if (ins.opcode >= 0x45u && ins.opcode <= 0x4au &&
            ins.length == 11u &&
            words[ins.offset + 8u] == 10u) {
            ++t10_sample;
            t10_word = ins.offset;
        }
    }

    const auto expected_t10_word =
        static_cast<std::size_t>(site.t12_word) +
        k_ptde_rgba_envspec_chain.size() + 4u;

    const auto color0 = site.color0_register;

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
        0x00101246u,color0
    }};
    const std::array<std::uint32_t,7> envspec_mul{{
        0x07000038u,
        0x001000e2u,1u,
        0x00100e56u,1u,
        0x00100246u,12u
    }};

    // Declaration insertion is above the semantic cuts, so stock EnvDiffuse
    // and its common merge shift by four DWORDs but remain byte-identical.
    if (static_cast<std::size_t>(site.t11_word) + 4u +
            preserved_envdiffuse.size() > words.size())
        return false;

    return
        t10_decl == 1u &&
        t10_sample == 1u &&
        t10_word == expected_t10_word &&
        sample_count(words,9u) == 0u &&
        sample_count(words,10u) == 1u &&
        sample_count(words,11u) == 1u &&
        sample_count(words,12u) == 1u &&
        sample_count(words,13u) == 1u &&
        sample_count(words,14u) == 1u &&
        std::equal(
            c101_mul.begin(),c101_mul.end(),
            words.begin() + static_cast<std::ptrdiff_t>(c101_at)) &&
        std::equal(
            color_mul.begin(),color_mul.end(),
            words.begin() + static_cast<std::ptrdiff_t>(color_at)) &&
        std::equal(
            envspec_mul.begin(),envspec_mul.end(),
            words.begin() + static_cast<std::ptrdiff_t>(envspec_at)) &&
        std::equal(
            preserved_envdiffuse.begin(),preserved_envdiffuse.end(),
            words.begin() + static_cast<std::ptrdiff_t>(site.t11_word + 4u));
}
#endif
#endif

} // namespace

pmetal_rgba_lerp_materialize_outcome
materialize_pmetal_rgba_lerp_receiver(
    const core::feature_registry &features,
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
        material_response::diffuse_v1_result;

    if (mr.result ==
            mr_result::pass_not_candidate) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::
                pass_not_candidate;
        return outcome;
    }

    if (mr.result ==
            mr_result::pass_unknown_exact_sha) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::
                pass_unknown_exact_sha;
        return outcome;
    }

    if (mr.result != mr_result::applied ||
        mr.family !=
            material_response::
                diffuse_v1_family::hemenvlerp) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::
                fail_diffuse_base;
        return outcome;
    }

    const auto stock_digest =
        hashing::sha256(
            stock_source,
            stock_size);

    const generated_lerp::pmetal_hemenvlerp_site
        *site = nullptr;
    for (const auto &candidate :
         generated_lerp::k_pmetal_hemenvlerp_sites) {
        if (!hashing::matches_hex(
                stock_digest,
                candidate.stock_sha256))
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
            pmetal_rgba_lerp_materialize_result::
                pass_unknown_exact_sha;
        return outcome;
    }

    if (site->pair_index !=
            mr.family_index ||
        site->semantic_receiver_id !=
            mr.receiver_id) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::
                fail_operator_precondition;
        return outcome;
    }

    outcome.pair_index = site->pair_index;
    outcome.semantic_receiver_id =
        site->semantic_receiver_id;

    // P_Metal EnvSpec is an operator-local island. Upper/Lower is a
    // separate LightBank operator and must never be composed into this
    // HemEnvLerp payload. Preserve the exact stock DSR b0[7]/b0[8]
    // continuation regardless of the global U/L feature state.
    const bool compose_upper_lower = false;

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index = 0u;

    if (!parse_dxbc(
            base.data(),
            base.size(),
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

    // Upper/Lower is independent from EnvSpec. Compose PTDE b13 only when the
    // U/L feature is explicitly enabled. With U/L OFF, preserve the exact
    // stock DSR b0[7]/b0[8] operands byte-for-byte while still porting the
    // P_Metal EnvSpec/SpecRGB/terminal-SAT island.
    if (compose_upper_lower) {
        for (const auto &[slot_word, source_register] :
             upper_lower_patches) {
            words[slot_word] = 13u;
            words[slot_word + 1u] =
                source_register == 7u
                    ? 6u
                    : 7u;
        }
    }

#if !defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) && !defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
    // R6 keeps the recent PR212 PHN terminal on HemEnvLerp too. The actual
    // k135 value is supplied through b12[0].w; unresolved producer state is
    // unity fail-open in draw runtime, never a hardcoded 0.5.
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
    if (!apply_exact_phn_scene_encoding(
            words,
            *site)) {
#else
    if (!apply_exact_terminal_rgb_sat(
            words,
            *site)) {
#endif
        outcome.result =
            pmetal_rgba_lerp_materialize_result::
                fail_terminal_sat;
        return outcome;
    }
#endif

    // The clean diffuse-v1 operator base inserted b12 at words 11..14.
    // Add b13 only for the explicitly enabled U/L-composed variant. The
    // U/L-off EnvSpec variant retains the original stock U/L ABI.
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

    if (compose_upper_lower) {
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
    }

    std::vector<std::uint8_t> envspec_base;
    if (!add_bridge_rdef(
            base.data(),
            base.size(),
            std::move(chunks),
            code_index,
            words,
            compose_upper_lower,
            envspec_base)) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::fail_b12_rdef;
        return outcome;
    }

#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
    std::vector<std::uint8_t> material_mod_base;
    if (!v13_native_dsr_no_tail_lerp_postcondition(
            envspec_base,
            *site,
            preserved_envdiffuse) ||
        !apply_v13_lerp_material_mod_only(
            envspec_base.data(),
            envspec_base.size(),
            *site,
            material_mod_base) ||
        !v13_lerp_material_mod_postcondition(
            material_mod_base,
            *site,
            preserved_envdiffuse)) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::fail_postcondition;
        return outcome;
    }

    outcome.envdiffuse_preserved = true;
    outcome.upper_lower_composed = false;
    outcome.upper_lower_preserved_stock = true;
    outcome.terminal_sat_rgb_composed = false;
    outcome.spec_rgb_consumer = true;
    output = std::move(material_mod_base);
#elif defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG)
    if (!v13_native_dsr_no_tail_lerp_postcondition(
            envspec_base,
            *site,
            preserved_envdiffuse)) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::fail_postcondition;
        return outcome;
    }

    outcome.envdiffuse_preserved = true;
    outcome.upper_lower_composed = false;
    outcome.upper_lower_preserved_stock = true;
    outcome.terminal_sat_rgb_composed = false;
    outcome.spec_rgb_consumer = false;
    output = std::move(envspec_base);
#else
    std::vector<std::uint8_t> spec_rgb_base;
    if (resource_bridges::materialize_spec_rgb_consumer(
            envspec_base.data(),
            envspec_base.size(),
            spec_rgb_base,
            true) !=
        resource_bridges::spec_rgb_consumer_result::applied) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::
                fail_spec_rgb_consumer;
        return outcome;
    }

    if (!final_postcondition(
            spec_rgb_base,
            *site,
            preserved_envdiffuse,
            compose_upper_lower)) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::fail_postcondition;
        return outcome;
    }

    outcome.envdiffuse_preserved = true;
    outcome.upper_lower_composed =
        compose_upper_lower;
    outcome.upper_lower_preserved_stock =
        !compose_upper_lower;
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
    outcome.phn_scene_encoding_composed = true;
#endif
    outcome.terminal_sat_rgb_composed = true;
    outcome.spec_rgb_consumer = true;
    output = std::move(spec_rgb_base);
#endif
    outcome.result =
        pmetal_rgba_lerp_materialize_result::applied;
    return outcome;
}

} // namespace dsrrl::operators::env_spec
