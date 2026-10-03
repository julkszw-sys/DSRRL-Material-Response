#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <vector>

namespace dsrrl::operators::surface {

// Shared PTDE PHN terminal scene-encoding operator.
//
// PTDE legacy PHN terminates as:
//   C_out.rgb = sat((c135.x / c135.y) * C_preterminal.rgb)
//
// Carrier resolution is deliberately outside this transform. A material family
// may use this operator only after its own producer/receiver routing proves the
// semantic k135 source.
struct phn_scene_encoding_carrier {
    std::uint32_t cb_slot = 0u;
    std::uint32_t cb_index = 0u;
    std::uint8_t cb_component = 0u;
};

enum class phn_scene_encoding_result : std::uint8_t {
    applied = 0,
    already_encoded,
    fail_invalid_carrier,
    fail_invalid_token_stream,
    fail_unverified_terminal_shape,
    fail_ambiguous_terminal,
    fail_unexpected_terminal_opcode
};

namespace phn_scene_encoding_detail {

inline constexpr std::uint32_t opcode_mask = 0x7ffu;
inline constexpr std::uint32_t opcode_mov = 0x36u;
inline constexpr std::uint32_t opcode_mul = 0x38u;
inline constexpr std::uint32_t mov_unsat = 0x05000036u;
inline constexpr std::uint32_t mov_sat = 0x05002036u;
inline constexpr std::uint32_t mul_sat = 0x08002038u;
inline constexpr std::uint32_t o0_xyz = 0x00102072u;

inline constexpr std::uint32_t cbuffer_scalar_token(
    std::uint8_t component) noexcept
{
    return 0x0020800au +
        (static_cast<std::uint32_t>(component) << 4u);
}

inline bool valid_carrier(
    const phn_scene_encoding_carrier &carrier) noexcept
{
    return carrier.cb_component < 4u &&
        carrier.cb_slot < 14u &&
        carrier.cb_index < 4096u;
}

struct terminal_scan {
    std::size_t at = static_cast<std::size_t>(-1);
    std::size_t length = 0u;
    std::uint32_t opcode_token = 0u;
    std::size_t hits = 0u;
};

inline bool scan_terminal(
    const std::vector<std::uint32_t> &words,
    terminal_scan &out) noexcept
{
    out = {};

    if (words.size() < 2u ||
        words[1] != words.size())
        return false;

    std::size_t at = 2u;
    while (at < words.size()) {
        const auto token = words[at];
        const auto length =
            static_cast<std::size_t>(
                (token >> 24u) & 0x7fu);

        if (length == 0u ||
            length > words.size() - at)
            return false;

        const auto opcode =
            token & opcode_mask;

        if (((opcode == opcode_mov &&
              length == 5u) ||
             (opcode == opcode_mul &&
              length == 8u)) &&
            at + 2u < words.size() &&
            words[at + 1u] == o0_xyz &&
            words[at + 2u] == 0u) {
            ++out.hits;
            out.at = at;
            out.length = length;
            out.opcode_token = token;
        }

        at += length;
    }

    return at == words.size();
}

inline bool matching_encoded_terminal(
    const std::vector<std::uint32_t> &words,
    std::size_t at,
    const phn_scene_encoding_carrier &carrier) noexcept
{
    return at + 7u < words.size() &&
        words[at] == mul_sat &&
        words[at + 1u] == o0_xyz &&
        words[at + 2u] == 0u &&
        words[at + 5u] ==
            cbuffer_scalar_token(
                carrier.cb_component) &&
        words[at + 6u] == carrier.cb_slot &&
        words[at + 7u] == carrier.cb_index;
}

} // namespace phn_scene_encoding_detail

// Replace the unique separate RGB terminal
//
//   mov[_sat] o0.xyz, S
//
// with
//
//   mul_sat o0.xyz, S, cb<slot>[index].component
//
// Alpha remains untouched. Combined RGBA writes and ambiguous terminal shapes
// fail open. The SM4/5 token count in words[1] is updated transactionally.
inline phn_scene_encoding_result
apply_unique_phn_scene_encoding_words(
    std::vector<std::uint32_t> &words,
    const phn_scene_encoding_carrier &carrier) noexcept
{
    using namespace phn_scene_encoding_detail;

    if (!valid_carrier(carrier))
        return phn_scene_encoding_result::
            fail_invalid_carrier;

    terminal_scan scan{};
    if (!scan_terminal(words, scan))
        return phn_scene_encoding_result::
            fail_invalid_token_stream;

    if (scan.hits == 0u)
        return phn_scene_encoding_result::
            fail_unverified_terminal_shape;

    if (scan.hits != 1u)
        return phn_scene_encoding_result::
            fail_ambiguous_terminal;

    if (scan.length == 8u)
        return matching_encoded_terminal(
                   words,
                   scan.at,
                   carrier)
            ? phn_scene_encoding_result::
                already_encoded
            : phn_scene_encoding_result::
                fail_unexpected_terminal_opcode;

    if (scan.length != 5u ||
        (scan.opcode_token != mov_unsat &&
         scan.opcode_token != mov_sat) ||
        scan.at + 4u >= words.size())
        return phn_scene_encoding_result::
            fail_unexpected_terminal_opcode;

    const std::uint32_t source_token =
        words[scan.at + 3u];
    const std::uint32_t source_index =
        words[scan.at + 4u];

    const std::uint32_t replacement[8] = {
        mul_sat,
        o0_xyz,
        0u,
        source_token,
        source_index,
        cbuffer_scalar_token(
            carrier.cb_component),
        carrier.cb_slot,
        carrier.cb_index
    };

    try {
        words.erase(
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    scan.at),
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    scan.at + 5u));

        words.insert(
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    scan.at),
            std::begin(replacement),
            std::end(replacement));
    } catch (...) {
        return phn_scene_encoding_result::
            fail_invalid_token_stream;
    }

    if (words.size() >
        static_cast<std::size_t>(
            std::numeric_limits<
                std::uint32_t>::max()))
        return phn_scene_encoding_result::
            fail_invalid_token_stream;

    words[1] =
        static_cast<std::uint32_t>(
            words.size());

    return matching_encoded_terminal(
               words,
               scan.at,
               carrier)
        ? phn_scene_encoding_result::applied
        : phn_scene_encoding_result::
            fail_invalid_token_stream;
}

// Exact postcondition: both the k135 multiply and RGB SAT must be present in
// one unique terminal instruction.
inline bool unique_phn_scene_encoding_exact(
    const std::vector<std::uint32_t> &words,
    const phn_scene_encoding_carrier &carrier) noexcept
{
    using namespace phn_scene_encoding_detail;

    if (!valid_carrier(carrier))
        return false;

    terminal_scan scan{};
    return scan_terminal(words, scan) &&
        scan.hits == 1u &&
        scan.length == 8u &&
        matching_encoded_terminal(
            words,
            scan.at,
            carrier);
}

} // namespace dsrrl::operators::surface
