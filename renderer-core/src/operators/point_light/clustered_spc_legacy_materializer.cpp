#include "dsrrl/operators/point_light/clustered_spc_legacy_materializer.hpp"

#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include "dsrrl/operators/point_light/fixed_local_specular_legacy_kernel.hpp"

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

constexpr std::uint16_t k_op_add = 0u;
constexpr std::uint16_t k_op_dp3 = 16u;
constexpr std::uint16_t k_op_endif = 21u;
constexpr std::uint16_t k_op_exp = 25u;
constexpr std::uint16_t k_op_mad = 50u;
constexpr std::uint16_t k_op_customdata = 53u;
constexpr std::uint16_t k_op_mov = 54u;
constexpr std::uint16_t k_op_mul = 56u;
constexpr std::uint16_t k_op_dcl_resource = 88u;
constexpr std::uint16_t k_op_dcl_temps = 104u;

constexpr std::uint32_t k_schlick_a_bits = 0xc0b1c059u;
constexpr std::uint32_t k_schlick_b_bits = 0xc0df760cu;
constexpr std::uint32_t k_inv_pi_bits = 0x3ea2f983u;
constexpr std::uint32_t k_pi_bits = 0x40490fdbu;

constexpr std::uint32_t k_temp_dst_xyz = 0x00100072u;
constexpr std::uint32_t k_temp_src_xyz = 0x00100246u;
constexpr std::uint32_t k_input_src_xyz = 0x00101246u;
constexpr std::uint32_t k_cb_src_xyz = 0x00208246u;
constexpr std::uint32_t k_imm_vector = 0x00004002u;

struct chunk {
    std::array<char,4> tag{};
    std::vector<std::uint8_t> payload;
};

struct instruction {
    std::uint32_t start = 0u;
    std::uint32_t end = 0u;
    std::uint16_t opcode = 0u;
};

struct insertion {
    std::uint32_t word = 0u;
    std::vector<std::uint32_t> payload;
    std::uint32_t erase_words = 0u;
};

struct sample_site {
    std::uint32_t instruction_word = 0u;
    std::uint32_t destination_token = 0u;
    std::uint32_t destination_register = 0u;
};

struct material_capture {
    sample_site spec_a{};
    sample_site spec_b{};
    bool blended = false;
    std::uint32_t blend_weight_token = 0u;
    std::uint32_t blend_weight_register = 0u;
};

struct clustered_contract {
    fixed_local_specular_light_operands operands{};
    fixed_local_specular_legacy_kernel kernel{};
    std::uint32_t schlick_instruction = 0u;
    std::uint32_t combine_instruction = 0u;
    std::uint32_t final_accum_instruction = 0u;
    std::uint32_t stock_specular_temp = 0u;
    std::uint32_t tail_temp = 0u;
    std::array<std::uint32_t,2> source_pair{};
    std::array<std::uint32_t,2> attenuation_pair{};
    std::array<std::uint32_t,2> source_factor_pair{};
    std::uint32_t accum_register = 0u;
};

std::uint8_t scalar_component(std::uint32_t dst_token) noexcept
{
    const auto mask = (dst_token >> 4u) & 0xfu;
    if (mask == 1u) return 0u;
    if (mask == 2u) return 1u;
    if (mask == 4u) return 2u;
    if (mask == 8u) return 3u;
    return 0xffu;
}

std::uint32_t temp_src_scalar(std::uint8_t component) noexcept
{
    return 0x0010000au +
        (static_cast<std::uint32_t>(component) << 4u);
}

bool temp_pair(const std::uint32_t *pair) noexcept
{
    const auto token = pair[0];
    return
        ((token >> 12u) & 0xffu) == 0u &&
        ((token >> 20u) & 0x3u) == 1u &&
        ((token >> 22u) & 0x7u) == 0u &&
        pair[1] <= 4095u;
}

bool temp_destination(
    std::uint32_t token,
    std::uint32_t reg) noexcept
{
    return
        ((token >> 12u) & 0xffu) == 0u &&
        ((token >> 20u) & 0x3u) == 1u &&
        ((token >> 22u) & 0x7u) == 0u &&
        reg <= 4095u;
}

