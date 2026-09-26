#include "dsrrl/operators/point_light/fixed_local_specular_patch_plan.hpp"

#include <iostream>

using namespace dsrrl::operators::point_light;

namespace {
bool check(bool condition,const char *expr,int line)
{
    if(condition) return true;
    std::cerr<<"CHECK FAILED line "<<line<<": "<<expr<<'\n';
    return false;
}
#define CHECK(e) do { if(!check(static_cast<bool>(e),#e,__LINE__)) return 1; } while(false)

local_specular_microfacet_window_scan exact_scan(std::uint8_t count)
{
    local_specular_microfacet_window_scan scan;
    scan.result=local_specular_window_result::exact;
    scan.window_count=count;
    for(std::uint8_t i=0u;i<count;++i){
        scan.windows[i].start_word=100u+i*100u;
        scan.windows[i].schlick_word=120u+i*100u;
        scan.windows[i].end_word_exclusive=180u+i*100u;
        scan.windows[i].instruction_count=60u;
        scan.windows[i].light_ordinal=i;
    }
    return scan;
}
}

int main()
{
    {
        local_specular_receiver_identity id{};
        id.receiver_class=local_specular_receiver_class::fixed_spc_pntss;
        id.representative_shader_index=729u;
        id.alias_count=2u;
        auto plan=build_fixed_local_specular_patch_plan_from_attested(
            id,exact_scan(2u));
        CHECK(plan.result==fixed_local_specular_plan_result::ready);
        CHECK(plan.light_count==2u);
        CHECK(plan.material_cb_slot==12u);
        CHECK(plan.raw_q_srv_slot==19u);
        CHECK(plan.specular_power_cb_slot==0u);
        CHECK(plan.specular_power_cb_index==11u);
        CHECK(plan.specular_power_component==0u);
        CHECK(plan.lights[0].raw_q_t19_index==0u);
        CHECK(plan.lights[1].raw_q_t19_index==1u);
        CHECK(plan.lights[0].position_begin_cb==112u);
        CHECK(plan.lights[1].position_begin_cb==113u);
        CHECK(plan.lights[0].color_end_cb==116u);
        CHECK(plan.lights[1].color_end_cb==117u);
        CHECK(plan.replace_complete_microfacet_window);
        CHECK(plan.use_ptde_legacy_reflect_pow);
        CHECK(plan.consume_g_specular_power_as_exponent);
        CHECK(plan.bypass_stock_roughness_tail);
        CHECK(plan.bypass_stock_common_ndotl_specular);
        CHECK(plan.preserve_stock_diffuse);
    }

    {
        local_specular_receiver_identity id{};
        id.receiver_class=local_specular_receiver_class::fixed_spc_pntssss;
        auto plan=build_fixed_local_specular_patch_plan_from_attested(
            id,exact_scan(4u));
        CHECK(plan.result==fixed_local_specular_plan_result::ready);
        CHECK(plan.light_count==4u);
        CHECK(plan.lights[3].raw_q_t19_index==3u);
        CHECK(plan.lights[3].position_begin_cb==115u);
        CHECK(plan.lights[3].color_end_cb==119u);
    }

    {
        local_specular_receiver_identity id{};
        id.receiver_class=local_specular_receiver_class::clustered_spc_pnts;
        auto plan=build_fixed_local_specular_patch_plan_from_attested(
            id,exact_scan(1u));
        CHECK(plan.result==
            fixed_local_specular_plan_result::
                pass_clustered_membership_not_owned);
        CHECK(plan.light_count==0u);
        CHECK(!plan.replace_complete_microfacet_window);
        CHECK(!plan.use_ptde_legacy_reflect_pow);
        CHECK(!plan.consume_g_specular_power_as_exponent);
    }

    {
        local_specular_receiver_identity id{};
        id.receiver_class=local_specular_receiver_class::fixed_spc_pntss;
        auto scan=exact_scan(2u);
        scan.result=local_specular_window_result::fail_open_anchor_shape;
        auto plan=build_fixed_local_specular_patch_plan_from_attested(id,scan);
        CHECK(plan.result==
            fixed_local_specular_plan_result::
                fail_microfacet_window_scan);
    }

    {
        local_specular_receiver_identity id{};
        id.receiver_class=local_specular_receiver_class::fixed_spc_pntssss;
        auto plan=build_fixed_local_specular_patch_plan_from_attested(
            id,exact_scan(2u));
        CHECK(plan.result==
            fixed_local_specular_plan_result::fail_fixed_light_count);
    }

    {
        local_specular_receiver_identity id{};
        id.receiver_class=local_specular_receiver_class::fixed_spc_pntss;
        auto scan=exact_scan(2u);
        scan.windows[1].light_ordinal=0u;
        auto plan=build_fixed_local_specular_patch_plan_from_attested(id,scan);
        CHECK(plan.result==
            fixed_local_specular_plan_result::fail_fixed_light_count);
        CHECK(plan.light_count==0u);
    }

    std::cout<<"fixed_local_specular_patch_plan_tests: PASS\n";
    return 0;
}
