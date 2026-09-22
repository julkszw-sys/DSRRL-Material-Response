#include "../src/runtime_core_v2.hpp"

#include <cassert>
#include <iostream>

using namespace dsrrl::runtime_v2;

int main()
{
    tracker t;

    // Resource/view lifetime and handle reuse.
    const auto g1 = t.init_resource(0x100, 0xAAA, 0x111);
    assert(g1 == 1);
    assert(t.init_resource(0x100, 0xBBB, 0x222) == g1);
    assert(t.init_view(0x200, 0x100));

    auto r = t.resolve_view(0x200, operator_kind::envspec);
    assert(r);
    assert(r->generation == 1);
    assert(r->desc_hash == 0xBBB);
    assert(r->logical_hash == 0x222);

    t.destroy_resource(0x100);
    assert(!t.resolve_view(0x200, operator_kind::envspec));

    const auto g2 = t.init_resource(0x100, 0xCCC, 0x333);
    assert(g2 == 2);
    assert(!t.resolve_view(0x200, operator_kind::envspec));

    // Per-command-list state: immediate/deferred contexts are isolated.
    t.init_pipeline(0x301, 0xDEAD, 24, 0x1001, true);
    t.init_pipeline(0x302, 0xBEEF, 47, 0x1002, true);
    t.init_command(0x401);
    t.init_command(0x402);

    assert(t.bind_pipeline(0x401, 0x301));
    assert(t.bind_pipeline(0x402, 0x302));
    assert(t.begin_draw(0x401) == 1);
    assert(t.begin_draw(0x402) == 1);

    const auto c1 = t.command_state(0x401);
    const auto c2 = t.command_state(0x402);
    assert(c1 && c2);
    assert(c1->bound_pipeline == 0x301);
    assert(c2->bound_pipeline == 0x302);

    // Missing exact material proof must fail open.
    const route_contract exact_material_route{
        evidence_shader |
        evidence_receiver |
        evidence_material |
        evidence_resource |
        evidence_logical_id |
        evidence_format |
        evidence_state
    };

    const route_observation incomplete{
        evidence_shader |
        evidence_receiver |
        evidence_resource |
        evidence_logical_id |
        evidence_format |
        evidence_state
    };

    assert(!t.begin_transaction(
        0x401, operator_kind::envspec, exact_material_route, incomplete));

    // A complete route may activate, but must restore.
    const route_observation complete{exact_material_route.required};
    assert(t.begin_transaction(
        0x401, operator_kind::envspec, exact_material_route, complete));
    assert(t.restore_transaction(0x401, operator_kind::envspec));

    // A frame boundary catches leaked draw-scoped state.
    assert(t.begin_transaction(
        0x402,
        operator_kind::normal,
        route_contract{evidence_receiver},
        route_observation{evidence_receiver}));

    const auto snap = t.seal_frame();

    const auto &env = snap.operators[
        static_cast<std::size_t>(operator_kind::envspec)];
    const auto &nrm = snap.operators[
        static_cast<std::size_t>(operator_kind::normal)];

    assert(env.activations == 1);
    assert(env.restores == 1);
    assert(env.fail_open == 1);
    assert(env.rejected == 1);

    assert(nrm.activations == 1);
    assert(nrm.unrestored == 1);
    assert(nrm.fail_open == 1);

    std::cout << "runtime_core_v2_tests: PASS\n";
    return 0;
}