bool parse(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<chunk> &chunks,
    std::size_t &code_index,
    std::vector<std::uint32_t> &words) noexcept
{
    if (source == nullptr ||
        size < 32u ||
        !legacy_plan::dxbc::checksum_container_valid(
            source,
            size))
        return false;

    const auto count = read_u32(source + 28u);
    if (count == 0u ||
        count > 64u ||
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
        const auto offset =
            static_cast<std::size_t>(
                read_u32(source + 32u + i * 4u));
        if (offset > size ||
            size - offset < 8u)
            return false;

        const auto payload_size =
            static_cast<std::size_t>(
                read_u32(source + offset + 4u));
        if (payload_size > size - offset - 8u)
            return false;

        chunk current{};
        std::memcpy(
            current.tag.data(),
            source + offset,
            4u);
        try {
            current.payload.assign(
                source + offset + 8u,
                source + offset + 8u + payload_size);
        } catch (...) {
            return false;
        }

        if (std::memcmp(
                current.tag.data(),
                "SHEX",
                4u) == 0 ||
            std::memcmp(
                current.tag.data(),
                "SHDR",
                4u) == 0) {
            if (code_index !=
                    static_cast<std::size_t>(-1) ||
                (payload_size & 3u) != 0u)
                return false;
            code_index = chunks.size();
        }

        chunks.push_back(
            std::move(current));
    }

    if (code_index ==
        static_cast<std::size_t>(-1))
        return false;

    const auto &payload =
        chunks[code_index].payload;
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
                payload.data() + i * 4u);

    return
        words.size() >= 3u &&
        words[1] ==
            static_cast<std::uint32_t>(
                words.size());
}

bool decode(
    const std::vector<std::uint32_t> &words,
    std::vector<instruction> &out) noexcept
{
    out.clear();
    std::size_t at = 2u;

    try {
        out.reserve(
            words.size() / 4u);
    } catch (...) {
        return false;
    }

    while (at < words.size()) {
        const auto token = words[at];
        const auto opcode =
            static_cast<std::uint16_t>(
                token & 0x7ffu);
        std::uint32_t length = 0u;

        if (opcode == k_op_customdata) {
            if (at + 1u >= words.size())
                return false;
            length = words[at + 1u];
        } else {
            length =
                (token >> 24u) & 0x7fu;
        }

        if (length == 0u ||
            length > words.size() - at)
            return false;

        out.push_back({
            static_cast<std::uint32_t>(at),
            static_cast<std::uint32_t>(
                at + length),
            opcode
        });
        at += length;
    }

    return at == words.size();
}

bool parse_color0(
    const std::vector<chunk> &chunks,
    std::uint32_t &reg) noexcept
{
    bool hit = false;

    for (const auto &current : chunks) {
        if (std::memcmp(
                current.tag.data(),
                "ISGN",
                4u) != 0)
            continue;

        if (current.payload.size() < 8u)
            return false;

        const auto count =
            read_u32(
                current.payload.data());
        if (count == 0u ||
            count > 64u ||
            8ull + 24ull * count >
                current.payload.size())
            return false;

        for (std::uint32_t i = 0u;
             i < count;
             ++i) {
            const auto at =
                8u + 24u * i;
            const auto name_offset =
                read_u32(
                    current.payload.data() +
                    at);
            const auto semantic_index =
                read_u32(
                    current.payload.data() +
                    at + 4u);
            if (name_offset >=
                current.payload.size())
                return false;

            const char *name =
                reinterpret_cast<const char *>(
                    current.payload.data() +
                    name_offset);
            const auto remaining =
                current.payload.size() -
                name_offset;

            bool terminated = false;
            for (std::size_t n = 0u;
                 n < remaining;
                 ++n) {
                if (name[n] == '\0') {
                    terminated = true;
                    break;
                }
            }
            if (!terminated)
                return false;

            if (std::strcmp(
                    name,
                    "COLOR") == 0 &&
                semantic_index == 0u) {
                if (hit)
                    return false;
                reg =
                    read_u32(
                        current.payload.data() +
                        at + 16u);
                hit = true;
            }
        }
    }

    return hit;
}

const instruction *find_instruction(
    const std::vector<instruction> &instructions,
    std::uint32_t word) noexcept
{
    for (const auto &current :
         instructions)
        if (current.start == word)
            return &current;

    return nullptr;
}

std::size_t instruction_index(
    const std::vector<instruction> &instructions,
    std::uint32_t word) noexcept
{
    for (std::size_t i = 0u;
         i < instructions.size();
         ++i)
        if (instructions[i].start ==
            word)
            return i;

    return instructions.size();
}

bool instruction_has_word(
    const std::vector<std::uint32_t> &words,
    const instruction &current,
    std::uint32_t value) noexcept
{
    for (std::uint32_t i =
             current.start;
         i < current.end;
         ++i)
        if (words[i] == value)
            return true;

    return false;
}

