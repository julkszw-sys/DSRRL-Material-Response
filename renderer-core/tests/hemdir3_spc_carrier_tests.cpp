#include "dsrrl/operators/lightbank/hemdir3.hpp"
#include <iostream>
using namespace dsrrl;
int main(){
 using namespace operators::lightbank;
 core::feature_registry f; if(!f.set(core::operator_id::hemdir3,true)) return 1;
 core::activation_context a{}; a.receiver_verified=true;a.material_verified=true;a.resource_ready=true;a.producer_ready=true;a.consumer_verified=true;a.immediate_context=true;a.graph_ready=true;
 hemdir3_runtime_context c{}; c.semantic.mode=2;c.upper_lower_source_ready=true;c.d123_source_ready=true;c.b13_carrier_ready=true;c.receiver_verified=true;c.receiver_class=hemdir3_receiver_class::spc;c.host_envdiffuse_source_suppressed=true;c.material_continuation_ready=true;c.downstream_material_domain_ready=true;c.downstream_postfog_ready=true;c.atmosphere_route_verified=true;c.draw_transaction_ready=true;
 auto p=evaluate_hemdir3_runtime_readiness(f,a,c);
 if(p.ready||p.reason!=hemdir3_runtime_reason::semantic_snapshot_not_proven) return 2;
 c.semantic.provenance=hemdir3_semantic_provenance::synthetic_debug_override;c.semantic.immutable_draw_local=true;c.semantic.exact_owner_context=true;c.semantic.lightbank_tuple_fresh=true;c.semantic.effective_mode_observation_verified=true;
 p=evaluate_hemdir3_runtime_readiness(f,a,c); if(p.ready||p.reason!=hemdir3_runtime_reason::semantic_snapshot_not_proven) return 3;
 c.semantic.provenance=hemdir3_semantic_provenance::ordinary_draw_descriptor;c.semantic.effective_mode_observation_verified=false;
 p=evaluate_hemdir3_runtime_readiness(f,a,c); if(p.ready||p.reason!=hemdir3_runtime_reason::semantic_snapshot_not_proven) return 4;
 c.semantic.effective_mode_observation_verified=true;
 p=evaluate_hemdir3_runtime_readiness(f,a,c); if(p.ready||p.reason!=hemdir3_runtime_reason::material_specular_b12_not_ready) return 5;
 c.material_specular_b12_ready=true;p=evaluate_hemdir3_runtime_readiness(f,a,c); if(p.ready||p.reason!=hemdir3_runtime_reason::directional_specular_continuation_not_ready) return 6;
 c.directional_specular_continuation_ready=true;p=evaluate_hemdir3_runtime_readiness(f,a,c); if(!p.ready||!p.require_material_specular_b12||!p.require_directional_legacy_specular) return 7;
 c.receiver_class=hemdir3_receiver_class::no_spc;c.material_specular_b12_ready=false;c.directional_specular_continuation_ready=false;p=evaluate_hemdir3_runtime_readiness(f,a,c); if(!p.ready||p.require_material_specular_b12||p.require_directional_legacy_specular) return 8;
 c.semantic.lightbank_tuple_fresh=false;p=evaluate_hemdir3_runtime_readiness(f,a,c); if(p.ready||p.reason!=hemdir3_runtime_reason::semantic_snapshot_not_proven) return 9;
 std::cout<<"hemdir3_spc_carrier_tests: PASS\n"; return 0;
}
