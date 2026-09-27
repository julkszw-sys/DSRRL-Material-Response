#include "dsrrl/operators/point_light/fixed_local_specular_material_samples.hpp"

#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dsrrl::operators::point_light {
namespace {

constexpr std::uint32_t k_shex_tag = 0x58454853u;
constexpr std::uint32_t k_shdr_tag = 0x52444853u;
constexpr std::uint16_t k_op_customdata = 53u;
constexpr std::size_t k_max_words = 8192u;

std::uint32_t read_u32(const std::uint8_t *p) noexcept
{
    std::uint32_t value=0u;
    std::memcpy(&value,p,sizeof(value));
    return value;
}

bool extract_words(
    const void *code,
    std::size_t size,
    std::array<std::uint32_t,k_max_words> &words,
    std::size_t &word_count) noexcept
{
    word_count=0u;
    if (code==nullptr || size<36u ||
        !legacy_plan::dxbc::checksum_container_valid(
            static_cast<const std::uint8_t *>(code),size))
        return false;

    const auto *bytes=static_cast<const std::uint8_t *>(code);
    const auto count=read_u32(bytes+28u);
    if (count==0u || count>64u ||
        32ull+4ull*count>size)
        return false;

    bool found=false;
    for(std::uint32_t i=0u;i<count;++i) {
        const auto off=static_cast<std::size_t>(
            read_u32(bytes+32u+i*4u));
        if (off>size || size-off<8u)
            return false;

        const auto tag=read_u32(bytes+off);
        if (tag!=k_shex_tag && tag!=k_shdr_tag)
            continue;
        if (found)
            return false;
        found=true;

        const auto payload=static_cast<std::size_t>(
            read_u32(bytes+off+4u));
        if ((payload&3u)!=0u || payload>size-off-8u)
            return false;
        word_count=payload/4u;
        if (word_count<3u || word_count>words.size())
            return false;
        std::memcpy(words.data(),bytes+off+8u,payload);
        if (words[1]!=word_count)
            return false;
    }
    return found;
}

bool temp_destination(
    std::uint32_t token,
    std::uint32_t reg) noexcept
{
    return ((token>>12u)&0xffu)==0u &&
           ((token>>20u)&0x3u)==1u &&
           ((token>>22u)&0x7u)==0u &&
           reg<=4095u;
}

struct decoded_instruction {
    std::uint32_t word = 0u;
    std::uint16_t opcode = 0u;
    std::uint32_t length = 0u;
};

bool decode_instructions(
    const std::array<std::uint32_t,k_max_words> &words,
    std::size_t word_count,
    std::array<decoded_instruction,k_max_words> &out,
    std::size_t &count) noexcept
{
    count=0u;
    std::size_t at=2u;
    while(at<word_count) {
        const auto token=words[at];
        const auto opcode=
            static_cast<std::uint16_t>(token&0x7ffu);
        std::uint32_t length=0u;
        if(opcode==k_op_customdata) {
            if(at+1u>=word_count)
                return false;
            length=words[at+1u];
        } else {
            length=(token>>24u)&0x7fu;
        }

        if(length==0u ||
           length>word_count-at ||
           count>=out.size())
            return false;

        out[count++]={
            static_cast<std::uint32_t>(at),
            opcode,
            length
        };
        at+=length;
    }
    return at==word_count;
}

std::size_t instruction_index(
    const std::array<decoded_instruction,k_max_words> &instructions,
    std::size_t count,
    std::uint32_t word) noexcept
{
    for(std::size_t i=0u;i<count;++i)
        if(instructions[i].word==word)
            return i;
    return count;
}

bool difference_add(
    const std::array<std::uint32_t,k_max_words> &words,
    const decoded_instruction &ins,
    std::uint32_t a,
    std::uint32_t b) noexcept
{
    if(ins.opcode!=0u || ins.length!=8u)
        return false;

    const auto at=ins.word;
    return
        words[at+1u]==0x00100072u &&
        words[at+2u]==b &&
        words[at+3u]==0x80100246u &&
        words[at+4u]==0x00000041u &&
        words[at+5u]==a &&
        words[at+6u]==0x00100246u &&
        words[at+7u]==b;
}

bool blend_mad(
    const std::array<std::uint32_t,k_max_words> &words,
    const decoded_instruction &ins,
    std::uint32_t a,
    std::uint32_t b,
    std::uint32_t &weight_token,
    std::uint32_t &weight_register) noexcept
{
    if(ins.opcode!=50u || ins.length!=9u)
        return false;

    const auto at=ins.word;
    if(words[at+1u]!=0x00100072u ||
       words[at+2u]!=a ||
       words[at+5u]!=0x00100246u ||
       words[at+6u]!=b ||
       words[at+7u]!=0x00100246u ||
       words[at+8u]!=a)
        return false;

    const auto token=words[at+3u];
    const auto reg=words[at+4u];

    // Exact census shows a vector input operand; preserve its swizzle token
    // and register rather than assuming a fixed TEXCOORD/COLOR register.
    if(((token>>12u)&0xffu)!=1u ||
       ((token>>20u)&0x3u)!=1u ||
       reg>4095u)
        return false;

    weight_token=token;
    weight_register=reg;
    return true;
}

} // namespace