bool locate_declarations(
    const std::vector<std::uint32_t> &words,
    const std::vector<instruction> &instructions,
    std::uint32_t &t1_decl,
    std::uint32_t &t4_decl,
    std::uint32_t &temps_word,
    std::uint32_t &temp_count,
    std::uint32_t &t10_count,
    std::uint32_t &t16_count) noexcept
{
    std::uint32_t t1_count = 0u;
    std::uint32_t t4_count = 0u;
    std::uint32_t temps_count = 0u;
    t10_count = 0u;
    t16_count = 0u;

    for (const auto &current :
         instructions) {
        if (current.opcode ==
                k_op_dcl_resource &&
            current.end -
                current.start == 4u) {
            const auto slot =
                words[current.start + 2u];
            if (slot == 1u) {
                t1_decl = current.start;
                ++t1_count;
            } else if (slot == 4u) {
                t4_decl = current.start;
                ++t4_count;
            } else if (slot == 10u) {
                ++t10_count;
            } else if (slot == 16u) {
                ++t16_count;
            }
        }

        if (current.opcode ==
                k_op_dcl_temps &&
            current.end -
                current.start == 2u) {
            temps_word = current.start;
            temp_count =
                words[current.start + 1u];
            ++temps_count;
        }
    }

    return
        t1_count == 1u &&
        temps_count == 1u &&
        temp_count > 0u &&
        temp_count < 4092u;
}

bool difference_add(
    const std::vector<std::uint32_t> &words,
    const instruction &current,
    std::uint32_t a,
    std::uint32_t b) noexcept
{
    if (current.opcode != k_op_add ||
        current.end -
            current.start != 8u)
        return false;

    const auto at = current.start;
    return
        words[at + 1u] == 0x00100072u &&
        words[at + 2u] == b &&
        words[at + 3u] == 0x80100246u &&
        words[at + 4u] == 0x00000041u &&
        words[at + 5u] == a &&
        words[at + 6u] == 0x00100246u &&
        words[at + 7u] == b;
}

bool blend_mad(
    const std::vector<std::uint32_t> &words,
    const instruction &current,
    std::uint32_t a,
    std::uint32_t b,
    std::uint32_t &weight_token,
    std::uint32_t &weight_register) noexcept
{
    if (current.opcode != k_op_mad ||
        current.end -
            current.start != 9u)
        return false;

    const auto at = current.start;
    if (words[at + 1u] !=
            0x00100072u ||
        words[at + 2u] != a ||
        words[at + 5u] !=
            0x00100246u ||
        words[at + 6u] != b ||
        words[at + 7u] !=
            0x00100246u ||
        words[at + 8u] != a)
        return false;

    const auto token =
        words[at + 3u];
    const auto reg =
        words[at + 4u];

    if (((token >> 12u) & 0xffu) !=
            1u ||
        ((token >> 20u) & 0x3u) !=
            1u ||
        reg > 4095u)
        return false;

    weight_token = token;
    weight_register = reg;
    return true;
}

bool locate_material_capture(
    const std::vector<std::uint32_t> &words,
    const std::vector<instruction> &instructions,
    std::size_t anchor_index,
    bool blended,
    material_capture &out) noexcept
{
    out = {};

    std::uint32_t t1_count = 0u;
    std::uint32_t t4_count = 0u;

    for (std::size_t i = 0u;
         i < anchor_index;
         ++i) {
        const auto &current =
            instructions[i];
        const auto length =
            current.end -
            current.start;

        if (current.opcode < 0x45u ||
            current.opcode > 0x4au ||
            length != 11u ||
            current.start + 8u >=
                words.size())
            continue;

        const auto resource =
            words[current.start + 8u];

        if (resource != 1u &&
            resource != 4u)
            continue;

        if (!temp_destination(
                words[current.start + 3u],
                words[current.start + 4u]))
            return false;

        sample_site site{
            current.start,
            words[current.start + 3u],
            words[current.start + 4u]
        };

        if (resource == 1u) {
            out.spec_a = site;
            ++t1_count;
        } else {
            out.spec_b = site;
            ++t4_count;
        }
    }

    if (t1_count != 1u ||
        (!blended && t4_count != 0u) ||
        (blended && t4_count != 1u))
        return false;

    out.blended = blended;

    if (!blended)
        return true;

    const auto a_index =
        instruction_index(
            instructions,
            out.spec_a.instruction_word);
    const auto b_index =
        instruction_index(
            instructions,
            out.spec_b.instruction_word);

    if (a_index >=
            instructions.size() ||
        b_index >=
            instructions.size() ||
        b_index != a_index + 1u ||
        b_index + 2u >=
            instructions.size())
        return false;

    if (!difference_add(
            words,
            instructions[b_index + 1u],
            out.spec_a.destination_register,
            out.spec_b.destination_register) ||
        !blend_mad(
            words,
            instructions[b_index + 2u],
            out.spec_a.destination_register,
            out.spec_b.destination_register,
            out.blend_weight_token,
            out.blend_weight_register))
        return false;

    return true;
}

