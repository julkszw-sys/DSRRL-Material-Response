#include "dsrrl/operators/point_light/fixed_local_specular_output_cut.hpp"

#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dsrrl::operators::point_light {
namespace {

constexpr std::uint32_t k_shex_tag = 0x58454853u;
constexpr std::uint32_t k_shdr_tag = 0x52444853u;
constexpr std::uint16_t k_op_add = 0u;
constexpr std::uint16_t k_op_customdata = 53u;
constexpr std::uint16_t k_op_mov = 54u;
constexpr std::size_t k_max_words = 8192u;
constexpr std::size_t k_max_instructions = 4096u;

struct instruction_view {
    std::uint32_t start = 0u;
    std::uint32_t end = 0u;
    std::uint16_t opcode = 0u;
};

std::uint32_t read_u32(const std::uint8_t *p) noexcept
{
    std::uint32_t v = 0u;
    std::memcpy(&v,p,sizeof(v));
    return v;
}

bool extract_code(
    const void *code,
    std::size_t size,
    std::array<std::uint32_t,k_max_words> &words,
    std::size_t &word_count) noexcept
{
    word_count = 0u;
    if (code == nullptr || size < 36u ||
        !legacy_plan::dxbc::checksum_container_valid(
            static_cast<const std::uint8_t *>(code),size))
        return false;

    const auto *bytes = static_cast<const std::uint8_t *>(code);
    const auto chunks = read_u32(bytes+28u);
    if (chunks == 0u || chunks > 64u ||
        32ull + 4ull*chunks > size)
        return false;

    bool found = false;
    for (std::uint32_t i=0u;i<chunks;++i) {
        const auto off = static_cast<std::size_t>(
            read_u32(bytes+32u+i*4u));
        if (off > size || size-off < 8u)
            return false;
        const auto tag = read_u32(bytes+off);
        if (tag != k_shex_tag && tag != k_shdr_tag)
            continue;
        if (found)
            return false;
        found = true;
        const auto payload = static_cast<std::size_t>(
            read_u32(bytes+off+4u));
        if ((payload&3u)!=0u || payload > size-off-8u)
            return false;
        word_count = payload/4u;
        if (word_count < 3u || word_count > words.size())
            return false;
        std::memcpy(words.data(),bytes+off+8u,payload);
        if (words[1] != word_count)
            return false;
    }
    return found;
}

bool decode(
    const std::uint32_t *words,
    std::size_t word_count,
    std::array<instruction_view,k_max_instructions> &out,
    std::size_t &count) noexcept
{
    count=0u;
    std::size_t at=2u;
    while (at<word_count) {
        if (count>=out.size())
            return false;
        const auto token=words[at];
        const auto opcode=static_cast<std::uint16_t>(token&0x7ffu);
        std::uint32_t length=0u;
        if (opcode==k_op_customdata) {
            if (at+1u>=word_count)
                return false;
            length=words[at+1u];
        } else {
            length=(token>>24u)&0x7fu;
        }
        if (length==0u || length>word_count-at)
            return false;
        out[count++] = {
            static_cast<std::uint32_t>(at),
            static_cast<std::uint32_t>(at+length),
            opcode
        };
        at+=length;
    }
    return at==word_count;
}

bool has_cb0_index(
    const std::uint32_t *words,
    const instruction_view &instruction,
    std::uint32_t index) noexcept
{
    for (std::uint32_t w=instruction.start+1u;
         w+2u<instruction.end;++w) {
        const auto token=words[w];
        if (((token>>12u)&0xffu)==8u &&
            ((token>>20u)&0x3u)==2u &&
            ((token>>22u)&0x7u)==0u &&
            ((token>>25u)&0x7u)==0u &&
            words[w+1u]==0u &&
            words[w+2u]==index)
            return true;
    }
    return false;
}

bool direct_temp_component_mask(
    const std::uint32_t token,
    std::uint8_t &mask) noexcept
{
    mask=0u;
    if (((token>>12u)&0xffu)!=0u ||
        ((token>>20u)&0x3u)!=1u ||
        ((token>>22u)&0x7u)!=0u)
        return false;

    const auto components=token&0x3u;
    if (components==1u) {
        mask=0x1u;
        return true;
    }
    if (components!=2u)
        return false;

    const auto mode=(token>>2u)&0x3u;
    if (mode==0u) {
        mask=static_cast<std::uint8_t>(
            (token>>4u)&0xfu);
        return mask!=0u;
    }
    if (mode==1u) {
        const auto swizzle=(token>>4u)&0xffu;
        for (std::uint32_t lane=0u;lane<4u;++lane)
            mask=static_cast<std::uint8_t>(
                mask |
                (1u<<((swizzle>>(lane*2u))&0x3u)));
        return mask!=0u;
    }
    if (mode==2u) {
        mask=static_cast<std::uint8_t>(
            1u<<((token>>4u)&0x3u));
        return true;
    }
    return false;
}

bool accumulate_direct_temp_component_uses(
    const std::uint32_t *words,
    const instruction_view &instruction,
    std::uint32_t temp_register,
    std::uint8_t &mask,
    std::uint32_t &use_count) noexcept
{
    if (instruction.opcode==k_op_customdata)
        return true;

    for (std::uint32_t w=instruction.start+1u;
         w+1u<instruction.end;
         ++w) {
        const auto token=words[w];
        if (((token>>12u)&0xffu)!=0u ||
            ((token>>20u)&0x3u)!=1u ||
            ((token>>22u)&0x7u)!=0u ||
            words[w+1u]!=temp_register)
            continue;

        std::uint8_t current=0u;
        if (!direct_temp_component_mask(
                token,current))
            return false;

        mask=static_cast<std::uint8_t>(
            mask|current);
        ++use_count;
        ++w;
    }
    return true;
}

} // namespace

