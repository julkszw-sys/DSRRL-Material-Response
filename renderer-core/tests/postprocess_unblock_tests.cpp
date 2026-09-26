#include "dsrrl/core/operator_catalog.hpp"
#include "dsrrl/core/island_policy.hpp"
#include "dsrrl/operators/postprocess/future_runtime_preflight.hpp"
#include "dsrrl/operators/postprocess/bloom_scene_bridge.hpp"
#include "dsrrl/operators/postprocess/bloom_scene_static_authority.hpp"
#include "dsrrl/operators/postprocess/bloom_legacy_graph.hpp"
#include "dsrrl/operators/postprocess/waterwave_authored_identity.hpp"
#include "dsrrl/runtime/bloom_fx_draw_transport.hpp"

#include <iostream>

using namespace dsrrl;

namespace {

bool check(bool condition,const char *expr,int line)
{
    if(condition) return true;
    std::cerr<<"CHECK FAILED line "<<line<<": "<<expr<<'\n';
    return false;
}
#define CHECK(e) do { if(!check(static_cast<bool>(e),#e,__LINE__)) return 1; } while(false)

operators::postprocess::bloom_unblock_context ready_bloom()
{
    operators::postprocess::bloom_unblock_context c;
    c.scene_domain_bridge_ready=true;
    c.q8_scene_source_ready=true;
    c.packed_depth_logical_bridge_ready=true;
    c.packed_depth_quantization_closed=true;
    c.type06_host_verified=true;
    c.type06_shader_identity_verified=true;
    c.fixed_rgba_ladder_ready=true;
    c.pass04_ready=true;
    c.pass18_ready=true;
    c.pass19_1a_ready=true;
    c.six_pass_graph_insertion_ready=true;
    c.synchronization_ready=true;
    c.resource_ownership_ready=true;
    c.resource_lifetime_ready=true;
    c.hdr_t1_handoff_verified=true;
    c.lightshaft_separation_preserved=true;
    c.stock_sfx_graph_preserved=true;
    return c;
}

operators::postprocess::bloom_legacy_graph_carrier ready_bloom_graph()
{
    using namespace operators::postprocess;
    bloom_legacy_graph_carrier c;
    c.q8_scene_1024x720_ready=true;
    c.packed_depth_256x180_ready=true;
    c.fixed_rgba_256x180_ready=true;
    c.fixed_rgba_128x90_ready=true;
    c.first_pass04=bloom_pass04_role::packed_depth_first;
    c.second_pass04=bloom_pass04_role::pre_brightpass_color_copy_second;
    c.second_pass04_source_is_q8_scene=true;
    c.second_pass04_destination_is_rgba_256x180=true;
    c.pass18_brightpass_ready=true;
    c.pass19_blur_h_ready=true;
    c.pass1a_blur_v_ready=true;
    c.hdr_t1_consumes_rgba_128x90=true;
    return c;
}

operators::postprocess::hdr_unblock_context ready_hdr()
{
    operators::postprocess::hdr_unblock_context c;
    c.scene_domain_bridge_ready=true;
    c.q8_scene_source_ready=true;
    c.legacy_scene_scale_lane_ready=true;
    c.lightshaft_lane_ready=true;
    c.dsr_c56w_preserved=true;
    c.bloom_input_semantics_ready=true;
    c.lightshaft_input_semantics_ready=true;
    c.legacy_hdr_transfer_ready=true;
    c.coloradjust_overlay_tail_ready=true;
    c.graph_insertion_ready=true;
    c.synchronization_ready=true;
    c.resource_ownership_ready=true;
    c.resource_lifetime_ready=true;
    c.output_handoff_verified=true;
    c.sfx_composite_order_verified=true;
    c.stock_dsr_sfx_preserved=true;
    c.sfx_inverse_tonemap_contract_preserved=true;
    return c;
}

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

} // namespace

