#include "dsrrl/core/operator_catalog.hpp"
#include "dsrrl/operators/resource_bridges/subsurface_runtime_readiness.hpp"
#include "dsrrl/operators/resource_bridges/envdiffuse_runtime_readiness.hpp"
#include "dsrrl/operators/lightbank/hemdir3.hpp"
#include "dsrrl/operators/env_spec/legacy_runtime_readiness.hpp"

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
    c.exact_probe_assignment_verified=true;
    c.probe_a_srv_ready=true;
    c.probe_b_srv_ready=true;
    c.sampler_s11_verified=true;
    c.sampler_s13_verified=true;
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
    c.upper_lower_source_ready=true;
    c.d123_source_ready=true;
    c.b13_carrier_ready=true;
    c.receiver_verified=true;
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
    c.receiver_verified=true;
    c.material_verified=true;
    c.ptde_receiver_math_ready=true;
    c.dsr_pbl_bypass_ready=true;
    c.material_response_b12_ready=true;
    c.sampler_descriptor_verified=true;
    c.semantic_sidecar_lookup_ready=true;
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

    envdiffuse=ready_envdiffuse();
    envdiffuse.receiver_family=
        operators::resource_bridges::envdiffuse_receiver_family::heme_env;
    envdiffuse.probe_b_srv_ready=false;
    envdiffuse.sampler_s13_verified=false;
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
