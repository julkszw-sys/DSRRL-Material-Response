#include "dsrrl/core/operator_catalog.hpp"
#include "dsrrl/operators/resource_bridges/subsurface_runtime_readiness.hpp"
#include "dsrrl/operators/resource_bridges/envdiffuse_runtime_readiness.hpp"
#include "dsrrl/operators/lightbank/hemdir3.hpp"
#include "dsrrl/operators/env_spec/legacy_runtime_readiness.hpp"
#include "dsrrl/operators/env_spec/legacy_resource_bridge.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>

using namespace dsrrl;

namespace {
bool check(bool condition,const char *expr,int line){if(condition)return true;std::cerr<<"CHECK FAILED line "<<line<<": "<<expr<<'\n';return false;}
#define CHECK(e) do { if(!check(static_cast<bool>(e),#e,__LINE__)) return 1; } while(false)
core::activation_context verified_activation(){core::activation_context c;c.receiver_verified=true;c.material_verified=true;c.resource_ready=true;c.producer_ready=true;c.consumer_verified=true;c.immediate_context=true;c.graph_ready=true;return c;}
operators::resource_bridges::envdiffuse_runtime_context ready_envdiffuse(){using namespace operators::resource_bridges;envdiffuse_runtime_context c;c.producer_snapshot_ready=true;c.endpoint_inverse_verified=true;c.alpha_beta_preserved=true;c.draw_multiplier_preserved=true;c.envspec_lanes_preserved=true;c.receiver_verified=true;c.receiver_family=envdiffuse_receiver_family::heme_env_lerp;c.producer_class_verified=true;c.producer_class=envdiffuse_producer_class::ordinary_mapmodel;c.assignment_route=envdiffuse_assignment_route::classic_legacy_environment;c.classic_assignment_homology_verified=true;c.exact_probe_assignment_verified=true;c.probe_a_srv_ready=true;c.probe_b_srv_ready=true;c.sampler_s11_verified=true;c.sampler_s13_verified=true;c.sampler_s11=ptde_envdiffuse_sampler();c.sampler_s13=ptde_envdiffuse_sampler();c.resource_ownership_ready=true;c.draw_restore_ready=true;return c;}
}

int main(){
    using namespace operators::resource_bridges;
    auto activation=verified_activation();core::feature_registry features;CHECK(features.set(core::operator_id::env_diffuse,true));
    auto c=ready_envdiffuse();auto p=evaluate_envdiffuse_runtime_readiness(features,activation,c);CHECK(p.ready);CHECK(!p.consume_packed_selector);

    // Ordinary MapModel is Classic/legacy. A packed selector on this class is
    // not merely unnecessary; accepting it would conflate the ChrModel route.
    c.assignment_route=envdiffuse_assignment_route::packed_gi_selector;c.packed_selector_verified=true;
    p=evaluate_envdiffuse_runtime_readiness(features,activation,c);CHECK(!p.ready);CHECK(p.reason==envdiffuse_runtime_reason::assignment_route_mismatch);

    c=ready_envdiffuse();c.classic_assignment_homology_verified=false;
    p=evaluate_envdiffuse_runtime_readiness(features,activation,c);CHECK(!p.ready);CHECK(p.reason==envdiffuse_runtime_reason::classic_assignment_homology_not_verified);

    // EnemyIns-owned ChrModel is the class for which the +0x7A packed selector
    // producer is known. It must not borrow the MapModel Classic route.
    c=ready_envdiffuse();c.producer_class=envdiffuse_producer_class::enemyins_chrmodel;c.assignment_route=envdiffuse_assignment_route::packed_gi_selector;c.classic_assignment_homology_verified=false;c.packed_selector_verified=true;
    p=evaluate_envdiffuse_runtime_readiness(features,activation,c);CHECK(p.ready);CHECK(p.consume_packed_selector);
    c.packed_selector_verified=false;p=evaluate_envdiffuse_runtime_readiness(features,activation,c);CHECK(!p.ready);CHECK(p.reason==envdiffuse_runtime_reason::packed_selector_not_verified);

    c=ready_envdiffuse();c.producer_class=envdiffuse_producer_class::remo_parts;
    p=evaluate_envdiffuse_runtime_readiness(features,activation,c);CHECK(!p.ready);CHECK(p.reason==envdiffuse_runtime_reason::assignment_route_mismatch);

    c=ready_envdiffuse();c.producer_class_verified=false;
    p=evaluate_envdiffuse_runtime_readiness(features,activation,c);CHECK(!p.ready);CHECK(p.reason==envdiffuse_runtime_reason::producer_class_not_verified);

    c=ready_envdiffuse();c.exact_probe_assignment_verified=false;
    p=evaluate_envdiffuse_runtime_readiness(features,activation,c);CHECK(!p.ready);CHECK(p.reason==envdiffuse_runtime_reason::probe_assignment_not_verified);

    c=ready_envdiffuse();c.sampler_s11.address_u=envdiffuse_address_mode::wrap;
    p=evaluate_envdiffuse_runtime_readiness(features,activation,c);CHECK(!p.ready);CHECK(p.reason==envdiffuse_runtime_reason::sampler_s11_not_verified);

    c=ready_envdiffuse();c.receiver_family=envdiffuse_receiver_family::heme_env;c.probe_b_srv_ready=false;c.sampler_s13_verified=false;c.sampler_s13={};
    p=evaluate_envdiffuse_runtime_readiness(features,activation,c);CHECK(p.ready);

    std::cout<<"future runtime readiness tests passed\n";return 0;
}