bool pair_equal(
    const std::uint32_t *a,
    const std::uint32_t *b) noexcept
{
    return
        a[0] == b[0] &&
        a[1] == b[1];
}

bool extract_clustered_contract(
    const std::vector<std::uint32_t> &words,
    const std::vector<instruction> &instructions,
    clustered_contract &out) noexcept
{
    out = {};

    std::size_t anchor_count = 0u;
    std::size_t anchor_index = 0u;

    for (std::size_t i = 0u;
         i < instructions.size();
         ++i) {
        const auto &current =
            instructions[i];

        if (current.opcode ==
                k_op_mad &&
            instruction_has_word(
                words,
                current,
                k_schlick_a_bits) &&
            instruction_has_word(
                words,
                current,
                k_schlick_b_bits)) {
            ++anchor_count;
            anchor_index = i;
        }
    }

    if (anchor_count != 1u ||
        anchor_index < 3u)
        return false;

    const auto &dp_vh =
        instructions[anchor_index - 3u];
    const auto &dp_nh =
        instructions[anchor_index - 2u];
    const auto &dp_nl =
        instructions[anchor_index - 1u];

    if (dp_vh.opcode != k_op_dp3 ||
        dp_nh.opcode != k_op_dp3 ||
        dp_nl.opcode != k_op_dp3 ||
        dp_vh.end - dp_vh.start != 7u ||
        dp_nh.end - dp_nh.start != 7u ||
        dp_nl.end - dp_nl.start != 7u)
        return false;

    const auto *vh_src0 =
        words.data() +
        dp_vh.start + 3u;
    const auto *vh_src1 =
        words.data() +
        dp_vh.start + 5u;
    const auto *nh_src0 =
        words.data() +
        dp_nh.start + 3u;
    const auto *nh_src1 =
        words.data() +
        dp_nh.start + 5u;
    const auto *nl_src0 =
        words.data() +
        dp_nl.start + 3u;
    const auto *nl_src1 =
        words.data() +
        dp_nl.start + 5u;

    if (!temp_pair(vh_src0) ||
        !temp_pair(vh_src1) ||
        !temp_pair(nh_src0) ||
        !temp_pair(nh_src1) ||
        !temp_pair(nl_src0) ||
        !temp_pair(nl_src1) ||
        !pair_equal(vh_src1,nh_src1) ||
        !pair_equal(nh_src0,nl_src0))
        return false;

    auto &operands = out.operands;
    operands.view.token = {
        vh_src0[0],vh_src0[1]
    };
    operands.normal.token = {
        nh_src0[0],nh_src0[1]
    };
    operands.light.token = {
        nl_src1[0],nl_src1[1]
    };
    operands.primary_scalar_dst.token = {
        words[dp_vh.start + 1u],
        words[dp_vh.start + 2u]
    };
    operands.auxiliary_scalar_dst.token = {
        words[dp_nh.start + 1u],
        words[dp_nh.start + 2u]
    };
    operands.ndotl_scalar_dst.token = {
        words[dp_nl.start + 1u],
        words[dp_nl.start + 2u]
    };

    if (scalar_component(
            operands.primary_scalar_dst.token[0]) >
            3u ||
        scalar_component(
            operands.auxiliary_scalar_dst.token[0]) >
            3u ||
        scalar_component(
            operands.ndotl_scalar_dst.token[0]) >
            3u)
        return false;

    if (emit_fixed_local_specular_legacy_kernel(
            operands,
            out.kernel) !=
        fixed_local_specular_legacy_kernel_result::exact)
        return false;

    out.schlick_instruction =
        instructions[anchor_index].start;

    std::size_t endif_index =
        instructions.size();
    for (std::size_t i =
             anchor_index + 1u;
         i < instructions.size();
         ++i) {
        if (instructions[i].opcode ==
            k_op_endif) {
            endif_index = i;
            break;
        }
    }

    if (endif_index ==
        instructions.size())
        return false;

    std::size_t combine_count = 0u;
    std::size_t combine_index = 0u;

    for (std::size_t i =
             anchor_index + 1u;
         i < endif_index;
         ++i) {
        const auto &current =
            instructions[i];

        if (current.opcode != k_op_mad ||
            current.end -
                current.start != 12u)
            continue;

        const auto at = current.start;
        if (words[at + 5u] ==
                k_imm_vector &&
            words[at + 6u] == 0u &&
            words[at + 7u] ==
                k_inv_pi_bits &&
            words[at + 8u] ==
                k_inv_pi_bits &&
            words[at + 9u] ==
                k_inv_pi_bits) {
            ++combine_count;
            combine_index = i;
        }
    }

    if (combine_count != 1u ||
        combine_index + 5u >=
            endif_index)
        return false;

    const auto &combine =
        instructions[combine_index];
    const auto &source_mul =
        instructions[combine_index + 1u];
    const auto &atten_mul =
        instructions[combine_index + 2u];
    const auto &ndotl_mul =
        instructions[combine_index + 3u];
    const auto &factor_mul =
        instructions[combine_index + 4u];
    const auto &accum_mad =
        instructions[combine_index + 5u];

    if (source_mul.opcode != k_op_mul ||
        atten_mul.opcode != k_op_mul ||
        ndotl_mul.opcode != k_op_mul ||
        factor_mul.opcode != k_op_mul ||
        accum_mad.opcode != k_op_mad ||
        source_mul.end-source_mul.start != 7u ||
        atten_mul.end-atten_mul.start != 7u ||
        ndotl_mul.end-ndotl_mul.start != 7u ||
        factor_mul.end-factor_mul.start != 7u ||
        accum_mad.end-accum_mad.start != 12u)
        return false;

    const auto combine_at =
        combine.start;
    if (!temp_destination(
            words[combine_at + 1u],
            words[combine_at + 2u]) ||
        !temp_pair(
            words.data() +
            combine_at + 10u))
        return false;

    out.tail_temp =
        words[combine_at + 2u];
    out.stock_specular_temp =
        words[combine_at + 11u];

    auto extract_other =
        [&](const instruction &mul,
            std::array<std::uint32_t,2> &other) noexcept {
            const auto at = mul.start;
            if (!temp_destination(
                    words[at + 1u],
                    words[at + 2u]) ||
                words[at + 2u] !=
                    out.tail_temp)
                return false;

            const std::array<std::uint32_t,2>
                a{words[at + 3u],
                  words[at + 4u]};
            const std::array<std::uint32_t,2>
                b{words[at + 5u],
                  words[at + 6u]};

            const bool a_tail =
                a[1] == out.tail_temp;
            const bool b_tail =
                b[1] == out.tail_temp;

            if (a_tail == b_tail)
                return false;

            other = a_tail ? b : a;
            return true;
        };

    if (!extract_other(
            source_mul,
            out.source_pair) ||
        !extract_other(
            atten_mul,
            out.attenuation_pair) ||
        !extract_other(
            factor_mul,
            out.source_factor_pair))
        return false;

    const auto accum_at =
        accum_mad.start;
    if (!temp_destination(
            words[accum_at + 1u],
            words[accum_at + 2u]) ||
        words[accum_at + 5u] !=
            k_imm_vector ||
        words[accum_at + 6u] !=
            k_pi_bits ||
        words[accum_at + 7u] !=
            k_pi_bits ||
        words[accum_at + 8u] !=
            k_pi_bits ||
        words[accum_at + 9u] != 0u ||
        !temp_pair(
            words.data() +
            accum_at + 10u) ||
        words[accum_at + 11u] !=
            words[accum_at + 2u])
        return false;

    out.accum_register =
        words[accum_at + 2u];
    out.combine_instruction =
        combine.start;
    out.final_accum_instruction =
        accum_mad.start;

    return true;
}

