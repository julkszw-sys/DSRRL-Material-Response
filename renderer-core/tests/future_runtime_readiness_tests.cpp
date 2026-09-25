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

operators::resource_bridges::subsurface_runtime_context ready_subsurface()
{
    using namespace operators::resource_bridges;
    subsurface_runtime_context c;
    c.route.actual_material_verified=true;
    c.route.actual_material_name=k_dsr_body_subsurf_material;
    c.route.actual_material_sha256=k_dsr_body_subsurf_material_sha256;
    c.route.actual_receiver_verified=true;
    c.route.actual_receiver_name=k_subsurface_receiver_routes[0].dsr_receiver_name;
    c.route.actual_receiver_sha256=k_subsurface_receiver_routes[0].dsr_receiver_sha256;
    c.route.actual_body_texture_verified=true;
    c.route.body_spec_texture=subsurface_body_texture::bd_f_body_s;
    c.route.stable_hemenv_no_pointlight_draw_verified=true;
    c.route.ptde_slot_mapping_verified=true;
    c.route.ptde_donor_verified=true;
    c.route.ptde_material_name=k_ptde_body_plain_material;
    c.route.ptde_material_sha256=k_ptde_body_plain_material_sha256;
    c.route.ptde_subsurface_usage_verified=true;
    c.route.ptde_uses_subsurface=false;
    c.route.ptde_plain_surface_target_verified=true;
    c.route.target_plain_receiver_ready=true;
    c.route.spec_rgb_route_ready=true;
    c.route.diffuse_route_ready=true;
    c.route.normal_route_ready=true;
    c.route.material_response_route_ready=true;
    c.route.dsr_subsurf_bypass_carrier_ready=true;
    c.replacement_shader_materialized=true;
    c.replacement_shader_identity_verified=true;
    c.draw_transaction_ready=true;
    return c;
}

operators::resource_bridges::envdiffuse_runtime_context ready_envdiffuse()
{
    using namespace operators::resource_bridges;
    envdiffuse_runtime_context c;
    c.producer_snapshot_ready=true;
    c.endpoint_inverse_verified=true;
    c.alpha_beta_preserved=true;
    c.draw_multiplier_preserved=true;
    c.envspec_lanes_preserved=true;
    c.receiver_verified=true;
    c.receiver_family=envdiffuse_receiver_family::heme_env_lerp;
    c.producer_class_verified=true;
    c.producer_class=envdiffuse_producer_class::ordinary_mapmodel;
    c.assignment_route=envdiffuse_assignment_route::classic_legacy_environment;
    c.classic_assignment_homology_verified=true;
    c.exact_probe_assignment_verified=true;
    c.probe_a_srv_ready=true;
    c.probe_b_srv_ready=true;
    c.sampler_s11_verified=true;
    c.sampler_s13_verified=true;
    c.sampler_s11=ptde_envdiffuse_sampler();
    c.sampler_s13=ptde_envdiffuse_sampler();
    c.resource_ownership_ready=true;
    c.draw_restore_ready=true;
    return c;
}

operators::lightbank::hemdir3_runtime_context ready_hemdir3()
{
    operators::lightbank::hemdir3_runtime_context c;
    c.semantic_mode=2;
    c.semantic_mode_provenance=
        operators::lightbank::hemdir3_semantic_mode_provenance::exact_effective_mode2;
    c.producer_snapshot={0x1234u,7u,9u,0x3f000000u};
    c.draw_snapshot=c.producer_snapshot;
    c.upper_lower_source_ready=true;
    c.d123_source_ready=true;
    c.b13_carrier_ready=true;
    c.receiver_verified=true;
    c.receiver_stratum=
        operators::lightbank::hemdir3_receiver_stratum::nospc;
    c.host_envdiffuse_source_suppressed=true;
    c.material_continuation_ready=true;
    c.downstream_material_domain_ready=true;
    c.downstream_postfog_ready=true;
    c.atmosphere_route_verified=true;
    c.draw_transaction_ready=true;
    return c;
}

