#include "dsrrl/operators/point_light/fixed_local_specular_operand_contract.hpp"

#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dsrrl::operators::point_light {
namespace {

constexpr std::uint32_t k_shex_tag = 0x58454853u;
constexpr std::uint32_t k_shdr_tag = 0x52444853u;
constexpr std::uint16_t k_op_dp3 = 16u;
constexpr std::uint16_t k_op_endswitch = 23u;
constexpr std::uint16_t k_op_customdata = 53u;
constexpr std::uint16_t k_op_mul = 56u;
constexpr std::size_t k_max_words = 8192u;
constexpr std::size_t k_max_instructions = 4096u;

struct instruction_view {
    std::uint32_t start = 0u;
    std::uint32_t end = 0u;
    std::uint16_t opcode = 0u;
};

std::uint32_t read_u32(
    const std::uint8_t *bytes,
    std::size_t offset) noexcept
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

bool code_words(
    const void *code,
    std::size_t size,
    std::array<std::uint32_t,k_max_words> &words,
    std::size_t &word_count) noexcept
{
    word_count = 0u;
    if (code == nullptr ||
        size < 36u ||
        !legacy_plan::dxbc::checksum_container_valid(
            static_cast<const std::uint8_t *>(code),
            size))
        return false;

    const auto *bytes =
        static_cast<const std::uint8_t *>(code);
    const auto chunk_count = read_u32(bytes,28u);
    if (chunk_count == 0u ||
        chunk_count > 64u ||
        32ull + 4ull * chunk_count > size)
        return false;

    bool found = false;
    for (std::uint32_t i=0u; i<chunk_count; ++i) {
        const auto off =
            static_cast<std::size_t>(
                read_u32(bytes,32u + i*4u));
        if (off > size || size-off < 8u)
            return false;

        const auto tag = read_u32(bytes,off);
        if (tag != k_shex_tag &&
            tag != k_shdr_tag)
            continue;
        if (found)
            return false;
        found = true;

        const auto payload =
            static_cast<std::size_t>(
                read_u32(bytes,off+4u));
        if ((payload & 3u) != 0u ||
            payload > size-off-8u)
            return false;

        word_count = payload/4u;
        if (word_count < 3u ||
            word_count > words.size())
            return false;

        std::memcpy(
            words.data(),
            bytes+off+8u,
            payload);
        if (words[1] != word_count)
            return false;
    }

    return found;
}

bool decode_instructions(
    const std::uint32_t *words,
    std::size_t word_count,
    std::array<instruction_view,k_max_instructions> &out,
    std::size_t &count) noexcept
{
    count = 0u;
    std::size_t at = 2u;
    while (at < word_count) {
        if (count >= out.size())
            return false;

        const auto token = words[at];
        const auto opcode =
            static_cast<std::uint16_t>(
                token & 0x7ffu);
        std::uint32_t length = 0u;
        if (opcode == k_op_customdata) {
            if (at+1u >= word_count)
                return false;
            length = words[at+1u];
        } else {
            length = (token>>24u)&0x7fu;
        }

        if (length == 0u ||
            length > word_count-at)
            return false;

        out[count++] = {
            static_cast<std::uint32_t>(at),
            static_cast<std::uint32_t>(at+length),
            opcode
        };
        at += length;
    }
    return at == word_count;
}

bool pair_equal(
    const std::uint32_t *a,
    const std::uint32_t *b) noexcept
{
    return a[0] == b[0] &&
           a[1] == b[1];
}

bool temp_pair(
    const std::uint32_t *pair) noexcept
{
    const auto token = pair[0];
    const auto type = (token>>12u)&0xffu;
    const auto dim = (token>>20u)&0x3u;
    const auto rep = (token>>22u)&0x7u;
    return type == 0u &&
           dim == 1u &&
           rep == 0u;
}

bool has_cb0_index(
    const std::uint32_t *words,
    const instruction_view &instruction,
    std::uint32_t index) noexcept
{
    for (std::uint32_t w=instruction.start+1u;
         w+2u<instruction.end;
         ++w) {
        const auto token = words[w];
        const auto type = (token>>12u)&0xffu;
        const auto dim = (token>>20u)&0x3u;
        const auto rep0 = (token>>22u)&0x7u;
        const auto rep1 = (token>>25u)&0x7u;
        if (type == 8u &&
            dim == 2u &&
            rep0 == 0u &&
            rep1 == 0u &&
            words[w+1u] == 0u &&
            words[w+2u] == index)
            return true;
    }
    return false;
}

bool find_instruction_by_word(
    const std::array<instruction_view,k_max_instructions> &instructions,
    std::size_t count,
    std::uint32_t word,
    std::size_t &index) noexcept
{
    for (std::size_t i=0u;i<count;++i) {
        if (instructions[i].start == word) {
            index=i;
            return true;
        }
    }
    return false;
}

} // namespace

