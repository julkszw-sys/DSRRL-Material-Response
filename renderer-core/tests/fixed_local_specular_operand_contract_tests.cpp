#include "dsrrl/operators/point_light/fixed_local_specular_operand_contract.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

using namespace dsrrl::operators::point_light;

namespace {
bool check(bool condition,const char *expr,int line)
{
    if(condition) return true;
    std::cerr<<"CHECK FAILED line "<<line<<": "<<expr<<'\n';
    return false;
}
#define CHECK(e) do { if(!check(static_cast<bool>(e),#e,__LINE__)) return 1; } while(false)

constexpr std::uint32_t op(
    std::uint32_t opcode,
    std::uint32_t length)
{
    return opcode | (length<<24u);
}

constexpr std::uint32_t temp_xyz()
{
    return 0x00100246u;
}

constexpr std::uint32_t cb_xyz()
{
    return 0x00208246u;
}

void pair(
    std::vector<std::uint32_t> &w,
    std::uint32_t token,
    std::uint32_t index)
{
    w.push_back(token);
    w.push_back(index);
}

void dp3(
    std::vector<std::uint32_t> &w,
    std::uint32_t dst,
    std::uint32_t src0,
    std::uint32_t src1)
{
    w.push_back(op(16u,7u));
    pair(w,temp_xyz(),dst);
    pair(w,temp_xyz(),src0);
    pair(w,temp_xyz(),src1);
}

local_specular_microfacet_window append_window(
    std::vector<std::uint32_t> &w,
    std::uint8_t ordinal,
    std::uint32_t position_cb,
    std::uint32_t color_cb,
    bool corrupt_h=false,
    bool corrupt_color=false)
{
    local_specular_microfacet_window out{};
    out.start_word=static_cast<std::uint32_t>(w.size());
    out.light_ordinal=ordinal;

    w.push_back(op(31u,1u)); // IF

    // Structural witness for cb0[112+i].
    w.push_back(op(0u,4u)); // ADD-shaped opaque test instruction
    w.push_back(cb_xyz());
    w.push_back(0u);
    w.push_back(position_cb);

    const std::uint32_t view=6u;
    const std::uint32_t normal=5u;
    const std::uint32_t half=10u+ordinal;
    const std::uint32_t light=14u+ordinal;

    dp3(w,3u,view,half);
    dp3(w,4u,normal,corrupt_h ? half+20u : half);
    dp3(w,7u,normal,light);

    out.schlick_word=static_cast<std::uint32_t>(w.size());
    w.push_back(op(50u,3u)); // MAD anchor body is already attested by plan
    w.push_back(0xc0b1c059u);
    w.push_back(0xc0df760cu);

    w.push_back(op(23u,1u)); // ENDSWITCH

    // Stock light-color tail witness cb0[116+i].xyz.
    w.push_back(op(56u,4u)); // MUL
    w.push_back(cb_xyz());
    w.push_back(0u);
    w.push_back(corrupt_color ? color_cb+20u : color_cb);

    w.push_back(op(21u,1u)); // ENDIF
    out.end_word_exclusive=static_cast<std::uint32_t>(w.size());
    out.instruction_count=8u;
    return out;
}

fixed_local_specular_patch_plan make_plan(
    std::vector<std::uint32_t> &words,
    std::uint8_t count,
    bool corrupt_h=false,
    bool corrupt_color=false)
{
    fixed_local_specular_patch_plan plan{};
    plan.result=fixed_local_specular_plan_result::ready;
    plan.light_count=count;
    plan.replace_complete_microfacet_window=true;
    plan.use_ptde_legacy_reflect_pow=true;
    plan.consume_g_specular_power_as_exponent=true;
    plan.stock_specular_power_cb_slot=0u;
    plan.stock_specular_power_cb_index=11u;
    plan.stock_specular_power_component=0u;
    plan.ptde_specular_power_cb_slot=12u;
    plan.ptde_specular_power_cb_index=0u;
    plan.ptde_specular_power_component=3u;
    plan.bypass_stock_roughness_tail=true;
    plan.bypass_stock_common_ndotl_specular=true;
    plan.preserve_stock_diffuse=true;

    words={0u,0u};
    for(std::uint8_t i=0u;i<count;++i){
        auto &light=plan.lights[i];
        light.raw_q_t19_index=i;
        light.position_begin_cb=static_cast<std::uint16_t>(112u+i);
        light.color_end_cb=static_cast<std::uint16_t>(116u+i);
        light.microfacet_window=append_window(
            words,i,112u+i,116u+i,
            corrupt_h && i==0u,
            corrupt_color && i==0u);
    }
    words[1]=static_cast<std::uint32_t>(words.size());
    return plan;
}
}

int main()
{
    {
        std::vector<std::uint32_t> words;
        const auto plan=make_plan(words,2u);
        const auto out=
            extract_fixed_local_specular_operand_contract_from_attested_shex_words(
                plan,words.data(),words.size());
        CHECK(out.result==fixed_local_specular_operand_result::ready);
        CHECK(out.light_count==2u);
        CHECK(out.stock_specular_power_cb_slot==0u);
        CHECK(out.stock_specular_power_cb_index==11u);
        CHECK(out.stock_specular_power_component==0u);
        CHECK(out.ptde_specular_power_cb_slot==12u);
        CHECK(out.ptde_specular_power_cb_index==0u);
        CHECK(out.ptde_specular_power_component==3u);
        CHECK(out.exponent_carrier_attested);
        CHECK(out.lights[0].view.token[1]==6u);
        CHECK(out.lights[0].normal.token[1]==5u);
        CHECK(out.lights[0].light.token[1]==14u);
        CHECK(out.lights[1].light.token[1]==15u);
        CHECK(out.lights[0].stock_position_begin_cb==112u);
        CHECK(out.lights[1].stock_position_begin_cb==113u);
        CHECK(out.lights[0].stock_light_color_cb==116u);
        CHECK(out.lights[1].stock_light_color_cb==117u);
    }

    {
        std::vector<std::uint32_t> words;
        const auto plan=make_plan(words,4u);
        const auto out=
            extract_fixed_local_specular_operand_contract_from_attested_shex_words(
                plan,words.data(),words.size());
        CHECK(out.result==fixed_local_specular_operand_result::ready);
        CHECK(out.light_count==4u);
        CHECK(out.lights[3].stock_position_begin_cb==115u);
        CHECK(out.lights[3].stock_light_color_cb==119u);
    }

    {
        std::vector<std::uint32_t> words;
        const auto plan=make_plan(words,2u,true,false);
        const auto out=
            extract_fixed_local_specular_operand_contract_from_attested_shex_words(
                plan,words.data(),words.size());
        CHECK(out.result==
            fixed_local_specular_operand_result::fail_operand_shape);
    }

    {
        std::vector<std::uint32_t> words;
        const auto plan=make_plan(words,2u,false,true);
        const auto out=
            extract_fixed_local_specular_operand_contract_from_attested_shex_words(
                plan,words.data(),words.size());
        CHECK(out.result==
            fixed_local_specular_operand_result::fail_fixed_cb_identity);
    }

    {
        fixed_local_specular_patch_plan plan{};
        plan.result=
            fixed_local_specular_plan_result::
                pass_clustered_membership_not_owned;
        const auto out=
            extract_fixed_local_specular_operand_contract_from_attested_shex_words(
                plan,nullptr,0u);
        CHECK(out.result==
            fixed_local_specular_operand_result::
                pass_not_fixed_local_specular);
    }

    std::cout<<"fixed_local_specular_operand_contract_tests: PASS\n";
    return 0;
}
