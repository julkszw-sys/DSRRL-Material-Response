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