operators::env_spec::legacy_runtime_context ready_packed_envspec()
{
    using namespace operators::env_spec;
    legacy_runtime_context c;
    c.resource_class=legacy_resource_class::packed_gi;
    c.receiver_family=legacy_receiver_family::hem_env_lerp;
    c.receiver_verified=true;
    c.material_verified=true;
    c.material_semantics_exact=true;
    c.material_envspec_present=true;
    c.material_envspc_slot_verified=true;
    c.material_envspc_slot=2u;
    c.ptde_receiver_math_ready=true;
    c.dsr_pbl_bypass_ready=true;
    c.material_response_b12_ready=true;
    c.sampler_descriptor_verified=true;
    c.semantic_sidecar_lookup_ready=true;
    c.envspc_slot_map_ready=true;
    c.stock_srv_identity_established=true;
    c.probe_a_identity_established=true;
    c.probe_b_identity_established=true;
    c.probe_a_ordinal=17u;
    c.probe_b_ordinal=23u;
    c.packed_gi_resource_ready=true;
    c.packed_gi_stored_alpha_preserved=true;
    c.draw_transaction_ready=true;
    return c;
}

} // namespace

int main()
{
    const auto subsurface_contract=
        core::find_operator_contract(core::operator_id::subsurface);
    CHECK(subsurface_contract.has_value());
    CHECK(subsurface_contract->default_state==core::port_state::active_candidate);

    const auto envdiffuse_contract=
        core::find_operator_contract(core::operator_id::env_diffuse);
    CHECK(envdiffuse_contract.has_value());
    CHECK(envdiffuse_contract->default_state==core::port_state::partial);

    const auto hemdir3_contract=
        core::find_operator_contract(core::operator_id::hemdir3);
    CHECK(hemdir3_contract.has_value());
    CHECK(hemdir3_contract->default_state==core::port_state::partial);

    const auto envspec_contract=
        core::find_operator_contract(core::operator_id::env_spec);
    CHECK(envspec_contract.has_value());
    CHECK(envspec_contract->default_state==core::port_state::partial);

    // Material Response 1.45 carried an exact 368-route EnvSpcSlotNo map and
    // required two consistent fresh stock-SRV observations before a PackedGI
    // identity could participate in a draw. Preserve that authority as a
    // dormant resource-routing contract without enabling the island.
    using namespace operators::env_spec;
    CHECK(k_legacy_envspec_slot_by_route.size()==368u);
    std::array<std::size_t,4> envspec_slot_counts{};
    for(const auto slot:k_legacy_envspec_slot_by_route){
        CHECK(slot<envspec_slot_counts.size());
        ++envspec_slot_counts[slot];
    }
    CHECK(envspec_slot_counts[0]==195u);
    CHECK(envspec_slot_counts[1]==59u);
    CHECK(envspec_slot_counts[2]==62u);
    CHECK(envspec_slot_counts[3]==52u);
    CHECK(legacy_envspec_slot_for_route(0u).value()==1u);
    CHECK(legacy_envspec_slot_for_route(2u).value()==2u);
    CHECK(!legacy_envspec_slot_for_route(368u).has_value());
    CHECK(k_legacy_packed_gi_size==33619968ull);
    CHECK(k_legacy_packed_gi_sha256.front()==0xc1u);
    CHECK(k_legacy_packed_gi_sha256.back()==0xc3u);

    CHECK(generated::k_native_probe_hash_v1.size()==342u);
    std::array<bool,k_legacy_envspec_probe_count> probe_ordinals{};
    for(const auto &record:generated::k_native_probe_hash_v1){
        CHECK(record.probe_ordinal<probe_ordinals.size());
        CHECK(!probe_ordinals[record.probe_ordinal]);
        probe_ordinals[record.probe_ordinal]=true;
    }
    for(const bool seen:probe_ordinals) CHECK(seen);

    const auto first_probe=
        legacy_native_probe_for_sha(
            generated::k_native_probe_hash_v1.front().sha256);
    CHECK(first_probe.has_value());
    CHECK(first_probe.value()==
          generated::k_native_probe_hash_v1.front().probe_ordinal);

    auto bad_probe_sha=generated::k_native_probe_hash_v1.front().sha256;
    bad_probe_sha[0]^=0xffu;
    CHECK(!legacy_native_probe_for_sha(bad_probe_sha).has_value());

    legacy_envspec_identity_tracker envspec_identity;
    legacy_envspec_identity_observation observation;
    observation.canonical_probe_ordinal=7u;
    observation.semantic_a=0x100u;
    observation.semantic_b=0x200u;
    observation.stock_t12=0x1000u;
    observation.stock_t14=0x2000u;
    observation.fresh=true;

    CHECK(envspec_identity.observe(observation)==
          legacy_envspec_identity_observe_result::learning);
    CHECK(envspec_identity.resolve(0x1000u,0x2000u).state==
          legacy_envspec_identity_resolution_state::not_found);
    CHECK(envspec_identity.observe(observation)==
          legacy_envspec_identity_observe_result::established);
    auto identity_resolution=envspec_identity.resolve(0x1000u,0x2000u);
    CHECK(identity_resolution.state==
          legacy_envspec_identity_resolution_state::unique);
    CHECK(identity_resolution.canonical_probe_ordinal==7u);

    observation.stock_t14=0x3000u;
    CHECK(envspec_identity.observe(observation)==
          legacy_envspec_identity_observe_result::refreshing);
    CHECK(envspec_identity.resolve(0x1000u,0x2000u).state==
          legacy_envspec_identity_resolution_state::not_found);
    CHECK(envspec_identity.observe(observation)==
          legacy_envspec_identity_observe_result::established);

    auto same_endpoint=observation;
    same_endpoint.canonical_probe_ordinal=8u;
    same_endpoint.semantic_b=same_endpoint.semantic_a;
    same_endpoint.stock_t14=0x4000u;
    CHECK(envspec_identity.observe(same_endpoint)==
          legacy_envspec_identity_observe_result::invalid);
    same_endpoint.stock_t12=0x4000u;
    CHECK(envspec_identity.observe(same_endpoint)==
          legacy_envspec_identity_observe_result::learning);
    CHECK(envspec_identity.observe(same_endpoint)==
          legacy_envspec_identity_observe_result::established);

    auto duplicate=observation;
    duplicate.canonical_probe_ordinal=9u;
    CHECK(envspec_identity.observe(duplicate)==
          legacy_envspec_identity_observe_result::learning);
    CHECK(envspec_identity.observe(duplicate)==
          legacy_envspec_identity_observe_result::established);
    CHECK(envspec_identity.resolve(observation.stock_t12,observation.stock_t14).state==
          legacy_envspec_identity_resolution_state::ambiguous);

    auto activation=verified_activation();
    core::feature_registry features;

    // Subsurface: future runtime can arm only after the full plain-surface
    // dependency chain, exact replacement identity and transaction are ready.
    CHECK(features.set(core::operator_id::subsurface,true));
    auto subsurface=ready_subsurface();
    auto subsurface_plan=
        operators::resource_bridges::evaluate_subsurface_runtime_readiness(
            features,activation,subsurface);
    CHECK(subsurface_plan.ready);
    CHECK(subsurface_plan.reason==
          operators::resource_bridges::subsurface_runtime_reason::ready);
    CHECK(subsurface_plan.route.target_plain_receiver_id==33u);
    CHECK(subsurface_plan.route.bypass_dsr_subsurf);

    subsurface.route.spec_rgb_route_ready=false;
    subsurface_plan=
        operators::resource_bridges::evaluate_subsurface_runtime_readiness(
            features,activation,subsurface);
    CHECK(!subsurface_plan.ready);
    CHECK(subsurface_plan.reason==
          operators::resource_bridges::subsurface_runtime_reason::route_not_active);

    subsurface=ready_subsurface();
    subsurface.replacement_shader_identity_verified=false;
    subsurface_plan=
        operators::resource_bridges::evaluate_subsurface_runtime_readiness(
            features,activation,subsurface);
    CHECK(!subsurface_plan.ready);
    CHECK(subsurface_plan.reason==
          operators::resource_bridges::subsurface_runtime_reason::
              replacement_shader_identity_mismatch);

    // EnvDiffuse: the split source+probe carrier is eligible only when both
    // endpoint semantics and exact resource/sampler ownership are complete.
    CHECK(features.set(core::operator_id::env_diffuse,true));
    auto envdiffuse=ready_envdiffuse();
    auto envdiffuse_plan=
        operators::resource_bridges::evaluate_envdiffuse_runtime_readiness(
            features,activation,envdiffuse);
    CHECK(envdiffuse_plan.ready);
    CHECK(envdiffuse_plan.probe_a_srv_slot==11u);
    CHECK(envdiffuse_plan.probe_b_srv_slot==13u);
    CHECK(envdiffuse_plan.bridge_endpoint_xyz_only);
    CHECK(envdiffuse_plan.preserve_draw_multiplier);
    CHECK(!envdiffuse_plan.consume_packed_selector);

    // Producer provenance is class-scoped. Ordinary MapModel is the
    // Classic/legacy environment route and must not borrow the ChrModel
    // packed-GI selector.
    envdiffuse=ready_envdiffuse();
    envdiffuse.assignment_route=
        operators::resource_bridges::envdiffuse_assignment_route::
            packed_gi_selector;
    envdiffuse.packed_selector_verified=true;
    envdiffuse_plan=
        operators::resource_bridges::evaluate_envdiffuse_runtime_readiness(
            features,activation,envdiffuse);
    CHECK(!envdiffuse_plan.ready);
    CHECK(envdiffuse_plan.reason==
          operators::resource_bridges::envdiffuse_runtime_reason::
              assignment_route_mismatch);

    envdiffuse=ready_envdiffuse();
    envdiffuse.classic_assignment_homology_verified=false;
    envdiffuse_plan=
        operators::resource_bridges::evaluate_envdiffuse_runtime_readiness(
            features,activation,envdiffuse);
    CHECK(!envdiffuse_plan.ready);
    CHECK(envdiffuse_plan.reason==
          operators::resource_bridges::envdiffuse_runtime_reason::
              classic_assignment_homology_not_verified);

    // EnemyIns/ChrModel is the verified owner of the packed selector route.
    envdiffuse=ready_envdiffuse();
    envdiffuse.producer_class=
        operators::resource_bridges::envdiffuse_producer_class::
            enemyins_chrmodel;
    envdiffuse.assignment_route=
        operators::resource_bridges::envdiffuse_assignment_route::
            packed_gi_selector;
    envdiffuse.classic_assignment_homology_verified=false;
    envdiffuse.packed_selector_verified=true;
    envdiffuse_plan=
        operators::resource_bridges::evaluate_envdiffuse_runtime_readiness(
            features,activation,envdiffuse);
    CHECK(envdiffuse_plan.ready);
    CHECK(envdiffuse_plan.consume_packed_selector);

    envdiffuse.packed_selector_verified=false;
    envdiffuse_plan=
        operators::resource_bridges::evaluate_envdiffuse_runtime_readiness(
            features,activation,envdiffuse);
    CHECK(!envdiffuse_plan.ready);
    CHECK(envdiffuse_plan.reason==
          operators::resource_bridges::envdiffuse_runtime_reason::
              packed_selector_not_verified);

    // REMO is intentionally unresolved and may not inherit gameplay routing.
    envdiffuse=ready_envdiffuse();
    envdiffuse.producer_class=
        operators::resource_bridges::envdiffuse_producer_class::remo_parts;
    envdiffuse_plan=
        operators::resource_bridges::evaluate_envdiffuse_runtime_readiness(
            features,activation,envdiffuse);
    CHECK(!envdiffuse_plan.ready);
    CHECK(envdiffuse_plan.reason==
          operators::resource_bridges::envdiffuse_runtime_reason::
              assignment_route_mismatch);

    envdiffuse=ready_envdiffuse();
    envdiffuse.producer_class_verified=false;
    envdiffuse_plan=
        operators::resource_bridges::evaluate_envdiffuse_runtime_readiness(
            features,activation,envdiffuse);
    CHECK(!envdiffuse_plan.ready);
    CHECK(envdiffuse_plan.reason==
          operators::resource_bridges::envdiffuse_runtime_reason::
              producer_class_not_verified);

    envdiffuse=ready_envdiffuse();
    envdiffuse.exact_probe_assignment_verified=false;
    envdiffuse_plan=
        operators::resource_bridges::evaluate_envdiffuse_runtime_readiness(
            features,activation,envdiffuse);
    CHECK(!envdiffuse_plan.ready);
    CHECK(envdiffuse_plan.reason==
          operators::resource_bridges::envdiffuse_runtime_reason::
              probe_assignment_not_verified);

    envdiffuse=ready_envdiffuse();
    envdiffuse.sampler_s13_verified=false;
    envdiffuse_plan=
        operators::resource_bridges::evaluate_envdiffuse_runtime_readiness(
            features,activation,envdiffuse);
    CHECK(!envdiffuse_plan.ready);
    CHECK(envdiffuse_plan.reason==
          operators::resource_bridges::envdiffuse_runtime_reason::
              sampler_s13_not_verified);

    // A boolean identity assertion is insufficient: the observed descriptor
    // itself must match the confirmed PTDE ordinary environment preset.
    envdiffuse=ready_envdiffuse();
    envdiffuse.sampler_s11.address_u=
        operators::resource_bridges::envdiffuse_address_mode::wrap;
    envdiffuse_plan=
        operators::resource_bridges::evaluate_envdiffuse_runtime_readiness(
            features,activation,envdiffuse);
    CHECK(!envdiffuse_plan.ready);
    CHECK(envdiffuse_plan.reason==
          operators::resource_bridges::envdiffuse_runtime_reason::
              sampler_s11_not_verified);

    envdiffuse=ready_envdiffuse();
    envdiffuse.sampler_s13.max_anisotropy=2u;
    envdiffuse_plan=
        operators::resource_bridges::evaluate_envdiffuse_runtime_readiness(
            features,activation,envdiffuse);
    CHECK(!envdiffuse_plan.ready);
    CHECK(envdiffuse_plan.reason==
          operators::resource_bridges::envdiffuse_runtime_reason::
              sampler_s13_not_verified);

    envdiffuse=ready_envdiffuse();
    envdiffuse.receiver_family=
        operators::resource_bridges::envdiffuse_receiver_family::heme_env;
    envdiffuse.probe_b_srv_ready=false;
    envdiffuse.sampler_s13_verified=false;
    envdiffuse.sampler_s13={};
    envdiffuse_plan=
        operators::resource_bridges::evaluate_envdiffuse_runtime_readiness(
            features,activation,envdiffuse);
    CHECK(envdiffuse_plan.ready);

    // HemDir3 D123 producer: PTDE interpolates direction angles on the
    // shortest arc, not Cartesian vectors, while decoded colors lerp linearly.
    std::array<operators::lightbank::hemdir3_raw_lobe_endpoint,3> d123_a{};
    std::array<operators::lightbank::hemdir3_raw_lobe_endpoint,3> d123_b{};
    d123_a[0].direction={170.0f,0.0f};
    d123_b[0].direction={-170.0f,0.0f};
    d123_a[0].color={{255.0f,0.0f,0.0f},100.0f};
    d123_b[0].color={{0.0f,0.0f,255.0f},100.0f};
    auto d123=operators::lightbank::evaluate_hemdir3_profile(
        d123_a,d123_b,0.5f);
    CHECK(d123.result==
          operators::lightbank::hemdir3_profile_result::exact);
    CHECK(std::fabs(d123.lobes[0].direction.y)<0.000001f);
    CHECK(d123.lobes[0].direction.z < -0.9999f);
    CHECK(std::fabs(d123.lobes[0].color.x-0.5f)<0.000001f);
    CHECK(std::fabs(d123.lobes[0].color.z-0.5f)<0.000001f);

    d123=operators::lightbank::evaluate_hemdir3_profile(
        d123_a,d123_b,0.0f);
    CHECK(d123.result==
          operators::lightbank::hemdir3_profile_result::exact);
    CHECK(d123.lobes[0].color.x==1.0f);
    CHECK(d123.lobes[0].color.z==0.0f);

    d123=operators::lightbank::evaluate_hemdir3_profile(
        d123_a,d123_b,1.0f);
    CHECK(d123.lobes[0].color.x==0.0f);
    CHECK(d123.lobes[0].color.z==1.0f);

    auto bad_d123=d123_a;
    bad_d123[1].direction.x_degrees=
        std::numeric_limits<float>::quiet_NaN();
    d123=operators::lightbank::evaluate_hemdir3_profile(
        bad_d123,d123_b,0.5f);
    CHECK(d123.result==
          operators::lightbank::hemdir3_profile_result::
              fail_open_nonfinite_input);

    // HemDir3 local source algebra is exact and remains linear.
    using operators::lightbank::hemdir3_lobe;
    using operators::lightbank::hemdir3_vec3;
    const hemdir3_vec3 H{0.1f,0.2f,0.3f};
    const hemdir3_vec3 N{0.0f,1.0f,0.0f};
    const std::array<hemdir3_lobe,3> lobes{{
        {{0.0f,-1.0f,0.0f},{1.0f,0.0f,0.0f}},
        {{0.0f, 1.0f,0.0f},{0.0f,1.0f,0.0f}},
        {{1.0f, 0.0f,0.0f},{0.0f,0.0f,1.0f}}
    }};
    auto h=
        operators::lightbank::evaluate_hemdir3_source_join(H,N,lobes);
    CHECK(h.result==
          operators::lightbank::hemdir3_math_result::exact);
    CHECK(std::fabs(h.weights[0]-1.0f)<0.000001f);
    CHECK(h.weights[1]==0.0f);
    CHECK(h.weights[2]==0.0f);
    CHECK(std::fabs(h.joined_source.x-1.1f)<0.000001f);
    CHECK(std::fabs(h.joined_source.y-0.2f)<0.000001f);
    CHECK(std::fabs(h.joined_source.z-0.3f)<0.000001f);

    auto badN=N;
    badN.x=std::numeric_limits<float>::quiet_NaN();
    h=operators::lightbank::evaluate_hemdir3_source_join(H,badN,lobes);
    CHECK(h.result==
          operators::lightbank::hemdir3_math_result::
              fail_open_nonfinite_input);

    CHECK(features.set(core::operator_id::hemdir3,true));
    auto hemdir3=ready_hemdir3();
    auto hemdir3_plan=
        operators::lightbank::evaluate_hemdir3_runtime_readiness(
            features,activation,hemdir3);
    CHECK(hemdir3_plan.ready);
    CHECK(hemdir3_plan.suppress_host_envdiffuse);
    CHECK(hemdir3_plan.use_ptde_linear_d123);
    CHECK(!hemdir3_plan.apply_source_gamma_compensation);
    CHECK(!hemdir3_plan.require_b12_material_donor);
    CHECK(!hemdir3_plan.require_directional_legacy_specular);

    hemdir3=ready_hemdir3();
    hemdir3.receiver_stratum=
        operators::lightbank::hemdir3_receiver_stratum::spc;
    hemdir3_plan=
        operators::lightbank::evaluate_hemdir3_runtime_readiness(
            features,activation,hemdir3);
    CHECK(!hemdir3_plan.ready);
    CHECK(hemdir3_plan.require_b12_material_donor);
    CHECK(hemdir3_plan.require_directional_legacy_specular);
    CHECK(hemdir3_plan.reason==
          operators::lightbank::hemdir3_runtime_reason::
              spc_b12_material_donor_not_ready);

    hemdir3.spc_b12_material_donor_ready=true;
    hemdir3_plan=
        operators::lightbank::evaluate_hemdir3_runtime_readiness(
            features,activation,hemdir3);
    CHECK(!hemdir3_plan.ready);
    CHECK(hemdir3_plan.reason==
          operators::lightbank::hemdir3_runtime_reason::
              directional_specular_continuation_not_ready);

    hemdir3.directional_specular_continuation_ready=true;
    hemdir3_plan=
        operators::lightbank::evaluate_hemdir3_runtime_readiness(
            features,activation,hemdir3);
    CHECK(hemdir3_plan.ready);

    // Ordinary/direct selector state cannot authorize HemDir3. Cross-render
    // RE establishes ordinary direct producer values only in {0,1}; even a
    // synthetic mode2 value must fail open without exact effective provenance.
    hemdir3=ready_hemdir3();
    hemdir3.semantic_mode_provenance=
        operators::lightbank::hemdir3_semantic_mode_provenance::ordinary_direct;
    hemdir3_plan=
        operators::lightbank::evaluate_hemdir3_runtime_readiness(
            features,activation,hemdir3);
    CHECK(!hemdir3_plan.ready);
    CHECK(hemdir3_plan.reason==
          operators::lightbank::hemdir3_runtime_reason::
              semantic_mode_provenance_not_verified);

    hemdir3=ready_hemdir3();
    hemdir3.semantic_mode_provenance=
        operators::lightbank::hemdir3_semantic_mode_provenance::unknown;
    hemdir3_plan=
        operators::lightbank::evaluate_hemdir3_runtime_readiness(
            features,activation,hemdir3);
    CHECK(!hemdir3_plan.ready);
    CHECK(hemdir3_plan.reason==
          operators::lightbank::hemdir3_runtime_reason::
              semantic_mode_provenance_not_verified);

    // Snapshot identity and freshness are separate from semantic-mode proof.
    hemdir3=ready_hemdir3();
    hemdir3.draw_snapshot.owner=0x5678u;
    hemdir3_plan=
        operators::lightbank::evaluate_hemdir3_runtime_readiness(
            features,activation,hemdir3);
    CHECK(!hemdir3_plan.ready);
    CHECK(hemdir3_plan.reason==
          operators::lightbank::hemdir3_runtime_reason::
              snapshot_owner_not_verified);

    hemdir3=ready_hemdir3();
    ++hemdir3.draw_snapshot.selector_b;
    hemdir3_plan=
        operators::lightbank::evaluate_hemdir3_runtime_readiness(
            features,activation,hemdir3);
    CHECK(!hemdir3_plan.ready);
    CHECK(hemdir3_plan.reason==
          operators::lightbank::hemdir3_runtime_reason::
              snapshot_assignment_tuple_not_fresh);

    hemdir3=ready_hemdir3();
    hemdir3.host_envdiffuse_source_suppressed=false;
    hemdir3_plan=
        operators::lightbank::evaluate_hemdir3_runtime_readiness(
            features,activation,hemdir3);
    CHECK(!hemdir3_plan.ready);
    CHECK(hemdir3_plan.reason==
          operators::lightbank::hemdir3_runtime_reason::
              host_envdiffuse_not_suppressed);

    hemdir3=ready_hemdir3();
    hemdir3.downstream_postfog_ready=false;
    hemdir3_plan=
        operators::lightbank::evaluate_hemdir3_runtime_readiness(
            features,activation,hemdir3);
    CHECK(!hemdir3_plan.ready);
    CHECK(hemdir3_plan.reason==
          operators::lightbank::hemdir3_runtime_reason::
              downstream_postfog_not_ready);

    // Legacy EnvSpec is split: packed GI may be armed independently; Classic
    // remains fail-open until authored same-world selector values are proven.
    CHECK(features.set(core::operator_id::env_spec,true));
    auto envspec=ready_packed_envspec();
    auto envspec_plan=
        operators::env_spec::evaluate_legacy_runtime_readiness(
            features,activation,envspec);
    CHECK(envspec_plan.ready);
    CHECK(envspec_plan.bypass_dsr_pbl_tail);
    CHECK(envspec_plan.preserve_ptde_sample_alpha);
    CHECK(envspec_plan.probe_b_required);
    CHECK(envspec_plan.envspc_slot==2u);
    CHECK(envspec_plan.probe_a_ordinal==17u);
    CHECK(envspec_plan.probe_b_ordinal==23u);

    // Pure HemEnv is a one-endpoint consumer: exact B identity is not a
    // readiness requirement. HemEnvLerp remains two-endpoint exact.
    envspec=ready_packed_envspec();
    envspec.receiver_family=
        operators::env_spec::legacy_receiver_family::hem_env;
    envspec.probe_b_identity_established=false;
    envspec.probe_b_ordinal=0u;
    envspec_plan=
        operators::env_spec::evaluate_legacy_runtime_readiness(
            features,activation,envspec);
    CHECK(envspec_plan.ready);
    CHECK(!envspec_plan.probe_b_required);

    envspec=ready_packed_envspec();
    envspec.material_semantics_exact=false;
    envspec_plan=
        operators::env_spec::evaluate_legacy_runtime_readiness(
            features,activation,envspec);
    CHECK(!envspec_plan.ready);
    CHECK(envspec_plan.reason==
          operators::env_spec::legacy_runtime_reason::
              material_semantics_not_exact);

    envspec=ready_packed_envspec();
    envspec.material_envspec_present=false;
    envspec_plan=
        operators::env_spec::evaluate_legacy_runtime_readiness(
            features,activation,envspec);
    CHECK(!envspec_plan.ready);
    CHECK(envspec_plan.reason==
          operators::env_spec::legacy_runtime_reason::
              material_envspec_not_present);

    envspec=ready_packed_envspec();
    envspec.material_envspc_slot_verified=false;
    envspec_plan=
        operators::env_spec::evaluate_legacy_runtime_readiness(
            features,activation,envspec);
    CHECK(!envspec_plan.ready);
    CHECK(envspec_plan.reason==
          operators::env_spec::legacy_runtime_reason::
              material_envspc_slot_not_verified);

    envspec=ready_packed_envspec();
    envspec.envspc_slot_map_ready=false;
    envspec_plan=
        operators::env_spec::evaluate_legacy_runtime_readiness(
            features,activation,envspec);
    CHECK(!envspec_plan.ready);
    CHECK(envspec_plan.reason==
          operators::env_spec::legacy_runtime_reason::
              envspc_slot_map_not_ready);

    envspec=ready_packed_envspec();
    envspec.stock_srv_identity_established=false;
    envspec_plan=
        operators::env_spec::evaluate_legacy_runtime_readiness(
            features,activation,envspec);
    CHECK(!envspec_plan.ready);
    CHECK(envspec_plan.reason==
          operators::env_spec::legacy_runtime_reason::
              stock_srv_identity_not_established);

    envspec=ready_packed_envspec();
    envspec.probe_a_identity_established=false;
    envspec_plan=
        operators::env_spec::evaluate_legacy_runtime_readiness(
            features,activation,envspec);
    CHECK(!envspec_plan.ready);
    CHECK(envspec_plan.reason==
          operators::env_spec::legacy_runtime_reason::
              probe_a_identity_not_established);

    envspec=ready_packed_envspec();
    envspec.probe_b_identity_established=false;
    envspec_plan=
        operators::env_spec::evaluate_legacy_runtime_readiness(
            features,activation,envspec);
    CHECK(!envspec_plan.ready);
    CHECK(envspec_plan.reason==
          operators::env_spec::legacy_runtime_reason::
              probe_b_identity_not_established);

    envspec=ready_packed_envspec();
    envspec.resource_class=
        operators::env_spec::legacy_resource_class::classic_rgb24;
    envspec.packed_gi_resource_ready=false;
    envspec.packed_gi_stored_alpha_preserved=false;
    envspec.classic_resource_ready=true;
    envspec.classic_api_alpha_one=true;
    envspec.classic_selector_algorithm_verified=true;
    envspec.classic_authored_selector_values_verified=false;
    envspec_plan=
        operators::env_spec::evaluate_legacy_runtime_readiness(
            features,activation,envspec);
    CHECK(!envspec_plan.ready);
    CHECK(envspec_plan.reason==
          operators::env_spec::legacy_runtime_reason::
              classic_authored_selector_values_not_verified);

    envspec.classic_authored_selector_values_verified=true;
    envspec_plan=
        operators::env_spec::evaluate_legacy_runtime_readiness(
            features,activation,envspec);
    CHECK(envspec_plan.ready);

    // Feature OFF remains a hard no-op for every future-runtime candidate.
    core::feature_registry disabled;
    subsurface=ready_subsurface();
    subsurface_plan=
        operators::resource_bridges::evaluate_subsurface_runtime_readiness(
            disabled,activation,subsurface);
    CHECK(!subsurface_plan.ready);
    CHECK(subsurface_plan.reason==
          operators::resource_bridges::subsurface_runtime_reason::
              core_gate_not_active);

    std::cout<<"future_runtime_readiness_tests: PASS\n";
    return 0;
}
