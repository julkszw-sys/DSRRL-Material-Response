#include "dsrrl/core/operator_catalog.hpp"
#include "dsrrl/core/island_policy.hpp"
#include "dsrrl/operators/postprocess/future_runtime_preflight.hpp"

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

    // Bloom: current canonical blocker is the missing decoded-linear -> Q8
    // stored-scene semantic bridge. The preflight must expose that first.
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

    std::cout<<"postprocess_unblock_tests: PASS\n";
    return 0;
}
