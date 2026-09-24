#include "dsrrl/core/operator_catalog.hpp"
#include "dsrrl/operators/surface/faceeye_shadow.hpp"
#include "dsrrl/operators/point_light/legacy_specular.hpp"
#include "dsrrl/operators/point_light/runtime_readiness.hpp"

#include <cmath>
#include <iostream>
#include <limits>

using namespace dsrrl;

namespace {

bool check(bool condition,const char *expr,int line)
{
    if(condition) return true;
    std::cerr<<"CHECK FAILED line "<<line<<": "<<expr<<'\n';
    return false;
}
#define CHECK(e) do { if(!check(static_cast<bool>(e),#e,__LINE__)) return 1; } while(false)

core::activation_context verified_activation()
{
    core::activation_context c;
    c.receiver_verified=true;
    c.material_verified=true;
    c.resource_ready=true;
    c.producer_ready=true;
    c.consumer_verified=true;
    c.immediate_context=true;
    c.graph_ready=true;
    return c;
}

operators::surface::faceeye_runtime_context ready_faceeye(
    operators::surface::faceeye_receiver_variant variant)
{
    using namespace operators::surface;
    faceeye_runtime_context c;
    c.receiver_verified=true;
    c.variant=variant;
    c.replacement_ptde_kernel_shader_ready=true;
    c.stock_ptde_kernel_identity_verified=true;
    c.runtime_t7_identity_verified=true;
    c.auxiliary_dirlight_snapshot.immutable_draw_local=true;
    c.auxiliary_dirlight_snapshot.register_present.fill(true);
    c.auxiliary_dirlight_value_homology_verified=true;
    c.csd_matrix_region_ready=true;
    c.stock_regular_s7_verified=true;
    c.regular_s7_sampler_ready=true;
    c.regular_s7_descriptor_verified=true;
    c.sampler_override_transaction_ready=true;
    c.legacy_env_probe_routes_ready=true;
    c.independent_lighting_exclusion_verified=true;
    c.draw_transaction_ready=true;
    return c;
}

operators::point_light::local_specular_runtime_context ready_local_spec(
    operators::point_light::local_specular_receiver_class cls)
{
    using namespace operators::point_light;
    local_specular_runtime_context c;
    c.receiver_verified=true;
    c.receiver_class=cls;
    c.material_c101_c102_verified=true;
    c.spec_rgb_route_ready=true;
    c.source_amplitude_category_ready=true;
    c.attenuation_ready=true;
    c.fixed_membership_verified=true;
    c.clustered_membership_sidecar_ready=true;
    c.clustered_four_slot_shader_ready=true;
    c.stock_cluster_membership_bypassed=true;
    c.complete_microfacet_window_owned=true;
    c.stock_roughness_tail_bypassed=true;
    c.stock_common_ndotl_specular_bypassed=true;
    c.diffuse_path_preserved=true;
    c.separate_specular_output_cut_ready=true;
    c.draw_transaction_ready=true;
    return c;
}

operators::point_light::pointlight_runtime_context ready_pointlight(
    operators::point_light::pointlight_receiver_stratum stratum)
{
    using namespace operators::point_light;
    pointlight_runtime_context c;
    c.receiver_verified=true;
    c.receiver_stratum=stratum;
    c.source_scope_verified=true;
    c.source_scope=pointlight_source_scope::ordinary_map_or_dynamic;
    c.source_instance_state_ready=true;
    c.source_amplitude_category_ready=true;
    c.attenuation_ready=true;
    c.fixed_owner_context_verified=true;
    c.fixed_producer_serial_fresh=true;
    c.fixed_raw_q_t19_sidecar_ready=true;
    c.fixed_selected_light_count=
        pointlight_fixed_expected_light_count(stratum);
    c.fixed_membership_verified=true;
    c.clustered_membership.immutable_draw_local=true;
    c.clustered_membership.ordered_source_identity_ready=true;
    c.clustered_membership.ordered_source_geometry_ready=true;
    c.clustered_membership.ordered_raw_q_ready=true;
    c.clustered_membership.raw_selected_count=4u;
    c.clustered_membership.material_max_pnt_lit_num=4u;
    c.clustered_membership.effective_count=4u;
    c.clustered_four_slot_shader_ready=true;
    c.stock_cluster_membership_bypassed=true;
    c.diffuse_material_path_ready=true;
    c.local_specular_path_ready=true;
    c.terminal_sat_ready=true;
    c.downstream_atmosphere_ready=true;
    c.draw_transaction_ready=true;
    return c;
}

} // namespace

