#include "dsrrl/core/renderer_core.hpp"

#include <cassert>
#include <iostream>

using namespace dsrrl::core;

namespace {

sha256_digest digest(std::uint8_t seed)
{
    sha256_digest out{};
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<std::uint8_t>(seed + i);
    return out;
}

} // namespace

int main()
{
    renderer_core core;

    // Phase 0 invariant: no feature, no hook, no transaction.
    assert(core.phase0_pass_through());
    assert(sizeof(carrier_v1) == 128);
    assert(carrier_ul_mask == ((1u << 6) | (1u << 7)));

    // One hook site has exactly one semantic owner.
    const hook_claim ul_single{
        0x140563B80ull,
        operator_id::upper_lower,
        hook_semantic::lightbank_single_packer
    };
    assert(core.hooks().claim(ul_single));
    assert(core.hooks().claim(ul_single));

    const hook_claim illegal_second_owner{
        0x140563B80ull,
        operator_id::hemdir3,
        hook_semantic::lightbank_single_packer
    };
    assert(!core.hooks().claim(illegal_second_owner));
    assert(core.hooks().release(0x140563B80ull, operator_id::upper_lower));
    assert(core.phase0_pass_through());

    // Exact receiver identity: fast hash is not enough.
    receiver_descriptor rx;
    rx.receiver_id = 33;
    rx.fast_hash = 0x1234567890ABCDEFull;
    rx.exact_sha256 = digest(7);
    rx.consumer_family_hash = 0xAA55;
    rx.capabilities = operator_bit(operator_id::upper_lower) |
                      operator_bit(operator_id::spec_rgb);
    assert(core.receivers().register_receiver(rx));
    assert(core.receivers().resolve(rx.fast_hash, rx.exact_sha256).has_value());
    assert(!core.receivers().resolve(rx.fast_hash, digest(8)).has_value());

    // Snapshot bus copies semantic values and carries no game pointer.
    semantic_payload ul_payload;
    ul_payload.lane_count = 2;
    ul_payload.lanes[0] = float4{0.1f, 0.2f, 0.3f, 0.0f};
    ul_payload.lanes[1] = float4{0.4f, 0.5f, 0.6f, 0.0f};

    const semantic_key ul_key{
        operator_id::upper_lower,
        0xABCDEF,
        4
    };

    assert(core.snapshots().publish(ul_key, 10, ul_payload));
    auto snap = core.snapshots().latest(ul_key);
    assert(snap);
    assert(snap->sequence == 1);
    assert(snap->payload.lane_count == 2);
    assert(snap->payload.lanes[1].z == 0.6f);

    ul_payload.lanes[0].x = 0.9f;
    assert(core.snapshots().publish(ul_key, 11, ul_payload));
    snap = core.snapshots().latest(ul_key);
    assert(snap && snap->sequence == 2 && snap->producer_epoch == 11);

    // Disabled islands never enter a draw patch plan.
    draw_context draw;
    draw.command = 0x1000;
    draw.draw_serial = 1;
    draw.context = context_kind::deferred;
    draw.shader_fast_hash = rx.fast_hash;
    draw.shader_sha256 = rx.exact_sha256;

    const island_request requests[] = {
        {operator_id::upper_lower, carrier_ul_mask, true, false},
        {operator_id::spec_rgb, 0, true, true}
    };

    auto plan = core.build_plan(draw, requests, 2);
    assert(plan.empty());

    // Enabling only U/L cannot activate SpecRGB.
    assert(core.features().set(operator_id::upper_lower, true));
    plan = core.build_plan(draw, requests, 2);
    assert(plan.patch_count == 1);
    assert(plan.patches[0].op == operator_id::upper_lower);
    assert(plan.carrier_write_mask == carrier_ul_mask);

    // Exactly one draw transaction owns a command until restore.
    assert(core.transactions().begin(
        draw.command, draw.draw_serial, draw.context, plan));
    assert(!core.transactions().begin(
        draw.command, draw.draw_serial + 1, draw.context, plan));
    assert(core.transactions().active(draw.command).has_value());
    assert(core.transactions().restore(draw.command));
    assert(!core.transactions().active(draw.command).has_value());

    // Carrier slot collisions fail before native draw.
    render_patch_plan invalid;
    invalid.patch_count = 2;
    invalid.patches[0] = island_patch{
        operator_id::upper_lower, carrier_ul_mask, true, false};
    invalid.patches[1] = island_patch{
        operator_id::hemdir3, carrier_slot_bit(carrier_v1_slot::upper_ptde), true, false};
    invalid.carrier_write_mask = carrier_ul_mask;
    assert(!core.transactions().begin(
        draw.command, 3, context_kind::immediate, invalid));

    // Restore Phase 0 invariant.
    assert(core.features().set(operator_id::upper_lower, false));
    core.snapshots().clear_all();
    assert(core.phase0_pass_through());

    std::cout << "dsrrl_renderer_core_tests: PASS\n";
    return 0;
}