template<std::size_t N>
void append(
    std::vector<std::uint32_t> &dst,
    const std::array<std::uint32_t,N> &src)
{
    dst.insert(
        dst.end(),
        src.begin(),
        src.end());
}

void append_raw(
    std::vector<std::uint32_t> &dst,
    const std::uint32_t *src,
    std::size_t count)
{
    dst.insert(
        dst.end(),
        src,
        src + count);
}

void append_mov_zero_xyz(
    std::vector<std::uint32_t> &dst,
    std::uint32_t reg)
{
    const std::uint32_t op[] = {
        0x08000036u,
        k_temp_dst_xyz,
        reg,
        k_imm_vector,
        0u,0u,0u,0u
    };
    append_raw(dst,op,8u);
}

void append_mul_vec_temp_cb(
    std::vector<std::uint32_t> &dst,
    std::uint32_t d,
    std::uint32_t s,
    std::uint32_t cb_index)
{
    const std::uint32_t op[] = {
        0x08000038u,
        k_temp_dst_xyz,d,
        k_temp_src_xyz,s,
        k_cb_src_xyz,12u,cb_index
    };
    append_raw(dst,op,8u);
}

void append_mul_vec_temp_input(
    std::vector<std::uint32_t> &dst,
    std::uint32_t d,
    std::uint32_t s,
    std::uint32_t input)
{
    const std::uint32_t op[] = {
        0x07000038u,
        k_temp_dst_xyz,d,
        k_temp_src_xyz,s,
        k_input_src_xyz,input
    };
    append_raw(dst,op,7u);
}

