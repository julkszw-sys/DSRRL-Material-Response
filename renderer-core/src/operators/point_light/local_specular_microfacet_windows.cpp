#include "dsrrl/operators/point_light/local_specular_microfacet_windows.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace dsrrl::operators::point_light {
namespace {

constexpr std::uint32_t k_dxbc_magic = 0x43425844u; // DXBC
constexpr std::uint32_t k_shex_tag = 0x58454853u;   // SHEX
constexpr std::uint32_t k_shdr_tag = 0x52444853u;   // SHDR

constexpr std::uint16_t k_op_dp3 = 16u;
constexpr std::uint16_t k_op_endif = 21u;
constexpr std::uint16_t k_op_exp = 25u;
constexpr std::uint16_t k_op_if = 31u;
constexpr std::uint16_t k_op_mad = 50u;
constexpr std::uint16_t k_op_customdata = 53u;
constexpr std::uint16_t k_op_mul = 56u;

constexpr std::uint32_t k_schlick_a_bits = 0xc0b1c059u; // -5.55473f
constexpr std::uint32_t k_schlick_b_bits = 0xc0df760cu; // -6.98316f

struct instruction_meta {
    std::uint32_t start = 0u;
    std::uint32_t end = 0u;
    std::uint16_t opcode = 0u;
    bool schlick_anchor = false;
};

constexpr std::size_t k_max_instructions = 4096u;
constexpr std::size_t k_max_if_depth = 128u;
constexpr std::uint16_t k_no_instruction =
    std::numeric_limits<std::uint16_t>::max();

std::uint32_t read_u32(
    const std::uint8_t *bytes,
    std::size_t offset) noexcept
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

std::uint8_t expected_windows(
    local_specular_receiver_class receiver_class) noexcept
{
    switch (receiver_class) {
    case local_specular_receiver_class::clustered_spc_pnts:
        return 1u;
    case local_specular_receiver_class::fixed_spc_pntss:
        return 2u;
    case local_specular_receiver_class::fixed_spc_pntssss:
        return 4u;
    default:
        return 0u;
    }
}

bool valid_window_instruction_count(
    local_specular_receiver_class receiver_class,
    std::uint32_t count) noexcept
{
    if (receiver_class ==
        local_specular_receiver_class::clustered_spc_pnts)
        return count == 34u || count == 37u;

    if (receiver_class ==
            local_specular_receiver_class::fixed_spc_pntss ||
        receiver_class ==
            local_specular_receiver_class::fixed_spc_pntssss)
        return count >= 60u && count <= 63u;

    return false;
}

bool instruction_has_word(
    const std::uint32_t *words,
    const instruction_meta &instruction,
    std::uint32_t value) noexcept
{
    for (std::uint32_t i = instruction.start;
         i < instruction.end;
         ++i)
        if (words[i] == value)
            return true;

    return false;
}

bool schlick_sequence_valid(
    const std::array<instruction_meta,k_max_instructions> &instructions,
    std::size_t instruction_count,
    std::size_t anchor_index) noexcept
{
    if (anchor_index < 3u ||
        anchor_index + 2u >= instruction_count)
        return false;

    return
        instructions[anchor_index - 3u].opcode == k_op_dp3 &&
        instructions[anchor_index - 2u].opcode == k_op_dp3 &&
        instructions[anchor_index - 1u].opcode == k_op_dp3 &&
        instructions[anchor_index].opcode == k_op_mad &&
        instructions[anchor_index + 1u].opcode == k_op_mul &&
        instructions[anchor_index + 2u].opcode == k_op_exp;
}

} // namespace

