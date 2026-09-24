#include "dsrrl/operators/lightbank/hemdir3.hpp"
#include <iostream>
using namespace dsrrl;
int main(){
 core::feature_registry f; if(!f.set(core::operator_id::hemdir3,true)) return 1;
 core::activation_context a{}; a.receiver_verified=true;a.material_verified=true;a.resource_ready=true;a.producer_ready=true;a.consumer_verified=true;a.immediate_context=true;a.graph_ready=true;
 operators::lightbank::hemdir3_runtime_context c{}; c.semantic_mode=2;c.upper_lower_source_ready=true;c.d123_source_ready=true;c.b13_carrier_ready=true;c.receiver_verified=true;c.receiver_class=operators::lightbank::hemdir3_receiver_class::spc;c.host_envdiffuse_source_suppressed=true;c.material_continuation_ready=true;c.downstream_material_domain_ready=true;c.downstream_postfog_ready=true;c.atmosphere_route_verified=true;c.draw_transaction_ready=true;
 auto p=operators::lightbank::evaluate_hemdir3_runtime_readiness(f,a,c);
 if(p.ready||p.reason!=operators::lightbank::hemdir3_runtime_reason::material_specular_b12_not_ready) return 2;
 c.material_specular_b12_ready=true;p=operators::lightbank::evaluate_hemdir3_runtime_readiness(f,a,c);
 if(p.ready||p.reason!=operators::lightbank::hemdir3_runtime_reason::directional_specular_continuation_not_ready) return 3;
 c.directional_specular_continuation_ready=true;p=operators::lightbank::evaluate_hemdir3_runtime_readiness(f,a,c);
 if(!p.ready||!p.require_material_specular_b12||!p.require_directional_legacy_specular) return 4;
 c.receiver_class=operators::lightbank::hemdir3_receiver_class::no_spc;c.material_specular_b12_ready=false;c.directional_specular_continuation_ready=false;p=operators::lightbank::evaluate_hemdir3_runtime_readiness(f,a,c);
 if(!p.ready||p.require_material_specular_b12||p.require_directional_legacy_specular) return 5;
 std::cout<<"hemdir3_spc_carrier_tests: PASS\n"; return 0;
}