void append_difference_vec(
    std::vector<std::uint32_t> &dst,
    std::uint32_t difference,
    std::uint32_t a,
    std::uint32_t b)
{
    const std::uint32_t op[] = {
        0x08000000u,
        k_temp_dst_xyz,difference,
        0x80100246u,0x00000041u,a,
        k_temp_src_xyz,b
    };
    append_raw(dst,op,8u);
}

void append_blend_vec(
    std::vector<std::uint32_t> &dst,
    std::uint32_t out,
    std::uint32_t weight_token,
    std::uint32_t weight_register,
    std::uint32_t difference,
    std::uint32_t a)
{
    const std::uint32_t op[] = {
        0x09000032u,
        k_temp_dst_xyz,out,
        weight_token,weight_register,
        k_temp_src_xyz,difference,
        k_temp_src_xyz,a
    };
    append_raw(dst,op,9u);
}

void append_mul_vec_scalar(
    std::vector<std::uint32_t> &dst,
    std::uint32_t d,
    std::uint32_t v,
    std::uint32_t scalar_reg,
    std::uint8_t component)
{
    const std::uint32_t op[] = {
        0x07000038u,
        k_temp_dst_xyz,d,
        k_temp_src_xyz,v,
        temp_src_scalar(component),
        scalar_reg
    };
    append_raw(dst,op,7u);
}

void append_mul_vec_pair(
    std::vector<std::uint32_t> &dst,
    std::uint32_t d,
    std::uint32_t v,
    const std::array<std::uint32_t,2> &pair)
{
    const std::uint32_t op[] = {
        0x07000038u,
        k_temp_dst_xyz,d,
        k_temp_src_xyz,v,
        pair[0],pair[1]
    };
    append_raw(dst,op,7u);
}

void append_add_accum(
    std::vector<std::uint32_t> &dst,
    std::uint32_t accum,
    std::uint32_t work)
{
    const std::uint32_t op[] = {
        0x07000000u,
        k_temp_dst_xyz,accum,
        k_temp_src_xyz,work,
        k_temp_src_xyz,accum
    };
    append_raw(dst,op,7u);
}

bool apply_insertions(
    std::vector<std::uint32_t> &words,
    std::vector<insertion> insertions) noexcept
{
    try {
        std::stable_sort(
            insertions.begin(),
            insertions.end(),
            [](const insertion &a,
               const insertion &b) {
                if (a.word != b.word)
                    return a.word > b.word;
                return a.erase_words >
                    b.erase_words;
            });

        for (const auto &current :
             insertions) {
            if (current.word >
                    words.size() ||
                current.erase_words >
                    words.size() -
                    current.word)
                return false;

            words.erase(
                words.begin() +
                    static_cast<std::ptrdiff_t>(
                        current.word),
                words.begin() +
                    static_cast<std::ptrdiff_t>(
                        current.word +
                        current.erase_words));

            words.insert(
                words.begin() +
                    static_cast<std::ptrdiff_t>(
                        current.word),
                current.payload.begin(),
                current.payload.end());
        }

        if (words.size() >
            std::numeric_limits<std::uint32_t>::max())
            return false;

        words[1] =
            static_cast<std::uint32_t>(
                words.size());
        return true;
    } catch (...) {
        return false;
    }
}