local_specular_microfacet_window_scan
scan_local_specular_microfacet_shex_words(
    const std::uint32_t *words,
    std::size_t word_count,
    local_specular_receiver_class receiver_class) noexcept
{
    local_specular_microfacet_window_scan out;

    const auto expected = expected_windows(receiver_class);
    if (words == nullptr ||
        word_count < 3u ||
        expected == 0u) {
        out.result =
            local_specular_window_result::fail_open_invalid_input;
        return out;
    }

    std::array<instruction_meta,k_max_instructions> instructions{};
    std::array<std::uint16_t,k_max_instructions> if_end{};
    if_end.fill(k_no_instruction);

    std::array<std::uint16_t,k_max_if_depth> if_stack{};
    std::size_t if_depth = 0u;
    std::size_t instruction_count = 0u;

    std::size_t word = 2u;
    while (word < word_count) {
        if (instruction_count >= instructions.size()) {
            out.result =
                local_specular_window_result::
                    fail_open_instruction_stream;
            return out;
        }

        const auto token = words[word];
        const auto opcode =
            static_cast<std::uint16_t>(token & 0x7ffu);

        std::uint32_t length = 0u;
        if (opcode == k_op_customdata) {
            if (word + 1u >= word_count) {
                out.result =
                    local_specular_window_result::
                        fail_open_instruction_stream;
                return out;
            }
            length = words[word + 1u];
        } else {
            length = (token >> 24u) & 0x7fu;
        }

        if (length == 0u ||
            length > word_count - word) {
            out.result =
                local_specular_window_result::
                    fail_open_instruction_stream;
            return out;
        }

        instruction_meta current;
        current.start =
            static_cast<std::uint32_t>(word);
        current.end =
            static_cast<std::uint32_t>(word + length);
        current.opcode = opcode;

        bool has_a = false;
        bool has_b = false;
        for (std::size_t w = word;
             w < word + length;
             ++w) {
            has_a = has_a ||
                words[w] == k_schlick_a_bits;
            has_b = has_b ||
                words[w] == k_schlick_b_bits;
        }
        current.schlick_anchor =
            opcode == k_op_mad && has_a && has_b;

        const auto current_index =
            static_cast<std::uint16_t>(instruction_count);
        instructions[instruction_count++] = current;

        if (opcode == k_op_if) {
            if (if_depth >= if_stack.size()) {
                out.result =
                    local_specular_window_result::
                        fail_open_instruction_stream;
                return out;
            }
            if_stack[if_depth++] = current_index;
        } else if (opcode == k_op_endif) {
            if (if_depth == 0u) {
                out.result =
                    local_specular_window_result::
                        fail_open_instruction_stream;
                return out;
            }
            const auto start_index =
                if_stack[--if_depth];
            if_end[start_index] = current_index;
        }

        word += length;
    }

    if (word != word_count ||
        if_depth != 0u) {
        out.result =
            local_specular_window_result::
                fail_open_instruction_stream;
        return out;
    }

    std::array<std::uint16_t,4> anchors{};
    std::size_t anchor_count = 0u;
    for (std::size_t i = 0u;
         i < instruction_count;
         ++i) {
        if (!instructions[i].schlick_anchor)
            continue;

        if (anchor_count >= anchors.size()) {
            out.result =
                local_specular_window_result::
                    fail_open_anchor_count;
            return out;
        }
        anchors[anchor_count++] =
            static_cast<std::uint16_t>(i);
    }

    if (anchor_count != expected) {
        out.result =
            local_specular_window_result::
                fail_open_anchor_count;
        return out;
    }

    for (std::size_t ordinal = 0u;
         ordinal < anchor_count;
         ++ordinal) {
        const auto anchor_index =
            static_cast<std::size_t>(anchors[ordinal]);

        if (!schlick_sequence_valid(
                instructions,
                instruction_count,
                anchor_index) ||
            !instruction_has_word(
                words,
                instructions[anchor_index],
                k_schlick_a_bits) ||
            !instruction_has_word(
                words,
                instructions[anchor_index],
                k_schlick_b_bits)) {
            out.result =
                local_specular_window_result::
                    fail_open_anchor_shape;
            return out;
        }

        std::uint16_t nearest_if = k_no_instruction;
        for (std::size_t i = 0u;
             i < anchor_index;
             ++i) {
            if (instructions[i].opcode != k_op_if ||
                if_end[i] == k_no_instruction ||
                if_end[i] <= anchor_index)
                continue;

            nearest_if = static_cast<std::uint16_t>(i);
        }

        if (nearest_if == k_no_instruction) {
            out.result =
                local_specular_window_result::
                    fail_open_window_shape;
            return out;
        }

        const auto endif_index =
            static_cast<std::size_t>(if_end[nearest_if]);
        const auto count =
            static_cast<std::uint32_t>(
                endif_index -
                static_cast<std::size_t>(nearest_if) +
                1u);

        if (instructions[nearest_if].opcode != k_op_if ||
            instructions[endif_index].opcode != k_op_endif ||
            !valid_window_instruction_count(
                receiver_class,
                count)) {
            out.result =
                local_specular_window_result::
                    fail_open_window_shape;
            return out;
        }

        std::size_t anchors_in_window = 0u;
        for (std::size_t i = nearest_if;
             i <= endif_index;
             ++i)
            if (instructions[i].schlick_anchor)
                ++anchors_in_window;

        if (anchors_in_window != 1u) {
            out.result =
                local_specular_window_result::
                    fail_open_window_shape;
            return out;
        }

        auto &window_out = out.windows[ordinal];
        window_out.start_word =
            instructions[nearest_if].start;
        window_out.schlick_word =
            instructions[anchor_index].start;
        window_out.end_word_exclusive =
            instructions[endif_index].end;
        window_out.instruction_count =
            static_cast<std::uint16_t>(count);
        window_out.light_ordinal =
            static_cast<std::uint8_t>(ordinal);
    }

    out.window_count =
        static_cast<std::uint8_t>(anchor_count);
    out.result = local_specular_window_result::exact;
    return out;
}

