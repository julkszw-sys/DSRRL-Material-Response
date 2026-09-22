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
    assert(t.annotate_resource(0x100, 0xBEE, 0x223));

    auto r = t.resolve_view(0x200, operator_kind::envspec);
    assert(r);
    assert(r->generation == 1);
    assert(r->desc_hash == 0xBEE);
    assert(r->logical_hash == 0x223);

    // Logical identity is generation-safe and exact. A unique live logical
    // resource resolves; ambiguous identities fail open instead of guessing.
    auto logical = t.resolve_logical_resource(0x223, operator_kind::normal);
    assert(logical);
    assert(logical->handle == 0x100);
    assert(logical->generation == 1);

    assert(t.init_resource(0x110, 0xDDD, 0x444) == 1);
    assert(t.init_resource(0x111, 0xEEE, 0x444) == 1);
    assert(!t.resolve_logical_resource(0x444, operator_kind::diffuse));
    t.destroy_resource(0x111);
    logical = t.resolve_logical_resource(0x444, operator_kind::diffuse);
    assert(logical);
    assert(logical->handle == 0x110);

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

    const std::uint64_t slot12_view = 0x200;
    assert(t.bind_pixel_shader_views(0x401, 12, 1, &slot12_view));

    // The original view still points at resource generation 1, while handle
    // 0x100 was reused as generation 2 above.
    assert(!t.resolve_bound_pixel_shader_resource(
        0x401, 12, operator_kind::envspec));

    t.destroy_view(0x200);
    assert(t.init_view(0x200, 0x100));

    // The command still references the old view generation until it is rebound.
    assert(!t.resolve_bound_pixel_shader_resource(
        0x401, 12, operator_kind::envspec));

    assert(t.bind_pixel_shader_views(0x401, 12, 1, &slot12_view));
    const auto bound_resource = t.resolve_bound_pixel_shader_resource(
        0x401, 12, operator_kind::envspec);
    assert(bound_resource);
    assert(bound_resource->generation == 2);

    // Re-annotation moves the live resource to a new logical identity without
    // letting stale reverse-index entries alias it.
    assert(t.annotate_resource_logical(0x100, 0x555));
    assert(!t.resolve_logical_resource(0x333, operator_kind::normal));
    logical = t.resolve_logical_resource(0x555, operator_kind::normal);
    assert(logical);
    assert(logical->handle == 0x100);
    assert(logical->generation == 2);

    const auto c1 = t.command_state(0x401);
    const auto c2 = t.command_state(0x402);
    assert(c1 && c2);
    assert(c1->bound_pipeline == 0x301);
    assert(c2->bound_pipeline == 0x302);

    auto bound = t.resolve_bound_pipeline(0x401);
    assert(bound);
    assert(bound->generation == 1);
    assert(bound->pixel_shader_hash == 0xDEAD);

    // Pipeline handle reuse must not make an old command binding alias the new pipeline.
    t.destroy_pipeline(0x301);
    t.init_pipeline(0x301, 0xABCD, 25, 0x1003, true);
    assert(!t.resolve_bound_pipeline(0x401));
    assert(t.bind_pipeline(0x401, 0x301));
    bound = t.resolve_bound_pipeline(0x401);
    assert(bound);
    assert(bound->generation == 2);
    assert(bound->pixel_shader_hash == 0xABCD);

    // Source-level staged telemetry replaces the old binary one-shot flags.
    t.note_stage(operator_kind::diffuse, route_stage::capture);
    t.note_stage(operator_kind::diffuse, route_stage::receiver);
    t.note_stage(operator_kind::diffuse, route_stage::resource_lookup);
    t.note_stage(operator_kind::diffuse, route_stage::route_gate);
    t.note_stage(operator_kind::diffuse, route_stage::sidecar_ready);
    t.note_stage(operator_kind::diffuse, route_stage::final_bind);
    t.note_stage(operator_kind::diffuse, route_stage::restore);

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

    const auto &dif_stages = snap.stages[
        static_cast<std::size_t>(operator_kind::diffuse)];

    assert(dif_stages.hits[static_cast<std::size_t>(route_stage::capture)] == 1);
    assert(dif_stages.hits[static_cast<std::size_t>(route_stage::receiver)] == 1);
    assert(dif_stages.hits[static_cast<std::size_t>(route_stage::resource_lookup)] == 1);
    assert(dif_stages.hits[static_cast<std::size_t>(route_stage::route_gate)] == 1);
    assert(dif_stages.hits[static_cast<std::size_t>(route_stage::sidecar_ready)] == 1);
    assert(dif_stages.hits[static_cast<std::size_t>(route_stage::final_bind)] == 1);
    assert(dif_stages.hits[static_cast<std::size_t>(route_stage::restore)] == 1);

    assert(snap.live_logical_resources == 2);

    assert(env.activations == 1);
    assert(env.restores == 1);
    assert(env.fail_open == 1);
    assert(env.rejected == 1);
    assert(env.logical_miss == 0);

    const auto &dif = snap.operators[
        static_cast<std::size_t>(operator_kind::diffuse)];
    assert(dif.logical_ambiguous == 1);

    const auto &normal = snap.operators[
        static_cast<std::size_t>(operator_kind::normal)];
    assert(normal.logical_miss == 1);

    assert(nrm.activations == 1);
    assert(nrm.unrestored == 1);
    assert(nrm.fail_open == 0);

    std::cout << "runtime_core_v2_tests: PASS\n";
    return 0;
}
