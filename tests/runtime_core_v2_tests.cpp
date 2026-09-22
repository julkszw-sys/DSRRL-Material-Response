#include "../src/runtime_core_v2.hpp"

#include <iostream>

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "CHECK failed: " #expr << " at " << __FILE__ << ":" << __LINE__ << "\\n"; \
            return 1; \
        } \
    } while (false)

using namespace dsrrl::runtime_v2;

int main()
{
    tracker t;

    // Resource/view lifetime and handle reuse.
    const auto g1 = t.init_resource(0x100, 0xAAA, 0x111);
    CHECK(g1 == 1);
    CHECK(t.init_resource(0x100, 0xBBB, 0x222) == g1);
    CHECK(t.init_view(0x200, 0x100));
    CHECK(t.annotate_resource(0x100, 0xBEE, 0x223));

    auto r = t.resolve_view(0x200, operator_kind::envspec);
    CHECK(r);
    CHECK(r->generation == 1);
    CHECK(r->desc_hash == 0xBEE);
    CHECK(r->logical_hash == 0x223);

    // Logical identity is generation-safe and exact. A unique live logical
    // resource resolves; ambiguous identities fail open instead of guessing.
    auto logical = t.resolve_logical_resource(0x223, operator_kind::normal);
    CHECK(logical);
    CHECK(logical->handle == 0x100);
    CHECK(logical->generation == 1);

    CHECK(t.init_resource(0x110, 0xDDD, 0x444) == 1);
    CHECK(t.init_resource(0x111, 0xEEE, 0x444) == 1);
    CHECK(!t.resolve_logical_resource(0x444, operator_kind::diffuse));
    t.destroy_resource(0x111);
    logical = t.resolve_logical_resource(0x444, operator_kind::diffuse);
    CHECK(logical);
    CHECK(logical->handle == 0x110);

    t.destroy_resource(0x100);
    CHECK(!t.resolve_view(0x200, operator_kind::envspec));

    const auto g2 = t.init_resource(0x100, 0xCCC, 0x333);
    CHECK(g2 == 2);
    CHECK(!t.resolve_view(0x200, operator_kind::envspec));

    // Command-list lifecycle is strict: state may not appear before init_command.\n    t.init_pipeline(0x300, 0xCAFE, 23, 0x1000, true);\n    CHECK(!t.bind_pipeline(0x499, 0x300));\n    CHECK(t.begin_draw(0x499) == 0);\n    CHECK(!t.command_state(0x499));\n\n    // Per-command-list state: immediate/deferred contexts are isolated.
    t.init_pipeline(0x301, 0xDEAD, 24, 0x1001, true);
    t.init_pipeline(0x302, 0xBEEF, 47, 0x1002, true);
    t.init_command(0x401);
    t.init_command(0x402);

    CHECK(t.bind_pipeline(0x401, 0x301));
    CHECK(t.bind_pipeline(0x402, 0x302));
    CHECK(t.begin_draw(0x401) == 1);
    CHECK(t.begin_draw(0x402) == 1);

    const std::uint64_t slot12_view = 0x200;
    CHECK(t.bind_pixel_shader_views(0x401, 12, 1, &slot12_view));

    // The original view still points at resource generation 1, while handle
    // 0x100 was reused as generation 2 above.
    CHECK(!t.resolve_bound_pixel_shader_resource(
        0x401, 12, operator_kind::envspec));

    t.destroy_view(0x200);
    CHECK(t.init_view(0x200, 0x100));

    // The command still references the old view generation until it is rebound.
    CHECK(!t.resolve_bound_pixel_shader_resource(
        0x401, 12, operator_kind::envspec));

    CHECK(t.bind_pixel_shader_views(0x401, 12, 1, &slot12_view));
    const auto bound_resource = t.resolve_bound_pixel_shader_resource(
        0x401, 12, operator_kind::envspec);
    CHECK(bound_resource);
    CHECK(bound_resource->generation == 2);

    // Re-annotation moves the live resource to a new logical identity without
    // letting stale reverse-index entries alias it.
    CHECK(t.annotate_resource_logical(0x100, 0x555));
    CHECK(!t.resolve_logical_resource(0x333, operator_kind::normal));
    logical = t.resolve_logical_resource(0x555, operator_kind::normal);
    CHECK(logical);
    CHECK(logical->handle == 0x100);
    CHECK(logical->generation == 2);

    const auto c1 = t.command_state(0x401);
    const auto c2 = t.command_state(0x402);
    CHECK(c1 && c2);
    CHECK(c1->bound_pipeline == 0x301);
    CHECK(c2->bound_pipeline == 0x302);

    auto bound = t.resolve_bound_pipeline(0x401);
    CHECK(bound);
    CHECK(bound->generation == 1);
    CHECK(bound->pixel_shader_hash == 0xDEAD);

    // Pipeline handle reuse must not make an old command binding alias the new pipeline.
    t.destroy_pipeline(0x301);
    t.init_pipeline(0x301, 0xABCD, 25, 0x1003, true);
    CHECK(!t.resolve_bound_pipeline(0x401));
    CHECK(t.bind_pipeline(0x401, 0x301));
    bound = t.resolve_bound_pipeline(0x401);
    CHECK(bound);
    CHECK(bound->generation == 2);
    CHECK(bound->pixel_shader_hash == 0xABCD);

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

    CHECK(!t.begin_transaction(
        0x401, operator_kind::envspec, exact_material_route, incomplete));

    // A complete route may activate, but must restore.
    const route_observation complete{exact_material_route.required};
    CHECK(t.begin_transaction(
        0x401, operator_kind::envspec, exact_material_route, complete));
    CHECK(t.restore_transaction(0x401, operator_kind::envspec));

    // A frame boundary catches leaked draw-scoped state.
    CHECK(t.begin_transaction(
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

    CHECK(dif_stages.hits[static_cast<std::size_t>(route_stage::capture)] == 1);
    CHECK(dif_stages.hits[static_cast<std::size_t>(route_stage::receiver)] == 1);
    CHECK(dif_stages.hits[static_cast<std::size_t>(route_stage::resource_lookup)] == 1);
    CHECK(dif_stages.hits[static_cast<std::size_t>(route_stage::route_gate)] == 1);
    CHECK(dif_stages.hits[static_cast<std::size_t>(route_stage::sidecar_ready)] == 1);
    CHECK(dif_stages.hits[static_cast<std::size_t>(route_stage::final_bind)] == 1);
    CHECK(dif_stages.hits[static_cast<std::size_t>(route_stage::restore)] == 1);

    CHECK(snap.live_logical_resources == 2);

    CHECK(env.activations == 1);
    CHECK(env.restores == 1);
    CHECK(env.fail_open == 1);
    CHECK(env.rejected == 1);
    CHECK(env.logical_miss == 0);

    const auto &dif = snap.operators[
        static_cast<std::size_t>(operator_kind::diffuse)];
    CHECK(dif.logical_ambiguous == 1);

    const auto &normal = snap.operators[
        static_cast<std::size_t>(operator_kind::normal)];
    CHECK(normal.logical_miss == 1);

    CHECK(nrm.activations == 1);
    CHECK(nrm.unrestored == 1);
    CHECK(nrm.fail_open == 0);

    std::cout << "runtime_core_v2_tests: PASS\n";
    return 0;
}
