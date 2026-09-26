#pragma once

#include "dsrrl/operators/postprocess/bloom_scene_bridge.hpp"

namespace dsrrl::operators::postprocess {

// Canonical construction snapshot for the evidence currently closed in PTDE.
// This is intentionally incomplete and must remain fail-open until every
// independent proof axis required by validate_bloom_scene_bridge_carrier()
// reaches its exact state.
//
// Closed:
// - PTDE normalized SAT -> A8R8G8B8/Q8 terminal storage.
// - main FLVER/material writer class exists.
// - FX/SFX TargetScene writer class exists.
// - FXHG collector membership -> exact entity callback execution route.
// - authored FX blend-mode -> render-state semantics are independently known.
//
// Still OPEN:
// - exhaustive writer census.
// - inter-class execution order.
// - complete main/FX per-draw target-write recurrence.
// - authored SPX/MTD identity token transport to the same collector draw.
// - same-material identity join and concrete material/blend recurrence.
inline bloom_scene_bridge_carrier
current_bloom_scene_static_authority() noexcept
{
    bloom_scene_bridge_carrier c{};

    c.source_domain =
        bloom_scene_source_domain::
            ptde_normalized_scene_history_q8;
    c.history_proof =
        bloom_scene_history_proof::
            terminal_sat_and_q8_storage_closed;

    c.capture_placement =
        bloom_scene_capture_placement::
            history_preserving_pre_loss;
    c.strategy =
        bloom_scene_construction_strategy::
            history_preserving_sidecar;

    c.proven_writer_classes =
        bloom_known_writer_classes;

    // Presence of both currently known classes is not an exhaustive census.
    c.writer_set_exhaustiveness_proven = false;

    c.writer_order =
        bloom_writer_order_proof::unknown;
    c.draw_recurrence =
        bloom_draw_recurrence_proof::unknown;

    // Rev9384 closes insertion -> exact entity callback, but not material
    // route or target-write recurrence.
    c.fx_sfx_recurrence =
        bloom_fx_sfx_recurrence_proof::
            entity_callback_route_closed;

    c.fx_material_route =
        bloom_fx_material_route_proof::
            authored_spx_mtd_identity_closed;

    c.fx_blend_semantics =
        bloom_fx_blend_semantics_proof::
            render_state_tuple_closed;

    c.fx_identity_transport =
        bloom_fx_identity_transport_proof::unknown;
    c.fx_identity_join =
        bloom_fx_identity_join_proof::unknown;

    c.q8_a8r8g8b8_storage_verified = true;
    c.source_freshness_verified = false;
    c.graph_handoff_verified = false;

    return c;
}

} // namespace dsrrl::operators::postprocess
