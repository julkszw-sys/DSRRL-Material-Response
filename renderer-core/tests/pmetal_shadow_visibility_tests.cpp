#include "dsrrl/operators/surface/pmetal_shadow_visibility.hpp"

#include <cmath>
#include <iostream>

using namespace dsrrl::operators::surface;

namespace {

bool check(bool condition,const char *expr,int line)
{
    if(condition) return true;
    std::cerr<<"CHECK FAILED line "<<line<<": "<<expr<<'\n';
    return false;
}
#define CHECK(e) do { if(!check(static_cast<bool>(e),#e,__LINE__)) return 1; } while(false)

pmetal_shadow_runtime_context ready_context(std::uint32_t shader_index)
{
    pmetal_shadow_runtime_context c;
    c.receiver_verified=true;
    c.material_pmetal_verified=true;
    c.shader_index=shader_index;
    c.replacement_ptde_kernel_shader_ready=true;
    c.stock_ptde_kernel_identity_verified=true;
    c.runtime_t7_identity_verified=true;
    c.runtime_t7_raw_rgb_verified=true;
    c.regular_s7_sampler_ready=true;
    c.regular_s7_descriptor=k_ptde_pmetal_shadow_s7;
    c.stock_regular_s7_verified=true;
    c.sampler_override_transaction_ready=true;
    c.draw_transaction_ready=true;
    return c;
}

} // namespace