bool rebuild(
    const std::uint8_t *basis,
    std::size_t basis_size,
    std::vector<chunk> chunks,
    std::size_t code_index,
    const std::vector<std::uint32_t> &words,
    std::vector<std::uint8_t> &out) noexcept
{
    if (basis == nullptr ||
        basis_size < 32u ||
        code_index >= chunks.size())
        return false;

    try {
        auto &payload =
            chunks[code_index].payload;
        payload.resize(
            words.size() * 4u);

        for (std::size_t i = 0u;
             i < words.size();
             ++i)
            write_u32(
                payload.data() + i * 4u,
                words[i]);

        const auto header_size =
            32u + 4u * chunks.size();
        if (header_size >
            std::numeric_limits<std::uint32_t>::max())
            return false;

        out.assign(
            basis,
            basis +
                std::min<std::size_t>(
                    basis_size,
                    32u));
        out.resize(
            header_size,
            0u);
        std::memcpy(
            out.data(),
            basis,
            24u);

        write_u32(
            out.data() + 28u,
            static_cast<std::uint32_t>(
                chunks.size()));

        std::vector<std::uint32_t> offsets;
        offsets.reserve(
            chunks.size());

        for (const auto &current :
             chunks) {
            if (out.size() >
                std::numeric_limits<std::uint32_t>::max())
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
            std::numeric_limits<std::uint32_t>::max())
            return false;

        write_u32(
            out.data() + 24u,
            static_cast<std::uint32_t>(
                out.size()));

        for (std::size_t i = 0u;
             i < offsets.size();
             ++i)
            write_u32(
                out.data() +
                    32u + i * 4u,
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

} // namespace

clustered_spc_legacy_materialize_outcome
materialize_clustered_spc_legacy_specular(
    const std::uint8_t *stage1,
    std::size_t size,
    bool spc,
    bool blended_material,
    std::uint32_t representative_shader_index,
    std::vector<std::uint8_t> &output) noexcept
{
    clustered_spc_legacy_materialize_outcome out;
    output.clear();

    if (!spc)
        return out;

    out.blended_material =
        blended_material;
    out.representative_shader_index =
        representative_shader_index;

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index = 0u;
    if (!parse(
            stage1,
            size,
            chunks,
            code_index,
            words)) {
        out.result =
            clustered_spc_legacy_materialize_result::
                fail_invalid_dxbc;
        return out;
    }

    std::vector<instruction> instructions;
    if (!decode(
            words,
            instructions)) {
        out.result =
            clustered_spc_legacy_materialize_result::
                fail_invalid_dxbc;
        return out;
    }

    clustered_contract contract{};
    if (!extract_clustered_contract(
            words,
            instructions,
            contract)) {
        out.result =
            clustered_spc_legacy_materialize_result::
                fail_contract;
        return out;
    }

    const auto anchor_index =
        instruction_index(
            instructions,
            contract.schlick_instruction);
    if (anchor_index >=
        instructions.size()) {
        out.result =
            clustered_spc_legacy_materialize_result::
                fail_contract;
        return out;
    }

    material_capture material{};
    if (!locate_material_capture(
            words,
            instructions,
            anchor_index,
            blended_material,
            material)) {
        out.result =
            clustered_spc_legacy_materialize_result::
                fail_material_capture;
        return out;
    }

    std::uint32_t color0 = 0u;
    if (!parse_color0(
            chunks,
            color0)) {
        out.result =
            clustered_spc_legacy_materialize_result::
                fail_contract;
        return out;
    }
    out.color0_input_register =
        color0;

    std::uint32_t t1_decl = 0u;
    std::uint32_t t4_decl = 0u;
    std::uint32_t temps_word = 0u;
    std::uint32_t temp_count = 0u;
    std::uint32_t t10_count = 0u;
    std::uint32_t t16_count = 0u;

    if (!locate_declarations(
            words,
            instructions,
            t1_decl,
            t4_decl,
            temps_word,
            temp_count,
            t10_count,
            t16_count) ||
        t10_count != 0u ||
        t16_count != 0u ||
        (blended_material &&
         t4_decl == 0u)) {
        out.result =
            clustered_spc_legacy_materialize_result::
                fail_declaration_shape;
        return out;
    }

    out.original_temp_count =
        temp_count;

    constexpr std::uint32_t k_scratch_count =
        2u;
    const auto spec_mat =
        temp_count;
    const auto work =
        temp_count + 1u;

    if (work > 4095u) {
        out.result =
            clustered_spc_legacy_materialize_result::
                fail_declaration_shape;
        return out;
    }

    out.final_temp_count =
        temp_count + k_scratch_count;
    words[temps_word + 1u] =
        out.final_temp_count;

    const auto *t1_sample =
        find_instruction(
            instructions,
            material.spec_a.instruction_word);
    const auto *t4_sample =
        blended_material
            ? find_instruction(
                instructions,
                material.spec_b.
                    instruction_word)
            : nullptr;

    if (t1_sample == nullptr ||
        t1_sample->end -
            t1_sample->start != 11u ||
        (blended_material &&
         (t4_sample == nullptr ||
          t4_sample->end -
              t4_sample->start != 11u))) {
        out.result =
            clustered_spc_legacy_materialize_result::
                fail_material_capture;
        return out;
    }

    std::vector<insertion> insertions;

    insertion declarations{};
    declarations.word =
        temps_word;

    std::array<std::uint32_t,4>
        t10_decl_words{};
    std::copy_n(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                t1_decl),
        4u,
        t10_decl_words.begin());
    t10_decl_words[2] = 10u;
    append(
        declarations.payload,
        t10_decl_words);
    out.t10_declared = true;

    if (blended_material) {
        std::array<std::uint32_t,4>
            t16_decl_words{};
        std::copy_n(
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    t4_decl),
            4u,
            t16_decl_words.begin());
        t16_decl_words[2] = 16u;
        append(
            declarations.payload,
            t16_decl_words);
        out.t16_declared = true;
    }

    insertions.push_back(
        std::move(declarations));

    insertion spec_a{};
    spec_a.word =
        t1_sample->end;
    spec_a.payload.assign(
        words.begin() +
            static_cast<std::ptrdiff_t>(
                t1_sample->start),
        words.begin() +
            static_cast<std::ptrdiff_t>(
                t1_sample->end));

    if (spec_a.payload[8] != 1u ||
        spec_a.payload[4] !=
            material.spec_a.
                destination_register) {
        out.result =
            clustered_spc_legacy_materialize_result::
                fail_material_capture;
        return out;
    }

    spec_a.payload[3] =
        k_temp_dst_xyz;
    spec_a.payload[4] =
        spec_mat;
    spec_a.payload[8] = 10u;

    append_mul_vec_temp_cb(
        spec_a.payload,
        spec_mat,
        spec_mat,
        2u);

    if (!blended_material) {
        append_mul_vec_temp_input(
            spec_a.payload,
            spec_mat,
            spec_mat,
            color0);
        insertions.push_back(
            std::move(spec_a));
    } else {
        insertions.push_back(
            std::move(spec_a));

        insertion spec_b{};
        spec_b.word =
            t4_sample->end;
        spec_b.payload.assign(
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    t4_sample->start),
            words.begin() +
                static_cast<std::ptrdiff_t>(
                    t4_sample->end));

        if (spec_b.payload[8] != 4u ||
            spec_b.payload[4] !=
                material.spec_b.
                    destination_register ||
            material.blend_weight_token ==
                0u) {
            out.result =
                clustered_spc_legacy_materialize_result::
                    fail_material_capture;
            return out;
        }

        spec_b.payload[3] =
            k_temp_dst_xyz;
        spec_b.payload[4] = work;
        spec_b.payload[8] = 16u;

        append_mul_vec_temp_cb(
            spec_b.payload,
            work,
            work,
            2u);
        append_difference_vec(
            spec_b.payload,
            work,
            spec_mat,
            work);
        append_blend_vec(
            spec_b.payload,
            spec_mat,
            material.blend_weight_token,
            material.blend_weight_register,
            work,
            spec_mat);
        append_mul_vec_temp_input(
            spec_b.payload,
            spec_mat,
            spec_mat,
            color0);

        insertions.push_back(
            std::move(spec_b));
    }

    insertion kill_stock_spec{};
    kill_stock_spec.word =
        contract.combine_instruction;
    append_mov_zero_xyz(
        kill_stock_spec.payload,
        contract.stock_specular_temp);
    insertions.push_back(
        std::move(kill_stock_spec));

    insertion legacy{};
    const auto *accum_instruction =
        find_instruction(
            instructions,
            contract.final_accum_instruction);
    if (accum_instruction == nullptr) {
        out.result =
            clustered_spc_legacy_materialize_result::
                fail_contract;
        return out;
    }

    legacy.word =
        accum_instruction->end;

    append(
        legacy.payload,
        contract.kernel.words);

    append_mul_vec_scalar(
        legacy.payload,
        work,
        spec_mat,
        contract.kernel.
            result_temp_register,
        contract.kernel.
            result_component);

    append_mul_vec_pair(
        legacy.payload,
        work,
        work,
        contract.source_pair);
    append_mul_vec_pair(
        legacy.payload,
        work,
        work,
        contract.attenuation_pair);
    append_mul_vec_pair(
        legacy.payload,
        work,
        work,
        contract.source_factor_pair);
    append_add_accum(
        legacy.payload,
        contract.accum_register,
        work);

    insertions.push_back(
        std::move(legacy));

    if (!apply_insertions(
            words,
            std::move(insertions))) {
        out.result =
            clustered_spc_legacy_materialize_result::
                fail_rebuild;
        return out;
    }

    if (!decode(
            words,
            instructions)) {
        out.result =
            clustered_spc_legacy_materialize_result::
                fail_postcondition;
        return out;
    }

    if (!rebuild(
            stage1,
            size,
            std::move(chunks),
            code_index,
            words,
            output) ||
        !legacy_plan::dxbc::
            checksum_container_valid(
                output.data(),
                output.size())) {
        output.clear();
        out.result =
            clustered_spc_legacy_materialize_result::
                fail_rebuild;
        return out;
    }

    out.replacement_size =
        output.size();
    out.replacement_sha256 =
        hashing::sha256(
            output.data(),
            output.size());
    out.stock_microfacet_killed_at_output_cut =
        true;
    out.common_ndotl_specular_bypassed =
        true;
    out.ptde_legacy_specular_added =
        true;
    out.result =
        clustered_spc_legacy_materialize_result::
            applied;
    return out;
}

} // namespace dsrrl::operators::point_light