fixed_local_specular_output_cut
locate_fixed_local_specular_output_cut(
    const void *pixel_shader_code,
    std::size_t code_size) noexcept
{
    fixed_local_specular_output_cut out;
    out.operands =
        extract_fixed_local_specular_operand_contract(
            pixel_shader_code,code_size);

    if (out.operands.result ==
        fixed_local_specular_operand_result::pass_not_fixed_local_specular) {
        out.result =
            fixed_local_specular_output_cut_result::
                pass_not_fixed_local_specular;
        return out;
    }

    if (out.operands.result !=
        fixed_local_specular_operand_result::ready) {
        out.result =
            fixed_local_specular_output_cut_result::
                fail_operand_contract;
        return out;
    }

    std::array<std::uint32_t,k_max_words> words{};
    std::size_t word_count=0u;
    std::array<instruction_view,k_max_instructions> instructions{};
    std::size_t instruction_count=0u;

    if (!extract_code(
            pixel_shader_code,code_size,words,word_count) ||
        !decode(
            words.data(),word_count,
            instructions,instruction_count)) {
        out.result =
            fixed_local_specular_output_cut_result::fail_invalid_dxbc;
        return out;
    }

    const auto final_end_word =
        out.operands.plan.lights[
            out.operands.light_count-1u].
                microfacet_window.end_word_exclusive;

    std::size_t first_after_window=instruction_count;
    for (std::size_t i=0u;i<instruction_count;++i) {
        if (instructions[i].start >= final_end_word) {
            first_after_window=i;
            break;
        }
    }
    if (first_after_window==instruction_count) {
        out.result =
            fixed_local_specular_output_cut_result::fail_join_shape;
        return out;
    }

    std::size_t fog=instruction_count;
    for (std::size_t i=first_after_window;i<instruction_count;++i) {
        if (has_cb0_index(words.data(),instructions[i],103u)) {
            fog=i;
            break;
        }
    }

    if (fog==instruction_count || fog==0u ||
        instructions[fog-1u].opcode != k_op_mov) {
        out.result =
            fixed_local_specular_output_cut_result::fail_fog_anchor;
        return out;
    }

    const auto bridge_mov=fog-1u;

    // Fixed Spc exact corpus invariant: the owned local-light join is the
    // first instruction immediately after the final per-light ENDIF. Do not
    // scan forward for arbitrary ADD/MAD instructions: 24/48 bodies contain
    // a later continuation MAD before Fog which belongs to another expression.
    const auto join=first_after_window;
    if (join>=bridge_mov) {
        out.result =
            fixed_local_specular_output_cut_result::fail_join_shape;
        return out;
    }

    const auto &j=instructions[join];
    if (j.opcode!=k_op_add || j.end-j.start!=7u) {
        out.result =
            fixed_local_specular_output_cut_result::fail_join_shape;
        return out;
    }

    const std::uint32_t token_word=j.start+5u;
    const std::uint32_t index_word=j.start+6u;

    std::uint8_t join_component_mask=0u;
    if (index_word>=word_count ||
        !direct_temp_component_mask(
            words[token_word],
            join_component_mask) ||
        join_component_mask!=0x7u) {
        out.result =
            fixed_local_specular_output_cut_result::
                fail_local_operand_shape;
        return out;
    }

    const auto stock_local_temp=words[index_word];
    const auto first_window_word=
        out.operands.plan.lights[0u].
            microfacet_window.start_word;

    std::uint8_t owned_window_mask=0u;
    std::uint8_t downstream_mask=0u;
    std::uint32_t owned_window_uses=0u;
    std::uint32_t downstream_uses=0u;

    for (std::size_t i=0u;
         i<instruction_count;
         ++i) {
        const auto &instruction=instructions[i];
        if (instruction.start>=first_window_word &&
            instruction.end<=final_end_word) {
            if (!accumulate_direct_temp_component_uses(
                    words.data(),
                    instruction,
                    stock_local_temp,
                    owned_window_mask,
                    owned_window_uses)) {
                out.result =
                    fixed_local_specular_output_cut_result::
                        fail_local_component_liveness;
                return out;
            }
        }

        if (i>join) {
            if (!accumulate_direct_temp_component_uses(
                    words.data(),
                    instruction,
                    stock_local_temp,
                    downstream_mask,
                    downstream_uses)) {
                out.result =
                    fixed_local_specular_output_cut_result::
                        fail_local_component_liveness;
                return out;
            }
        }
    }

    // Exact 48-body fixed-Spc liveness contract:
    //   24 bodies: owned window xyz, downstream none
    //   16 bodies: owned window xyzw, downstream none
    //    8 bodies: owned window xyz, downstream w only
    //
    // The redirected PointLight join owns xyz. Any later xyz consumer would
    // keep the DSR microfacet/local-light tail live outside the cut. A later
    // component may survive only when the owned PointLight windows never
    // touch that component. This preserves the eight unrelated o3.w carriers
    // without allowing a DSR-only PointLight value to escape downstream.
    if ((owned_window_mask&0x7u)!=0x7u ||
        (downstream_mask&0x7u)!=0u ||
        (owned_window_mask&downstream_mask)!=0u) {
        out.result =
            fixed_local_specular_output_cut_result::
                fail_local_component_liveness;
        return out;
    }

    out.join_word=j.start;
    out.join_opcode=j.opcode;
    out.local_operand_token_word=token_word;
    out.local_operand_index_word=index_word;
    out.stock_local_temp=stock_local_temp;
    out.stock_local_join_component_mask=join_component_mask;
    out.stock_local_owned_window_component_mask=owned_window_mask;
    out.stock_local_downstream_component_mask=downstream_mask;
    out.stock_local_owned_window_use_count=owned_window_uses;
    out.stock_local_downstream_use_count=downstream_uses;
    out.stock_local_owned_components_dead_after_join=true;
    out.bridge_mov_word=instructions[bridge_mov].start;
    out.first_fog_word=instructions[fog].start;
    out.result=fixed_local_specular_output_cut_result::exact;
    return out;
}

} // namespace dsrrl::operators::point_light
