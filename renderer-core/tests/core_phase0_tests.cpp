#include "dsrrl/core/renderer_core.hpp"

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

bool check(bool condition, const char *expression, int line)
{
    if (condition)
        return true;

    std::cerr << "CHECK FAILED line " << line << ": " << expression << '\n';
    return false;
}

#define CHECK(expr) do { if (!check(static_cast<bool>(expr), #expr, __LINE__)) return 1; } while (false)

} // namespace

int main()
{
    renderer_core core;

    // Phase 0 invariant: no feature, no hook, no transaction.
    CHECK(core.phase0_pass_through());
    CHECK(sizeof(carrier_v1) == 128);
    CHECK(carrier_ul_mask == ((1u << 6) | (1u << 7)));
    CHECK(operator_bit(static_cast<operator_id>(0xffu)) == 0u);

    // One hook site has exactly one semantic owner.
    const hook_claim ul_single{
        0x140563B80ull,
        operator_id::upper_lower,
        hook_semantic::lightbank_single_packer
    };
    CHECK(core.hooks().claim(ul_single));
    CHECK(core.hooks().claim(ul_single));

    const hook_claim illegal_second_owner{
        0x140563B80ull,
        operator_id::hemdir3,
        hook_semantic::lightbank_single_packer
    };
    CHECK(!core.hooks().claim(illegal_second_owner));

    const hook_claim invalid_owner{
        0x140563B90ull,
        static_cast<operator_id>(0xffu),
        hook_semantic::lightbank_single_packer
    };
    CHECK(!core.hooks().claim(invalid_owner));

    CHECK(core.hooks().release(0x140563B80ull, operator_id::upper_lower));
    CHECK(core.phase0_pass_through());

    // Exact receiver identity: fast hash is not enough.
    receiver_descriptor rx;
    rx.receiver_id = 33;
    rx.fast_hash = 0x1234567890ABCDEFull;
    rx.exact_sha256 = digest(7);
    rx.consumer_family_hash = 0xAA55;
    rx.capabilities = operator_bit(operator_id::upper_lower) |
                      operator_bit(operator_id::spec_rgb) |
                      operator_bit(operator_id::hemdir3);

    receiver_descriptor nonexact = rx;
    nonexact.receiver_id = 34;
    nonexact.exact_sha256 = {};
    CHECK(!core.receivers().register_receiver(nonexact));

    receiver_descriptor invalid_caps = rx;
    invalid_caps.receiver_id = 35;
    invalid_caps.capabilities = 0x80000000u;
    CHECK(!core.receivers().register_receiver(invalid_caps));

    CHECK(core.receivers().register_receiver(rx));
    CHECK(core.receivers().resolve(rx.fast_hash, rx.exact_sha256).has_value());
    CHECK(!core.receivers().resolve(rx.fast_hash, digest(8)).has_value());

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

    semantic_key invalid_key{
        static_cast<operator_id>(0xffu),
        0xABCDEF,
        4
    };
    CHECK(!core.snapshots().publish(invalid_key, 9, ul_payload));

    CHECK(core.snapshots().publish(ul_key, 10, ul_payload));
    auto snap = core.snapshots().latest(ul_key);
    CHECK(snap.has_value());
    CHECK(snap->sequence == 1);
    CHECK(snap->payload.lane_count == 2);
    CHECK(snap->payload.lanes[1].z == 0.6f);

    ul_payload.lanes[0].x = 0.9f;
    CHECK(core.snapshots().publish(ul_key, 11, ul_payload));
    snap = core.snapshots().latest(ul_key);
    CHECK(snap.has_value());
    CHECK(snap->sequence == 2);
    CHECK(snap->producer_epoch == 11);

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
    CHECK(plan.empty());

    // Enabling a feature is not enough: Core must also enforce the
    // canonical operator-local producer/consumer readiness gates.
    CHECK(core.features().set(operator_id::upper_lower, true));
    plan = core.build_plan(draw, requests, 2);
    CHECK(plan.empty());

    draw.activation.producer_ready = true;
    draw.activation.consumer_verified = true;
    plan = core.build_plan(draw, requests, 2);
    CHECK(plan.patch_count == 1);
    CHECK(plan.patches[0].op == operator_id::upper_lower);
    CHECK(plan.carrier_write_mask == carrier_ul_mask);

    // PARTIAL/ACTIVE_CANDIDATE/DIAGNOSTIC islands need their own exact
    // readiness contract in addition to generic Core gates.
    CHECK(core.features().set(operator_id::hemdir3, true));
    const island_request hemdir_unverified{
        operator_id::hemdir3,
        carrier_hemdir3_mask,
        true,
        false,
        false
    };
    plan = core.build_plan(draw, &hemdir_unverified, 1);
    CHECK(plan.empty());

    const island_request hemdir_verified{
        operator_id::hemdir3,
        carrier_hemdir3_mask,
        true,
        false,
        true
    };
    plan = core.build_plan(draw, &hemdir_verified, 1);
    CHECK(plan.patch_count == 1);
    CHECK(plan.patches[0].op == operator_id::hemdir3);
    CHECK(core.features().set(operator_id::hemdir3, false));

    // Restore the U/L plan used by the transaction ownership tests.
    plan = core.build_plan(draw, requests, 2);
    CHECK(plan.patch_count == 1);

    // Exactly one draw transaction owns a command until restore.
    CHECK(core.transactions().begin(
        draw.command, draw.draw_serial, draw.context, plan));
    CHECK(!core.transactions().begin(
        draw.command, draw.draw_serial + 1, draw.context, plan));
    CHECK(core.transactions().active(draw.command).has_value());
    CHECK(core.transactions().restore(draw.command));
    CHECK(!core.transactions().active(draw.command).has_value());

    // Carrier slot collisions fail before native draw.
    render_patch_plan invalid;
    invalid.patch_count = 2;
    invalid.patches[0] = island_patch{
        operator_id::upper_lower, carrier_ul_mask, true, false};
    invalid.patches[1] = island_patch{
        operator_id::hemdir3, carrier_slot_bit(carrier_v1_slot::upper_ptde), true, false};
    invalid.carrier_write_mask = carrier_ul_mask;
    CHECK(!core.transactions().begin(
        draw.command, 3, context_kind::immediate, invalid));

    // Malformed plans must fail before any array walk or native mutation.
    render_patch_plan oversized;
    oversized.patch_count =
        static_cast<std::uint32_t>(oversized.patches.size() + 1u);
    CHECK(!core.transactions().begin(
        draw.command, 4, context_kind::immediate, oversized));

    render_patch_plan invalid_lane;
    invalid_lane.patch_count = 1;
    invalid_lane.patches[0] = island_patch{
        operator_id::upper_lower, 0x80000000u, true, false};
    invalid_lane.carrier_write_mask = 0x80000000u;
    CHECK(!core.transactions().begin(
        draw.command, 5, context_kind::immediate, invalid_lane));

    render_patch_plan duplicate_owner;
    duplicate_owner.patch_count = 2;
    duplicate_owner.patches[0] = island_patch{
        operator_id::upper_lower, carrier_ul_mask, true, false};
    duplicate_owner.patches[1] = island_patch{
        operator_id::upper_lower, 0u, false, true};
    duplicate_owner.carrier_write_mask = carrier_ul_mask;
    CHECK(!core.transactions().begin(
        draw.command, 6, context_kind::immediate, duplicate_owner));

    // Restore Phase 0 invariant.
    CHECK(core.features().set(operator_id::upper_lower, false));
    core.snapshots().clear_all();
    CHECK(core.phase0_pass_through());

    std::cout << "dsrrl_renderer_core_tests: PASS\n";
    return 0;
}