int main()
{
    const auto faceeye_contract=
        core::find_operator_contract(core::operator_id::faceeye_shadow_legacy);
    CHECK(faceeye_contract.has_value());
    CHECK(faceeye_contract->status==core::canonical_status::confirmed);
    CHECK(faceeye_contract->default_state==core::port_state::partial);
    CHECK(faceeye_contract->carrier==core::carrier_kind::hybrid);

    const auto local_spec_contract=
        core::find_operator_contract(core::operator_id::local_specular_legacy);
    CHECK(local_spec_contract.has_value());
    CHECK(local_spec_contract->status==core::canonical_status::confirmed);
    CHECK(local_spec_contract->default_state==core::port_state::partial);

    const auto pointlight_contract=
        core::find_operator_contract(core::operator_id::point_light);
    CHECK(pointlight_contract.has_value());
    CHECK(pointlight_contract->default_state==core::port_state::partial);

    using operators::surface::faceeye_vec3;
    const float packed=
        operators::surface::decode_faceeye_packed_depth({1.0f,0.0f,0.0f});
    CHECK(std::fabs(packed-(255.0f/256.0f))<0.000001f);

    operators::surface::faceeye_shadow_input shadow{};
    shadow.pcf16=0.25f;
    shadow.normal={0.0f,1.0f,0.0f};
    shadow.c175_direction={0.0f,1.0f,0.0f};
    shadow.c121_bias=0.0f;
    shadow.c121_normal_scale=0.5f;
    shadow.c121_fade_start=10.0f;
    shadow.c121_fade_scale=0.1f;
    shadow.view_length=5.0f;
    shadow.c122_shadow_rgb={0.5f,1.0f,0.25f};

    auto shadow_out=
        operators::surface::evaluate_faceeye_shadow_response(shadow);
    CHECK(shadow_out.result==operators::surface::faceeye_shadow_math_result::exact);
    CHECK(std::fabs(shadow_out.normal_term-0.5f)<0.000001f);
    CHECK(std::fabs(shadow_out.shadow_s-0.75f)<0.000001f);
    CHECK(std::fabs(shadow_out.fade-0.5f)<0.000001f);
    CHECK(std::fabs(shadow_out.shadow_rgb.x-0.8125f)<0.000001f);
    CHECK(std::fabs(shadow_out.shadow_rgb.y-0.625f)<0.000001f);
    CHECK(std::fabs(shadow_out.shadow_rgb.z-0.90625f)<0.000001f);

    shadow.view_length=std::numeric_limits<float>::quiet_NaN();
    shadow_out=operators::surface::evaluate_faceeye_shadow_response(shadow);
    CHECK(shadow_out.result==operators::surface::faceeye_shadow_math_result::fail_open_nonfinite_input);

    core::feature_registry features;
    const auto activation=verified_activation();
    CHECK(features.set(core::operator_id::faceeye_shadow_legacy,true));

    auto face=ready_faceeye(operators::surface::faceeye_receiver_variant::sdw_pnts);
    auto face_plan=operators::surface::evaluate_faceeye_runtime_readiness(features,activation,face);
    CHECK(face_plan.ready);
    CHECK(face_plan.keep_live_t7);
    CHECK(face_plan.replace_comparison_kernel);
    CHECK(!face_plan.use_stock_ptde_style_kernel);
    CHECK(face_plan.override_s7_with_regular_sampler);
    CHECK(face_plan.apply_shadow_only_to_envdiffuse_envspec);
    CHECK(face_plan.preserve_upper_lower);
    CHECK(face_plan.preserve_local_pointlight);

    face.auxiliary_dirlight_snapshot.register_present[0]=false;
    face_plan=operators::surface::evaluate_faceeye_runtime_readiness(features,activation,face);
    CHECK(!face_plan.ready);
    CHECK(face_plan.reason==operators::surface::faceeye_runtime_reason::auxiliary_dirlight_snapshot_not_ready);

    face=ready_faceeye(operators::surface::faceeye_receiver_variant::sdw_pnts);
    face.replacement_ptde_kernel_shader_ready=false;
    face_plan=operators::surface::evaluate_faceeye_runtime_readiness(features,activation,face);
    CHECK(!face_plan.ready);
    CHECK(face_plan.reason==operators::surface::faceeye_runtime_reason::replacement_ptde_kernel_shader_not_ready);

    face=ready_faceeye(operators::surface::faceeye_receiver_variant::sdw_pnts);
    face.auxiliary_dirlight_value_homology_verified=false;
    face_plan=operators::surface::evaluate_faceeye_runtime_readiness(features,activation,face);
    CHECK(!face_plan.ready);
    CHECK(face_plan.reason==operators::surface::faceeye_runtime_reason::auxiliary_dirlight_value_homology_not_verified);

    face=ready_faceeye(operators::surface::faceeye_receiver_variant::sdw_pnts);
    face.regular_s7_descriptor_verified=false;
    face_plan=operators::surface::evaluate_faceeye_runtime_readiness(features,activation,face);
    CHECK(!face_plan.ready);
    CHECK(face_plan.reason==operators::surface::faceeye_runtime_reason::regular_s7_descriptor_not_verified);

    face=ready_faceeye(operators::surface::faceeye_receiver_variant::sdw_pntss);
    face.regular_s7_sampler_ready=false;
    face.regular_s7_descriptor_verified=false;
    face.sampler_override_transaction_ready=false;
    face_plan=operators::surface::evaluate_faceeye_runtime_readiness(features,activation,face);
    CHECK(face_plan.ready);
    CHECK(!face_plan.replace_comparison_kernel);
    CHECK(face_plan.use_stock_ptde_style_kernel);
    CHECK(!face_plan.override_s7_with_regular_sampler);

    face.stock_ptde_kernel_identity_verified=false;
    face_plan=operators::surface::evaluate_faceeye_runtime_readiness(features,activation,face);
    CHECK(!face_plan.ready);
    CHECK(face_plan.reason==operators::surface::faceeye_runtime_reason::stock_ptde_kernel_identity_not_verified);

    face=ready_faceeye(operators::surface::faceeye_receiver_variant::sdw_pntss);
    face.stock_regular_s7_verified=false;
    face_plan=operators::surface::evaluate_faceeye_runtime_readiness(features,activation,face);
    CHECK(!face_plan.ready);
    CHECK(face_plan.reason==operators::surface::faceeye_runtime_reason::stock_regular_s7_not_verified);

    face=ready_faceeye(operators::surface::faceeye_receiver_variant::csd_pntssss);
    face.csd_matrix_region_ready=false;
    face_plan=operators::surface::evaluate_faceeye_runtime_readiness(features,activation,face);
    CHECK(!face_plan.ready);
    CHECK(face_plan.reason==operators::surface::faceeye_runtime_reason::csd_matrix_region_not_ready);

    // Legacy scalar seam retained for captured RdotL fixtures.
    operators::point_light::legacy_specular_input spec{};
    spec.source_rgb={1.0f,2.0f,3.0f};
    spec.attenuation=0.5f;
    spec.r_dot_l=0.5f;
    spec.exponent_c102=2.0f;
    spec.spec_texture_blend={0.5f,1.0f,2.0f};
    spec.c101=2.0f;
    spec.color0={1.0f,0.5f,0.25f};

    auto spec_out=operators::point_light::evaluate_legacy_local_specular(spec);
    CHECK(spec_out.result==operators::point_light::legacy_specular_math_result::exact);
    CHECK(std::fabs(spec_out.angular-0.25f)<0.000001f);
    CHECK(std::fabs(spec_out.specular.x-0.125f)<0.000001f);
    CHECK(std::fabs(spec_out.specular.y-0.25f)<0.000001f);
    CHECK(std::fabs(spec_out.specular.z-0.375f)<0.000001f);

    // Complete replacement window owns R=reflect(-V,N) and RdotL itself.
    operators::point_light::legacy_specular_vector_input vector_spec{};
    vector_spec.source_rgb={1.0f,2.0f,3.0f};
    vector_spec.attenuation=0.5f;
    vector_spec.normal={0.0f,1.0f,0.0f};
    vector_spec.view_direction={0.0f,1.0f,0.0f};
    vector_spec.light_direction={0.0f,1.0f,0.0f};
    vector_spec.exponent_c102=2.0f;
    vector_spec.spec_texture_blend={0.5f,1.0f,2.0f};
    vector_spec.c101=2.0f;
    vector_spec.color0={1.0f,0.5f,0.25f};

    spec_out=operators::point_light::evaluate_legacy_local_specular(vector_spec);
    CHECK(spec_out.result==operators::point_light::legacy_specular_math_result::exact);
    CHECK(std::fabs(spec_out.reflection.y-1.0f)<0.000001f);
    CHECK(std::fabs(spec_out.r_dot_l-1.0f)<0.000001f);
    CHECK(std::fabs(spec_out.angular-1.0f)<0.000001f);
    CHECK(std::fabs(spec_out.specular.x-0.5f)<0.000001f);
    CHECK(std::fabs(spec_out.specular.y-1.0f)<0.000001f);
    CHECK(std::fabs(spec_out.specular.z-1.5f)<0.000001f);

    vector_spec.normal={0.0f,2.0f,0.0f};
    spec_out=operators::point_light::evaluate_legacy_local_specular(vector_spec);
    CHECK(spec_out.result==operators::point_light::legacy_specular_math_result::fail_open_invalid_vector);

    spec.exponent_c102=-1.0f;
    spec_out=operators::point_light::evaluate_legacy_local_specular(spec);
    CHECK(spec_out.result==operators::point_light::legacy_specular_math_result::fail_open_invalid_exponent);

    CHECK(features.set(core::operator_id::local_specular_legacy,true));
    auto local=ready_local_spec(operators::point_light::local_specular_receiver_class::fixed_spc_pntss);
    auto local_plan=operators::point_light::evaluate_local_specular_runtime_readiness(features,activation,local);
    CHECK(local_plan.ready);
    CHECK(local_plan.preserve_dsr_diffuse);
    CHECK(local_plan.use_ptde_legacy_specular);
    CHECK(local_plan.bypass_dsr_microfacet);
    CHECK(local_plan.bypass_dsr_roughness_tail);
    CHECK(!local_plan.common_ndotl_on_specular);

    local.complete_microfacet_window_owned=false;
    local_plan=operators::point_light::evaluate_local_specular_runtime_readiness(features,activation,local);
    CHECK(!local_plan.ready);
    CHECK(local_plan.reason==operators::point_light::local_specular_runtime_reason::microfacet_window_not_owned);

    local=ready_local_spec(operators::point_light::local_specular_receiver_class::fixed_spc_pntss);
    local.stock_roughness_tail_bypassed=false;
    local_plan=operators::point_light::evaluate_local_specular_runtime_readiness(features,activation,local);
    CHECK(!local_plan.ready);
    CHECK(local_plan.reason==operators::point_light::local_specular_runtime_reason::roughness_tail_not_bypassed);

    local=ready_local_spec(operators::point_light::local_specular_receiver_class::fixed_spc_pntss);
    local.stock_common_ndotl_specular_bypassed=false;
    local_plan=operators::point_light::evaluate_local_specular_runtime_readiness(features,activation,local);
    CHECK(!local_plan.ready);
    CHECK(local_plan.reason==operators::point_light::local_specular_runtime_reason::common_ndotl_tail_not_bypassed);

    local=ready_local_spec(operators::point_light::local_specular_receiver_class::fixed_spc_pntss);
    local.fixed_membership_verified=false;
    local_plan=operators::point_light::evaluate_local_specular_runtime_readiness(features,activation,local);
    CHECK(!local_plan.ready);
    CHECK(local_plan.reason==operators::point_light::local_specular_runtime_reason::fixed_membership_not_verified);

    local=ready_local_spec(operators::point_light::local_specular_receiver_class::clustered_spc_pnts);
    local.clustered_membership_sidecar_ready=false;
    local_plan=operators::point_light::evaluate_local_specular_runtime_readiness(features,activation,local);
    CHECK(!local_plan.ready);
    CHECK(local_plan.reason==operators::point_light::local_specular_runtime_reason::clustered_membership_sidecar_not_ready);

    CHECK(features.set(core::operator_id::point_light,true));
    auto point=ready_pointlight(operators::point_light::pointlight_receiver_stratum::fixed_nospc_pntss);
    point.local_specular_path_ready=false;
    auto point_plan=operators::point_light::evaluate_pointlight_runtime_readiness(features,activation,point);
    CHECK(point_plan.ready);
    CHECK(point_plan.use_fixed_native_membership);
    CHECK(!point_plan.require_local_specular);

    point=ready_pointlight(operators::point_light::pointlight_receiver_stratum::fixed_nospc_pntss);
    point.fixed_owner_context_verified=false;
    point_plan=operators::point_light::evaluate_pointlight_runtime_readiness(features,activation,point);
    CHECK(!point_plan.ready);
    CHECK(point_plan.reason==operators::point_light::pointlight_runtime_reason::fixed_owner_context_not_verified);

    point=ready_pointlight(operators::point_light::pointlight_receiver_stratum::fixed_nospc_pntss);
    point.fixed_producer_serial_fresh=false;
    point_plan=operators::point_light::evaluate_pointlight_runtime_readiness(features,activation,point);
    CHECK(!point_plan.ready);
    CHECK(point_plan.reason==operators::point_light::pointlight_runtime_reason::fixed_producer_serial_not_fresh);

    point=ready_pointlight(operators::point_light::pointlight_receiver_stratum::fixed_nospc_pntssss);
    point.fixed_selected_light_count=2u;
    point_plan=operators::point_light::evaluate_pointlight_runtime_readiness(features,activation,point);
    CHECK(!point_plan.ready);
    CHECK(point_plan.reason==operators::point_light::pointlight_runtime_reason::fixed_light_count_mismatch);

    point=ready_pointlight(operators::point_light::pointlight_receiver_stratum::fixed_spc_pntssss);
    point.local_specular_path_ready=false;
    point_plan=operators::point_light::evaluate_pointlight_runtime_readiness(features,activation,point);
    CHECK(!point_plan.ready);
    CHECK(point_plan.require_local_specular);
    CHECK(point_plan.reason==operators::point_light::pointlight_runtime_reason::local_specular_path_not_ready);

    point=ready_pointlight(operators::point_light::pointlight_receiver_stratum::clustered_nospc_pnts);
    point.local_specular_path_ready=false;
    point_plan=operators::point_light::evaluate_pointlight_runtime_readiness(features,activation,point);
    CHECK(point_plan.ready);
    CHECK(point_plan.use_cpu_selected_four_sidecar);
    CHECK(point_plan.bypass_stock_cluster_membership);
    CHECK(!point_plan.require_local_specular);
    CHECK(point_plan.selected_light_count==4u);

    point=ready_pointlight(operators::point_light::pointlight_receiver_stratum::clustered_spc_pnts);
    point.local_specular_path_ready=false;
    point_plan=operators::point_light::evaluate_pointlight_runtime_readiness(features,activation,point);
    CHECK(!point_plan.ready);
    CHECK(point_plan.require_local_specular);
    CHECK(point_plan.reason==operators::point_light::pointlight_runtime_reason::local_specular_path_not_ready);

    point=ready_pointlight(operators::point_light::pointlight_receiver_stratum::clustered_spc_pnts);
    point.clustered_membership.effective_count=3u;
    point_plan=operators::point_light::evaluate_pointlight_runtime_readiness(features,activation,point);
    CHECK(!point_plan.ready);
    CHECK(point_plan.reason==operators::point_light::pointlight_runtime_reason::clustered_membership_descriptor_invalid);

    point=ready_pointlight(operators::point_light::pointlight_receiver_stratum::clustered_spc_pnts);
    point_plan=operators::point_light::evaluate_pointlight_runtime_readiness(features,activation,point);
    CHECK(point_plan.ready);
    CHECK(point_plan.require_local_specular);

    point.stock_cluster_membership_bypassed=false;
    point_plan=operators::point_light::evaluate_pointlight_runtime_readiness(features,activation,point);
    CHECK(!point_plan.ready);
    CHECK(point_plan.reason==operators::point_light::pointlight_runtime_reason::stock_cluster_membership_not_bypassed);

    point=ready_pointlight(operators::point_light::pointlight_receiver_stratum::fixed_spc_pntss);
    point.source_scope=operators::point_light::pointlight_source_scope::player_lantern;
    point_plan=operators::point_light::evaluate_pointlight_runtime_readiness(features,activation,point);
    CHECK(!point_plan.ready);
    CHECK(point_plan.reason==operators::point_light::pointlight_runtime_reason::special_source_scope_not_supported);

    std::cout<<"faceeye/pointlight readiness tests passed\n";
    return 0;
}