local_specular_microfacet_window_scan
scan_local_specular_microfacet_windows(
    const void *pixel_shader_code,
    std::size_t code_size,
    local_specular_receiver_class receiver_class) noexcept
{
    local_specular_microfacet_window_scan out;

    if (pixel_shader_code == nullptr ||
        code_size < 36u) {
        out.result =
            local_specular_window_result::
                fail_open_invalid_input;
        return out;
    }

    const auto *bytes =
        static_cast<const std::uint8_t *>(
            pixel_shader_code);

    if (read_u32(bytes,0u) != k_dxbc_magic) {
        out.result =
            local_specular_window_result::
                fail_open_dxbc_container;
        return out;
    }

    const auto chunk_count = read_u32(bytes,28u);
    const std::size_t table_bytes =
        32u +
        static_cast<std::size_t>(chunk_count) *
            sizeof(std::uint32_t);

    if (chunk_count == 0u ||
        table_bytes > code_size) {
        out.result =
            local_specular_window_result::
                fail_open_dxbc_container;
        return out;
    }

    for (std::uint32_t i = 0u;
         i < chunk_count;
         ++i) {
        const auto chunk_offset =
            static_cast<std::size_t>(
                read_u32(
                    bytes,
                    32u +
                        static_cast<std::size_t>(i) *
                            sizeof(std::uint32_t)));

        if (chunk_offset > code_size ||
            code_size - chunk_offset < 8u)
            continue;

        const auto tag =
            read_u32(bytes,chunk_offset);
        if (tag != k_shex_tag &&
            tag != k_shdr_tag)
            continue;

        const auto chunk_size =
            static_cast<std::size_t>(
                read_u32(bytes,chunk_offset + 4u));
        if ((chunk_size & 3u) != 0u ||
            chunk_size > code_size - chunk_offset - 8u) {
            out.result =
                local_specular_window_result::
                    fail_open_dxbc_container;
            return out;
        }

        const auto *chunk_bytes =
            bytes + chunk_offset + 8u;

        // Avoid alignment assumptions: copy only the confirmed DSR target
        // payload into a bounded local DWORD array.
        constexpr std::size_t k_max_shex_words = 8192u;
        const auto word_count =
            chunk_size / sizeof(std::uint32_t);
        if (word_count > k_max_shex_words) {
            out.result =
                local_specular_window_result::
                    fail_open_instruction_stream;
            return out;
        }

        std::array<std::uint32_t,k_max_shex_words>
            words{};
        std::memcpy(
            words.data(),
            chunk_bytes,
            chunk_size);

        return scan_local_specular_microfacet_shex_words(
            words.data(),
            word_count,
            receiver_class);
    }

    out.result =
        local_specular_window_result::
            fail_open_dxbc_container;
    return out;
}

} // namespace dsrrl::operators::point_light