fixed_local_specular_operand_contract
extract_fixed_local_specular_operand_contract_from_attested_shex_words(
    const fixed_local_specular_patch_plan &plan,
    const std::uint32_t *shex_words,
    std::size_t word_count) noexcept
{
    fixed_local_specular_operand_contract out;
    out.plan = plan;

    if (out.plan.result ==
            fixed_local_specular_plan_result::
                pass_not_local_specular_receiver ||
        out.plan.result ==
            fixed_local_specular_plan_result::
                pass_clustered_membership_not_owned) {
        out.result =
            fixed_local_specular_operand_result::
                pass_not_fixed_local_specular;
        return out;
    }

    if (out.plan.result !=
        fixed_local_specular_plan_result::ready) {
        out.result =
            fixed_local_specular_operand_result::
                fail_plan_not_ready;
        return out;
    }

    if (shex_words == nullptr ||
        word_count < 3u ||
        word_count > k_max_words ||
        shex_words[1] != word_count) {
        out.result =
            fixed_local_specular_operand_result::
                fail_invalid_dxbc;
        return out;
    }

    std::array<instruction_view,k_max_instructions>
        instructions{};
    std::size_t instruction_count=0u;
    if (!decode_instructions(
            shex_words,
            word_count,
            instructions,
            instruction_count)) {
        out.result =
            fixed_local_specular_operand_result::
                fail_invalid_dxbc;
        return out;
    }

    for (std::uint8_t light=0u;
         light<out.plan.light_count;
         ++light) {
        const auto &window =
            out.plan.lights[light].
                microfacet_window;

        std::size_t anchor=0u;
        if (!find_instruction_by_word(
                instructions,
                instruction_count,
                window.schlick_word,
                anchor) ||
            anchor < 3u) {
            out.result =
                fixed_local_specular_operand_result::
                    fail_window_bounds;
            return out;
        }

        const auto &dp_vh=instructions[anchor-3u];
        const auto &dp_nh=instructions[anchor-2u];
        const auto &dp_nl=instructions[anchor-1u];
        if (dp_vh.opcode != k_op_dp3 ||
            dp_nh.opcode != k_op_dp3 ||
            dp_nl.opcode != k_op_dp3 ||
            dp_vh.end-dp_vh.start != 7u ||
            dp_nh.end-dp_nh.start != 7u ||
            dp_nl.end-dp_nl.start != 7u) {
            out.result =
                fixed_local_specular_operand_result::
                    fail_dp3_shape;
            return out;
        }

        // Every audited fixed DP3 here has:
        //   opcode | dst(2) | src0(2) | src1(2)
        const auto *vh_src0 =
            shex_words+dp_vh.start+3u;
        const auto *vh_src1 =
            shex_words+dp_vh.start+5u;
        const auto *nh_src0 =
            shex_words+dp_nh.start+3u;
        const auto *nh_src1 =
            shex_words+dp_nh.start+5u;
        const auto *nl_src0 =
            shex_words+dp_nl.start+3u;
        const auto *nl_src1 =
            shex_words+dp_nl.start+5u;

        // Semantic identities:
        //   DP3(V,H), DP3(N,H), DP3(N,L)
        if (!temp_pair(vh_src0) ||
            !temp_pair(vh_src1) ||
            !temp_pair(nh_src0) ||
            !temp_pair(nh_src1) ||
            !temp_pair(nl_src0) ||
            !temp_pair(nl_src1) ||
            !pair_equal(vh_src1,nh_src1) ||
            !pair_equal(nh_src0,nl_src0)) {
            out.result =
                fixed_local_specular_operand_result::
                    fail_operand_shape;
            return out;
        }

        const auto expected_position =
            static_cast<std::uint32_t>(
                out.plan.lights[light].
                    position_begin_cb);
        const auto expected_color =
            static_cast<std::uint32_t>(
                out.plan.lights[light].
                    color_end_cb);

        bool has_position=false;
        bool has_color=false;
        bool tail_color=false;
        bool after_switch=false;

        std::size_t window_start=0u;
        std::size_t window_end=0u;
        if (!find_instruction_by_word(
                instructions,
                instruction_count,
                window.start_word,
                window_start)) {
            out.result =
                fixed_local_specular_operand_result::
                    fail_window_bounds;
            return out;
        }

        bool found_end=false;
        for (std::size_t i=window_start;
             i<instruction_count;
             ++i) {
            const auto &instruction =
                instructions[i];
            if (instruction.start >=
                window.end_word_exclusive) {
                window_end=i;
                found_end=true;
                break;
            }

            has_position = has_position ||
                has_cb0_index(
                    shex_words,
                    instruction,
                    expected_position);
            has_color = has_color ||
                has_cb0_index(
                    shex_words,
                    instruction,
                    expected_color);

            if (instruction.opcode ==
                k_op_endswitch)
                after_switch=true;
            else if (after_switch &&
                     instruction.opcode ==
                        k_op_mul &&
                     has_cb0_index(
                        shex_words,
                        instruction,
                        expected_color))
                tail_color=true;
        }

        if (!found_end)
            window_end=instruction_count;

        if (!has_position ||
            !has_color ||
            !tail_color ||
            window_end <= window_start) {
            out.result =
                fixed_local_specular_operand_result::
                    fail_fixed_cb_identity;
            return out;
        }

        auto &dst=out.lights[light];
        dst.view.token = {
            vh_src0[0],vh_src0[1]};
        dst.normal.token = {
            nh_src0[0],nh_src0[1]};
        dst.light.token = {
            nl_src1[0],nl_src1[1]};
        dst.primary_scalar_dst.token = {
            shex_words[dp_vh.start+1u],
            shex_words[dp_vh.start+2u]};
        dst.auxiliary_scalar_dst.token = {
            shex_words[dp_nh.start+1u],
            shex_words[dp_nh.start+2u]};
        dst.ndotl_scalar_dst.token = {
            shex_words[dp_nl.start+1u],
            shex_words[dp_nl.start+2u]};
        dst.ndotl_dp3_word =
            dp_nl.start;
        dst.stock_light_color_cb =
            expected_color;
        dst.stock_position_begin_cb =
            expected_position;
    }

    out.light_count=out.plan.light_count;
    out.stock_specular_power_cb_slot=
        out.plan.stock_specular_power_cb_slot;
    out.stock_specular_power_cb_index=
        out.plan.stock_specular_power_cb_index;
    out.stock_specular_power_component=
        out.plan.stock_specular_power_component;
    out.ptde_specular_power_cb_slot=
        out.plan.ptde_specular_power_cb_slot;
    out.ptde_specular_power_cb_index=
        out.plan.ptde_specular_power_cb_index;
    out.ptde_specular_power_component=
        out.plan.ptde_specular_power_component;
    out.exponent_carrier_attested=
        out.plan.use_ptde_legacy_reflect_pow &&
        out.plan.consume_g_specular_power_as_exponent &&
        out.stock_specular_power_cb_slot==0u &&
        out.stock_specular_power_cb_index==11u &&
        out.stock_specular_power_component==0u &&
        out.ptde_specular_power_cb_slot==12u &&
        out.ptde_specular_power_cb_index==0u &&
        out.ptde_specular_power_component==3u;

    if (!out.exponent_carrier_attested) {
        out.result =
            fixed_local_specular_operand_result::
                fail_fixed_cb_identity;
        return out;
    }

    out.result =
        fixed_local_specular_operand_result::ready;
    return out;
}


fixed_local_specular_operand_contract
extract_fixed_local_specular_operand_contract(
    const void *pixel_shader_code,
    std::size_t code_size) noexcept
{
    const auto plan =
        build_fixed_local_specular_patch_plan(
            pixel_shader_code,
            code_size);

    if (plan.result !=
        fixed_local_specular_plan_result::ready)
        return
            extract_fixed_local_specular_operand_contract_from_attested_shex_words(
                plan,
                nullptr,
                0u);

    std::array<std::uint32_t,k_max_words> words{};
    std::size_t word_count=0u;
    if (!code_words(
            pixel_shader_code,
            code_size,
            words,
            word_count)) {
        fixed_local_specular_operand_contract out;
        out.plan=plan;
        out.result=
            fixed_local_specular_operand_result::
                fail_invalid_dxbc;
        return out;
    }

    return
        extract_fixed_local_specular_operand_contract_from_attested_shex_words(
            plan,
            words.data(),
            word_count);
}

} // namespace dsrrl::operators::point_light