int main()
{
    using namespace operators::postprocess;

    // A6 deliberately does NOT promote Bloom/HDR out of BLOCKED.
    const auto bloom_contract=
        core::find_operator_contract(core::operator_id::post_bloom);
    CHECK(bloom_contract.has_value());
    CHECK(bloom_contract->status==core::canonical_status::confirmed);
    CHECK(bloom_contract->default_state==core::port_state::blocked);
    CHECK(bloom_contract->carrier==core::carrier_kind::composite);

    const auto hdr_contract=
        core::find_operator_contract(core::operator_id::post_hdr);
    CHECK(hdr_contract.has_value());
    CHECK(hdr_contract->status==core::canonical_status::confirmed);
    CHECK(hdr_contract->default_state==core::port_state::blocked);
    CHECK(hdr_contract->carrier==core::carrier_kind::composite);

    // Even with the generic feature and Core graph/resource gates set, blocked
    // catalog state remains an absolute runtime fail-open.
    core::feature_registry features;
    CHECK(features.set(core::operator_id::post_bloom,true));
    CHECK(features.set(core::operator_id::post_hdr,true));
    const auto activation=verified_activation();

    auto gate=core::evaluate_operator_activation(
        features,core::operator_id::post_bloom,activation);
    CHECK(gate.state==core::island_state::fail_open);
    CHECK(gate.reason==core::activation_reason::blocked);

    gate=core::evaluate_operator_activation(
        features,core::operator_id::post_hdr,activation);
    CHECK(gate.state==core::island_state::fail_open);
    CHECK(gate.reason==core::activation_reason::blocked);

    // WaterWaveSfx authored identity is exact across PTDE/DSR, but authored
    // identity is not a live draw authority. The same runtime material/entity
    // instance must still reach the authenticated FX collector draw.
    waterwave_authored_identity waterwave{};
    waterwave.raw_mtd_exact=true;
    waterwave.spx_semantic_id=k_waterwave_spx_semantic_id;
    waterwave.blend_mode=k_waterwave_blend_mode;
    waterwave.is_waterwave_sfx=true;
    CHECK(validate_waterwave_authored_identity(waterwave)==
          waterwave_authored_identity_result::exact_authored_identity);
    CHECK(!waterwave_authored_identity_is_draw_authority());

    waterwave.blend_mode=1u;
    CHECK(validate_waterwave_authored_identity(waterwave)==
          waterwave_authored_identity_result::blend_mode_not_exact);

    // Live authority is a stricter stage than authored identity. Exact
    // WaterWave identity must be attached to the very same live Particle model
    // instance that the authenticated collector draw resolves through.
    runtime::bloom_fx_draw_transport::fx_draw_snapshot fxdraw{};
    CHECK(runtime::bloom_fx_draw_transport::validate_waterwave_draw_authority(fxdraw)==
          runtime::bloom_fx_draw_transport::waterwave_draw_authority_result::
              draw_snapshot_not_ready);

    fxdraw.ready=true;
    fxdraw.exact_entity_vtable=true;
    fxdraw.exact_appearance_vtable=true;
    fxdraw.draw_context=reinterpret_cast<void *>(0x1);
    fxdraw.kind=runtime::bloom_fx_draw_transport::fx_draw_entity_kind::particle;
    CHECK(runtime::bloom_fx_draw_transport::validate_waterwave_draw_authority(fxdraw)==
          runtime::bloom_fx_draw_transport::waterwave_draw_authority_result::
              model_instance_not_joined);

    fxdraw.particle_model_instance_join=true;
    fxdraw.particle_model_instance=reinterpret_cast<void *>(0x2);
    fxdraw.particle_model_generation=1u;
    CHECK(runtime::bloom_fx_draw_transport::validate_waterwave_draw_authority(fxdraw)==
          runtime::bloom_fx_draw_transport::waterwave_draw_authority_result::
              model_join_channel_missing);

    fxdraw.particle_model_join_channel=
        runtime::bloom_fx_draw_transport::
            fx_particle_model_join_channel::appearance_owner;
    CHECK(runtime::bloom_fx_draw_transport::validate_waterwave_draw_authority(fxdraw)==
          runtime::bloom_fx_draw_transport::waterwave_draw_authority_result::
              authored_identity_not_exact);

    fxdraw.waterwave_authored_identity_exact=true;
    fxdraw.waterwave_same_model_instance=true;
    CHECK(runtime::bloom_fx_draw_transport::validate_waterwave_draw_authority(fxdraw)==
          runtime::bloom_fx_draw_transport::waterwave_draw_authority_result::
              authorized);

    // Canonical static authority is deliberately incomplete. Rev9384 closes
    // FXHG collector insertion -> entity callback, not writer exhaustiveness
    // or material identity transport. It must therefore fail open.
    const auto static_scene =
        current_bloom_scene_static_authority();
    CHECK(static_scene.fx_sfx_recurrence ==
          bloom_fx_sfx_recurrence_proof::entity_callback_route_closed);
    CHECK(!static_scene.writer_set_exhaustiveness_proven);
    CHECK(validate_bloom_scene_bridge_carrier(static_scene) ==
          bloom_scene_bridge_result::writer_set_not_closed);

    // The late DSR HDR surface is not an authenticated PTDE Q8 source.
    bloom_scene_bridge_carrier scene{};
    scene.source_domain=bloom_scene_source_domain::dsr_late_r11g11b10;
    scene.history_proof=bloom_scene_history_proof::blend_history_closed;
    scene.q8_a8r8g8b8_storage_verified=true;
    scene.source_freshness_verified=true;
    scene.graph_handoff_verified=true;
    CHECK(validate_bloom_scene_bridge_carrier(scene)==
          bloom_scene_bridge_result::source_domain_mismatch);

    // Terminal SAT/Q8 by itself is not enough: complete writer-set and blend
    // history closure are explicit prerequisites.
    scene.source_domain=
        bloom_scene_source_domain::ptde_normalized_scene_history_q8;
    scene.history_proof=
        bloom_scene_history_proof::terminal_sat_and_q8_storage_closed;
    CHECK(validate_bloom_scene_bridge_carrier(scene)==
          bloom_scene_bridge_result::writer_set_not_closed);
    scene.history_proof=bloom_scene_history_proof::writer_set_closed;
    CHECK(validate_bloom_scene_bridge_carrier(scene)==
          bloom_scene_bridge_result::writer_class_coverage_incomplete);

    scene.proven_writer_classes=bloom_known_writer_classes;
    scene.writer_set_exhaustiveness_proven=true;
    scene.writer_order=bloom_writer_order_proof::execution_order_closed;
    scene.draw_recurrence=
        bloom_draw_recurrence_proof::target_write_recurrence_closed;
    scene.fx_sfx_recurrence=
        bloom_fx_sfx_recurrence_proof::target_write_recurrence_closed;
    scene.fx_material_route=
        bloom_fx_material_route_proof::collector_material_binding_closed;
    scene.fx_blend_semantics=
        bloom_fx_blend_semantics_proof::render_state_tuple_closed;
    CHECK(validate_bloom_scene_bridge_carrier(scene)==
          bloom_scene_bridge_result::fx_identity_transport_not_closed);

    scene.fx_identity_transport=
        bloom_fx_identity_transport_proof::collector_draw_token_closed;
    scene.fx_identity_join=
        bloom_fx_identity_join_proof::same_collector_draw_closed;
    CHECK(validate_bloom_scene_bridge_carrier(scene)==
          bloom_scene_bridge_result::blend_history_not_closed);

    scene.history_proof=bloom_scene_history_proof::blend_history_closed;
    scene.capture_placement=
        bloom_scene_capture_placement::late_fullscreen_after_dsr_accumulation;
    scene.strategy=
        bloom_scene_construction_strategy::late_fullscreen_reconstruction;
    CHECK(validate_bloom_scene_bridge_carrier(scene)==
          bloom_scene_bridge_result::late_fullscreen_history_loss);

    scene.capture_placement=
        bloom_scene_capture_placement::history_preserving_pre_loss;
    scene.strategy=
        bloom_scene_construction_strategy::history_preserving_sidecar;
    CHECK(validate_bloom_scene_bridge_carrier(scene)==
          bloom_scene_bridge_result::exact_construction);

    // PTDE pass 0x04 has two semantic roles. Stock DSR depth-only 0x04 cannot
    // masquerade as the second PTDE Q8->RGBA color-copy edge.
    auto graph=ready_bloom_graph();
    CHECK(validate_bloom_legacy_graph_carrier(graph)==
          bloom_legacy_graph_result::exact_construction);
    graph.second_pass04=bloom_pass04_role::dsr_depth_only_host;
    CHECK(validate_bloom_legacy_graph_carrier(graph)==
          bloom_legacy_graph_result::second_pass04_role_mismatch);

    // Bloom scene bridge remains the first real blocker.
    bloom_unblock_context bloom{};
    bloom.packed_depth_logical_bridge_ready=true;
    bloom.type06_host_verified=true;
    bloom.pass04_ready=true;
    bloom.pass18_ready=true;
    bloom.pass19_1a_ready=true;

    auto bp=evaluate_bloom_unblock_preflight(bloom);
    CHECK(bp.state==post_unblock_state::blocked);
    CHECK(bp.reason==bloom_unblock_reason::scene_domain_bridge_not_ready);
    CHECK(!bp.direct_shader_body_swap_allowed);
    CHECK(bp.requires_q8_scene_bridge);
    CHECK(bp.requires_fixed_rgba_sidecars);
    CHECK(bp.preserve_stock_hdr_until_separately_ready);

    bloom=ready_bloom();
    bloom.packed_depth_quantization_closed=false;
    bp=evaluate_bloom_unblock_preflight(bloom);
    CHECK(bp.state==post_unblock_state::blocked);
    CHECK(bp.reason==
          bloom_unblock_reason::packed_depth_quantization_not_closed);

    bloom=ready_bloom();
    bloom.six_pass_graph_insertion_ready=false;
    bp=evaluate_bloom_unblock_preflight(bloom);
    CHECK(bp.reason==
          bloom_unblock_reason::six_pass_graph_insertion_not_ready);

    bloom=ready_bloom();
    bloom.stock_sfx_graph_preserved=false;
    bp=evaluate_bloom_unblock_preflight(bloom);
    CHECK(bp.reason==bloom_unblock_reason::stock_sfx_graph_not_preserved);

    bloom=ready_bloom();
    bp=evaluate_bloom_unblock_preflight(bloom);
    CHECK(bp.state==post_unblock_state::ready_for_partial);
    CHECK(bp.reason==bloom_unblock_reason::ready_for_partial);
    CHECK(!bp.direct_shader_body_swap_allowed);
    CHECK(!bp.includes_sfx_in_postprocess);
    CHECK(!bp.diagnostic_only);

    // Owner-approved diagnostic scope: Bloom may intentionally affect SFX.
    // This relaxes only SFX preservation, not scene/graph proof.
    bloom=ready_bloom();
    bloom.stock_sfx_graph_preserved=false;
    bloom.sfx_scope=postprocess_sfx_scope::full_frame_diagnostic;
    bloom.sfx_diagnostic_opt_in=true;
    bloom.sfx_diagnostic_attribution_ready=true;
    bloom.sfx_diagnostic_non_release=true;
    bp=evaluate_bloom_unblock_preflight(bloom);
    CHECK(bp.state==post_unblock_state::ready_for_partial);
    CHECK(bp.includes_sfx_in_postprocess);
    CHECK(bp.diagnostic_only);

    bloom.sfx_diagnostic_non_release=false;
    bp=evaluate_bloom_unblock_preflight(bloom);
    CHECK(bp.state==post_unblock_state::blocked);
    CHECK(bp.reason==
          bloom_unblock_reason::sfx_diagnostic_release_guard_not_ready);

    // HDR: the historical R24 decoded-input body substitution must remain
    // forbidden until an explicit Q8 scene-domain bridge exists.
    hdr_unblock_context hdr{};
    hdr.legacy_scene_scale_lane_ready=true;
    hdr.lightshaft_lane_ready=true;
    hdr.dsr_c56w_preserved=true;

    auto hp=evaluate_hdr_unblock_preflight(hdr);
    CHECK(hp.state==post_unblock_state::blocked);
    CHECK(hp.reason==hdr_unblock_reason::scene_domain_bridge_not_ready);
    CHECK(!hp.direct_legacy_body_swap_allowed);
    CHECK(!hp.whole_c56_copy_allowed);
    CHECK(hp.requires_q8_scene_bridge);
    CHECK(hp.preserve_native_sfx_island);

    hdr=ready_hdr();
    hdr.dsr_c56w_preserved=false;
    hp=evaluate_hdr_unblock_preflight(hdr);
    CHECK(hp.reason==hdr_unblock_reason::dsr_c56w_preservation_not_ready);

    hdr=ready_hdr();
    hdr.sfx_composite_order_verified=false;
    hp=evaluate_hdr_unblock_preflight(hdr);
    CHECK(hp.reason==hdr_unblock_reason::sfx_composite_order_not_verified);

    hdr=ready_hdr();
    hdr.sfx_inverse_tonemap_contract_preserved=false;
    hp=evaluate_hdr_unblock_preflight(hdr);
    CHECK(hp.reason==
          hdr_unblock_reason::sfx_inverse_tonemap_contract_not_preserved);

    hdr=ready_hdr();
    hp=evaluate_hdr_unblock_preflight(hdr);
    CHECK(hp.state==post_unblock_state::ready_for_partial);
    CHECK(hp.reason==hdr_unblock_reason::ready_for_partial);
    CHECK(!hp.direct_legacy_body_swap_allowed);
    CHECK(!hp.whole_c56_copy_allowed);
    CHECK(hp.preserve_native_sfx_island);
    CHECK(!hp.includes_sfx_in_postprocess);
    CHECK(!hp.diagnostic_only);

    // Owner-approved full-frame diagnostic: legacy/PTDE HDR can be tested on
    // the complete frame, including spells/VFX. Known DSR SFX inverse-tone
    // semantics are not claimed equivalent; telemetry and non-release guards
    // are mandatory so the test can be falsified by runtime/pixel evidence.
    hdr=ready_hdr();
    hdr.stock_dsr_sfx_preserved=false;
    hdr.sfx_inverse_tonemap_contract_preserved=false;
    hdr.sfx_scope=postprocess_sfx_scope::full_frame_diagnostic;
    hdr.sfx_diagnostic_opt_in=true;
    hdr.sfx_diagnostic_attribution_ready=true;
    hdr.sfx_diagnostic_non_release=true;
    hp=evaluate_hdr_unblock_preflight(hdr);
    CHECK(hp.state==post_unblock_state::ready_for_partial);
    CHECK(!hp.preserve_native_sfx_island);
    CHECK(hp.includes_sfx_in_postprocess);
    CHECK(hp.diagnostic_only);

    hdr.sfx_diagnostic_attribution_ready=false;
    hp=evaluate_hdr_unblock_preflight(hdr);
    CHECK(hp.state==post_unblock_state::blocked);
    CHECK(hp.reason==
          hdr_unblock_reason::sfx_diagnostic_attribution_not_ready);

    std::cout<<"postprocess_unblock_tests: PASS\n";
    return 0;
}