int main()
{
    CHECK(!k_ptde_pmetal_shadow_s7.comparison);
    CHECK(k_ptde_pmetal_shadow_s7.min_filter==
          pmetal_shadow_filter::point);
    CHECK(k_ptde_pmetal_shadow_s7.mag_filter==
          pmetal_shadow_filter::point);
    CHECK(k_ptde_pmetal_shadow_s7.mip_filter==
          pmetal_shadow_filter::point);
    CHECK(k_ptde_pmetal_shadow_s7.address_u==
          pmetal_shadow_address_mode::clamp);
    CHECK(k_ptde_pmetal_shadow_s7.address_v==
          pmetal_shadow_address_mode::clamp);
    CHECK(k_ptde_pmetal_shadow_s7.address_w==
          pmetal_shadow_address_mode::clamp);
    CHECK(k_ptde_pmetal_shadow_s7.max_anisotropy==1u);
    CHECK(k_ptde_pmetal_shadow_s7.ptde_d3d9_max_mip_level==0u);

    CHECK(std::fabs(k_pmetal_shadow_packed_depth_weights[0]-
                    (255.0f/256.0f))<0.000001f);
    CHECK(std::fabs(k_pmetal_shadow_pcf_weight-0.0625f)<0.000001f);
    CHECK(std::fabs(k_pmetal_shadow_pcf_offsets.front()+1.5f)<0.000001f);
    CHECK(std::fabs(k_pmetal_shadow_pcf_offsets.back()-1.5f)<0.000001f);
    CHECK(std::fabs(k_pmetal_shadow_texel_scale-(1.0f/2048.0f))<
          0.0000001f);

    auto c=pmetal_shadow_contract_for_shader(894u);
    CHECK(c.variant==pmetal_shadow_receiver_variant::csd_no_point);
    CHECK(c.stock_kernel==pmetal_shadow_kernel_mode::dsr_comparison_pcf9);
    CHECK(c.shadow_operator_present);
    CHECK(c.needs_ptde_kernel_replacement);
    CHECK(!c.stock_regular_s7);

    c=pmetal_shadow_contract_for_shader(913u);
    CHECK(c.variant==pmetal_shadow_receiver_variant::sdw_no_point);
    CHECK(c.needs_ptde_kernel_replacement);

    c=pmetal_shadow_contract_for_shader(904u);
    CHECK(c.variant==pmetal_shadow_receiver_variant::csd_pnts);
    CHECK(c.needs_ptde_kernel_replacement);

    c=pmetal_shadow_contract_for_shader(923u);
    CHECK(c.variant==pmetal_shadow_receiver_variant::sdw_pnts);
    CHECK(c.needs_ptde_kernel_replacement);

    c=pmetal_shadow_contract_for_shader(905u);
    CHECK(c.variant==pmetal_shadow_receiver_variant::csd_pntss);
    CHECK(c.stock_kernel==
          pmetal_shadow_kernel_mode::ptde_manual_packed_pcf16);
    CHECK(!c.needs_ptde_kernel_replacement);
    CHECK(c.stock_regular_s7);

    c=pmetal_shadow_contract_for_shader(924u);
    CHECK(c.variant==pmetal_shadow_receiver_variant::sdw_pntss);
    CHECK(c.stock_regular_s7);

    c=pmetal_shadow_contract_for_shader(906u);
    CHECK(c.variant==pmetal_shadow_receiver_variant::csd_pntssss);
    CHECK(c.stock_regular_s7);

    c=pmetal_shadow_contract_for_shader(925u);
    CHECK(c.variant==pmetal_shadow_receiver_variant::sdw_pntssss);
    CHECK(c.stock_regular_s7);

    c=pmetal_shadow_contract_for_shader(932u);
    CHECK(c.variant==pmetal_shadow_receiver_variant::plain_no_shadow);
    CHECK(!c.shadow_operator_present);
    CHECK(c.stock_kernel==pmetal_shadow_kernel_mode::none);

    c=pmetal_shadow_contract_for_shader(999999u);
    CHECK(c.variant==pmetal_shadow_receiver_variant::unsupported);

    auto ctx=ready_context(894u);
    auto plan=evaluate_pmetal_shadow_runtime_readiness(ctx);
    CHECK(plan.ready);
    CHECK(plan.replace_comparison_kernel);
    CHECK(plan.override_s7_with_ptde_regular_sampler);
    CHECK(plan.keep_live_t7);
    CHECK(!plan.replace_t7_resource);
    CHECK(!plan.preserve_stock_ptde_style_kernel);

    ctx=ready_context(894u);
    ctx.runtime_t7_identity_verified=false;
    plan=evaluate_pmetal_shadow_runtime_readiness(ctx);
    CHECK(!plan.ready);
    CHECK(plan.reason==
          pmetal_shadow_runtime_reason::runtime_t7_identity_not_verified);

    ctx=ready_context(894u);
    ctx.runtime_t7_raw_rgb_verified=false;
    plan=evaluate_pmetal_shadow_runtime_readiness(ctx);
    CHECK(!plan.ready);
    CHECK(plan.reason==
          pmetal_shadow_runtime_reason::runtime_t7_raw_rgb_not_verified);

    ctx=ready_context(894u);
    ctx.regular_s7_descriptor.comparison=true;
    plan=evaluate_pmetal_shadow_runtime_readiness(ctx);
    CHECK(!plan.ready);
    CHECK(plan.reason==
          pmetal_shadow_runtime_reason::regular_s7_descriptor_mismatch);

    ctx=ready_context(894u);
    ctx.regular_s7_descriptor.address_u=
        static_cast<pmetal_shadow_address_mode>(1u);
    plan=evaluate_pmetal_shadow_runtime_readiness(ctx);
    CHECK(!plan.ready);
    CHECK(plan.reason==
          pmetal_shadow_runtime_reason::regular_s7_descriptor_mismatch);

    ctx=ready_context(913u);
    ctx.sampler_override_transaction_ready=false;
    plan=evaluate_pmetal_shadow_runtime_readiness(ctx);
    CHECK(!plan.ready);
    CHECK(plan.reason==
          pmetal_shadow_runtime_reason::
              sampler_override_transaction_not_ready);

    ctx=ready_context(905u);
    ctx.regular_s7_sampler_ready=false;
    ctx.sampler_override_transaction_ready=false;
    plan=evaluate_pmetal_shadow_runtime_readiness(ctx);
    CHECK(plan.ready);
    CHECK(plan.preserve_stock_ptde_style_kernel);
    CHECK(!plan.replace_comparison_kernel);
    CHECK(!plan.override_s7_with_ptde_regular_sampler);

    ctx=ready_context(905u);
    ctx.stock_ptde_kernel_identity_verified=false;
    plan=evaluate_pmetal_shadow_runtime_readiness(ctx);
    CHECK(!plan.ready);
    CHECK(plan.reason==
          pmetal_shadow_runtime_reason::
              stock_ptde_kernel_identity_not_verified);

    ctx=ready_context(924u);
    ctx.stock_regular_s7_verified=false;
    plan=evaluate_pmetal_shadow_runtime_readiness(ctx);
    CHECK(!plan.ready);
    CHECK(plan.reason==
          pmetal_shadow_runtime_reason::stock_regular_s7_not_verified);

    ctx=ready_context(932u);
    plan=evaluate_pmetal_shadow_runtime_readiness(ctx);
    CHECK(!plan.ready);
    CHECK(plan.reason==pmetal_shadow_runtime_reason::no_shadow_operator);

    ctx=ready_context(999999u);
    plan=evaluate_pmetal_shadow_runtime_readiness(ctx);
    CHECK(!plan.ready);
    CHECK(plan.reason==pmetal_shadow_runtime_reason::unsupported_receiver);

    ctx=ready_context(894u);
    ctx.material_pmetal_verified=false;
    plan=evaluate_pmetal_shadow_runtime_readiness(ctx);
    CHECK(!plan.ready);
    CHECK(plan.reason==pmetal_shadow_runtime_reason::material_not_verified);

    return 0;
}