fixed_local_specular_material_samples
locate_fixed_local_specular_material_samples(
    const void *pixel_shader_code,
    std::size_t code_size) noexcept
{
    fixed_local_specular_material_samples out;
    out.island=build_fixed_local_specular_island_plan(
        pixel_shader_code,code_size);

    if (out.island.result ==
        fixed_local_specular_island_plan_result::
            pass_not_fixed_local_specular) {
        out.result=
            fixed_local_specular_material_sample_result::
                pass_not_fixed_local_specular;
        return out;
    }
    if (out.island.result !=
        fixed_local_specular_island_plan_result::ready) {
        out.result=
            fixed_local_specular_material_sample_result::
                fail_island_plan;
        return out;
    }

    std::array<std::uint32_t,k_max_words> words{};
    std::size_t word_count=0u;
    if (!extract_words(
            pixel_shader_code,code_size,words,word_count)) {
        out.result=
            fixed_local_specular_material_sample_result::
                fail_invalid_dxbc;
        return out;
    }

    const auto first_light=
        out.island.output_cut.operands.plan.lights[0].
            microfacet_window.start_word;

    std::array<std::uint8_t,5> counts{};
    std::array<fixed_local_specular_sample_site,5> sites{};

    std::size_t at=2u;
    while(at<word_count && at<first_light) {
        const auto token=words[at];
        const auto opcode=static_cast<std::uint16_t>(token&0x7ffu);
        std::uint32_t length=0u;
        if (opcode==k_op_customdata) {
            if (at+1u>=word_count) {
                out.result=
                    fixed_local_specular_material_sample_result::
                        fail_invalid_dxbc;
                return out;
            }
            length=words[at+1u];
        } else {
            length=(token>>24u)&0x7fu;
        }
        if (length==0u || length>word_count-at) {
            out.result=
                fixed_local_specular_material_sample_result::
                    fail_invalid_dxbc;
            return out;
        }

        if (opcode>=0x45u && opcode<=0x4au &&
            length==11u && at+8u<word_count) {
            const auto resource=words[at+8u];
            if (resource<=4u &&
                (resource==0u || resource==1u ||
                 resource==3u || resource==4u)) {
                if (at+4u>=word_count ||
                    !temp_destination(
                        words[at+3u],
                        words[at+4u])) {
                    out.result=
                        fixed_local_specular_material_sample_result::
                            fail_sample_shape;
                    return out;
                }
                if (++counts[resource] != 1u) {
                    out.result=
                        fixed_local_specular_material_sample_result::
                            fail_sample_count;
                    return out;
                }
                sites[resource]={
                    static_cast<std::uint32_t>(at),
                    words[at+3u],
                    words[at+4u]
                };
            }
        }
        at+=length;
    }

    if (counts[0u]!=1u || counts[1u]!=1u ||
        counts[3u]!=counts[4u] || counts[3u]>1u) {
        out.result=
            fixed_local_specular_material_sample_result::
                fail_sample_count;
        return out;
    }

    out.diffuse_a_t0=sites[0u];
    out.specular_a_t1=sites[1u];
    out.has_blend_b=counts[3u]==1u;
    if (out.has_blend_b) {
        out.diffuse_b_t3=sites[3u];
        out.specular_b_t4=sites[4u];
        out.topology=
            fixed_local_specular_material_topology::
                blended_diffuse_spec;

        std::array<decoded_instruction,k_max_words> instructions{};
        std::size_t instruction_count=0u;
        if(!decode_instructions(
                words,
                word_count,
                instructions,
                instruction_count)) {
            out.result=
                fixed_local_specular_material_sample_result::
                    fail_invalid_dxbc;
            return out;
        }

        const auto spec_a_index=
            instruction_index(
                instructions,
                instruction_count,
                out.specular_a_t1.instruction_word);
        const auto spec_b_index=
            instruction_index(
                instructions,
                instruction_count,
                out.specular_b_t4.instruction_word);
        const auto diff_a_index=
            instruction_index(
                instructions,
                instruction_count,
                out.diffuse_a_t0.instruction_word);
        const auto diff_b_index=
            instruction_index(
                instructions,
                instruction_count,
                out.diffuse_b_t3.instruction_word);

        if(spec_a_index>=instruction_count ||
           spec_b_index>=instruction_count ||
           diff_a_index>=instruction_count ||
           diff_b_index>=instruction_count ||
           spec_b_index!=spec_a_index+1u ||
           diff_b_index!=diff_a_index+1u ||
           spec_b_index+2u>=instruction_count) {
            out.result=
                fixed_local_specular_material_sample_result::
                    fail_sample_order;
            return out;
        }

        const auto spec_add_index=spec_b_index+1u;
        const auto spec_mad_index=spec_b_index+2u;
        std::uint32_t spec_weight_token=0u;
        std::uint32_t spec_weight_register=0u;

        if(!difference_add(
                words,
                instructions[spec_add_index],
                out.specular_a_t1.destination_register,
                out.specular_b_t4.destination_register) ||
           !blend_mad(
                words,
                instructions[spec_mad_index],
                out.specular_a_t1.destination_register,
                out.specular_b_t4.destination_register,
                spec_weight_token,
                spec_weight_register)) {
            out.result=
                fixed_local_specular_material_sample_result::
                    fail_sample_shape;
            return out;
        }

        bool diffuse_lerp_found=false;
        std::uint32_t diffuse_weight_token=0u;
        std::uint32_t diffuse_weight_register=0u;
        std::uint32_t diffuse_mad_word=0u;

        for(std::size_t i=diff_b_index+1u;
            i+1u<instruction_count &&
            i<=diff_b_index+4u;
            ++i) {
            if(!difference_add(
                    words,
                    instructions[i],
                    out.diffuse_a_t0.destination_register,
                    out.diffuse_b_t3.destination_register))
                continue;

            if(!blend_mad(
                    words,
                    instructions[i+1u],
                    out.diffuse_a_t0.destination_register,
                    out.diffuse_b_t3.destination_register,
                    diffuse_weight_token,
                    diffuse_weight_register))
                continue;

            diffuse_lerp_found=true;
            diffuse_mad_word=
                instructions[i+1u].word;
            break;
        }

        if(!diffuse_lerp_found ||
           diffuse_weight_token!=spec_weight_token ||
           diffuse_weight_register!=spec_weight_register) {
            out.result=
                fixed_local_specular_material_sample_result::
                    fail_sample_shape;
            return out;
        }

        out.blend_weight_token=spec_weight_token;
        out.blend_weight_register=spec_weight_register;
        out.specular_blend_mad_word=
            instructions[spec_mad_index].word;
        out.diffuse_blend_mad_word=
            diffuse_mad_word;
    } else {
        out.topology=
            fixed_local_specular_material_topology::
                single_diffuse_spec;
    }

    const auto last_material =
        out.has_blend_b
            ? std::max(
                std::max(out.diffuse_a_t0.instruction_word,
                         out.specular_a_t1.instruction_word),
                std::max(out.diffuse_b_t3.instruction_word,
                         out.specular_b_t4.instruction_word))
            : std::max(
                out.diffuse_a_t0.instruction_word,
                out.specular_a_t1.instruction_word);

    if (last_material>=first_light) {
        out.result=
            fixed_local_specular_material_sample_result::
                fail_sample_order;
        return out;
    }

    out.result=fixed_local_specular_material_sample_result::exact;
    return out;
}

} // namespace dsrrl::operators::point_light
