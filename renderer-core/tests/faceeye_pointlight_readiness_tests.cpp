#include "dsrrl/core/operator_catalog.hpp"
#include "dsrrl/operators/surface/faceeye_shadow.hpp"
#include "dsrrl/operators/point_light/legacy_specular.hpp"
#include "dsrrl/operators/point_light/runtime_readiness.hpp"
#include <cmath>
#include <iostream>
using namespace dsrrl;
namespace {
bool check(bool c,const char*e,int l){if(c)return true;std::cerr<<"CHECK FAILED line "<<l<<": "<<e<<'\n';return false;}
#define CHECK(e) do{if(!check(static_cast<bool>(e),#e,__LINE__))return 1;}while(false)
core::activation_context activation(){core::activation_context c;c.receiver_verified=c.material_verified=c.resource_ready=c.producer_ready=c.consumer_verified=c.immediate_context=c.graph_ready=true;return c;}
operators::point_light::local_specular_runtime_context local_ready(operators::point_light::local_specular_receiver_class cls){using namespace operators::point_light;local_specular_runtime_context c;c.receiver_verified=true;c.receiver_class=cls;c.material_c101_c102_verified=true;c.spec_rgb_route_ready=true;c.source_amplitude_category_ready=true;c.attenuation_ready=true;c.fixed_membership_verified=true;c.clustered_membership_sidecar_ready=true;c.clustered_four_slot_shader_ready=true;c.stock_cluster_membership_bypassed=true;c.complete_microfacet_window_owned=true;c.stock_roughness_tail_bypassed=true;c.stock_common_ndotl_specular_bypassed=true;c.diffuse_path_preserved=true;c.separate_specular_output_cut_ready=true;c.draw_transaction_ready=true;return c;}
operators::point_light::pointlight_runtime_context point_ready(operators::point_light::pointlight_receiver_stratum s){using namespace operators::point_light;pointlight_runtime_context c;c.receiver_verified=true;c.receiver_stratum=s;c.source_scope_verified=true;c.source_scope=pointlight_source_scope::ordinary_map_or_dynamic;c.source_instance_state_ready=true;c.source_amplitude_category_ready=true;c.attenuation_ready=true;c.fixed_receiver_payload_ready=true;c.fixed_membership_verified=true;c.clustered_cpu_membership_sidecar_ready=true;c.clustered_membership_order_verified=true;c.clustered_count_provenance_ready=true;c.clustered_raw_selected_count=4;c.clustered_material_max_count=4;c.clustered_effective_count=4;c.clustered_four_slot_shader_ready=true;c.stock_cluster_membership_bypassed=true;c.diffuse_material_path_ready=true;c.local_specular_path_ready=true;c.terminal_sat_ready=true;c.downstream_atmosphere_ready=true;c.draw_transaction_ready=true;return c;}
}
int main(){
 const auto ls=core::find_operator_contract(core::operator_id::local_specular_legacy);CHECK(ls.has_value());CHECK(ls->status==core::canonical_status::confirmed);
 core::feature_registry f;const auto a=activation();CHECK(f.set(core::operator_id::local_specular_legacy,true));
 operators::point_light::legacy_specular_vector_input v{};v.source_rgb={1,2,3};v.attenuation=.5f;v.normal={0,1,0};v.view_direction={0,1,0};v.light_direction={0,1,0};v.exponent_c102=2;v.spec_texture_blend={.5f,1,2};v.c101=2;v.color0={1,.5f,.25f};
 auto s=operators::point_light::evaluate_legacy_local_specular(v);CHECK(s.result==operators::point_light::legacy_specular_math_result::exact);CHECK(std::fabs(s.reflection.y-1)<1e-6f);CHECK(std::fabs(s.r_dot_l-1)<1e-6f);CHECK(std::fabs(s.specular.x-.5f)<1e-6f);CHECK(std::fabs(s.specular.y-1)<1e-6f);CHECK(std::fabs(s.specular.z-1.5f)<1e-6f);
 v.normal={0,2,0};s=operators::point_light::evaluate_legacy_local_specular(v);CHECK(s.result==operators::point_light::legacy_specular_math_result::fail_open_invalid_vector);
 auto c=local_ready(operators::point_light::local_specular_receiver_class::fixed_spc_pntss);auto p=operators::point_light::evaluate_local_specular_runtime_readiness(f,a,c);CHECK(p.ready);CHECK(p.bypass_dsr_microfacet);CHECK(p.bypass_dsr_roughness_tail);CHECK(!p.common_ndotl_on_specular);
 c.complete_microfacet_window_owned=false;p=operators::point_light::evaluate_local_specular_runtime_readiness(f,a,c);CHECK(!p.ready);CHECK(p.reason==operators::point_light::local_specular_runtime_reason::microfacet_window_not_owned);
 c=local_ready(operators::point_light::local_specular_receiver_class::fixed_spc_pntss);c.stock_roughness_tail_bypassed=false;p=operators::point_light::evaluate_local_specular_runtime_readiness(f,a,c);CHECK(!p.ready);CHECK(p.reason==operators::point_light::local_specular_runtime_reason::roughness_tail_not_bypassed);
 c=local_ready(operators::point_light::local_specular_receiver_class::fixed_spc_pntss);c.stock_common_ndotl_specular_bypassed=false;p=operators::point_light::evaluate_local_specular_runtime_readiness(f,a,c);CHECK(!p.ready);CHECK(p.reason==operators::point_light::local_specular_runtime_reason::common_ndotl_tail_not_bypassed);
 c=local_ready(operators::point_light::local_specular_receiver_class::clustered_pnts);c.clustered_membership_sidecar_ready=false;p=operators::point_light::evaluate_local_specular_runtime_readiness(f,a,c);CHECK(!p.ready);CHECK(p.reason==operators::point_light::local_specular_runtime_reason::clustered_membership_sidecar_not_ready);
 CHECK(f.set(core::operator_id::point_light,true));auto pc=point_ready(operators::point_light::pointlight_receiver_stratum::clustered_pnts);pc.clustered_material_max_count=2;pc.clustered_effective_count=2;auto pp=operators::point_light::evaluate_pointlight_runtime_readiness(f,a,pc);CHECK(pp.ready);CHECK(pp.selected_light_count==2);pc.stock_cluster_membership_bypassed=false;pp=operators::point_light::evaluate_pointlight_runtime_readiness(f,a,pc);CHECK(!pp.ready);
 std::cout<<"faceeye_pointlight_readiness_tests: PASS\n";return 0;
}
