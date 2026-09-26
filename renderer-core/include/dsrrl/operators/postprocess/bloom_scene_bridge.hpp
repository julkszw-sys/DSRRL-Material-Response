#pragma once

#include <cstdint>

namespace dsrrl::operators::postprocess {

// Semantic boundary shared by restored PTDE Bloom and legacy HDR.
//
// PTDE scene writers terminate in RGB SAT and store through A8R8G8B8 UNORM/Q8.
// Direct legacy-HDR evidence reconstructs Base as saturate(c56.z * scene_q8).
// No nonlinear PTDE scene-code decoder is established. Therefore a late stock
// DSR R11G11B10_FLOAT surface is not a valid source from which to invent a
// universal inverse encoder. Exact construction requires the PTDE-normalized
// scene history itself to be preserved or replayed before Bloom/HDR.
enum class bloom_scene_source_domain : std::uint8_t {
    unknown = 0,
    dsr_late_r11g11b10,
    ptde_normalized_scene_history_q8
};

enum class bloom_scene_history_proof : std::uint8_t {
    unknown = 0,
    terminal_sat_and_q8_storage_closed,
    writer_set_closed,
    blend_history_closed
};

// Fresh retail RE closes the former placement ambiguity: PTDE main-scene
// AddDrawStageMask(0x01000000) reaches parser+0x84, runtime executor 0xBD1840
// writes the active one-hot stage into DrawContext+0x8C, and BlendMode4 then
// performs decision-relevant SRCALPHA/ONE accumulation into the Q8 scene.
// Once that iterative blend/storage history has been collapsed into the stock
// DSR floating-point scene, there is no universal pointwise exact inverse.
enum class bloom_scene_capture_placement : std::uint8_t {
    unknown = 0,
    history_preserving_pre_loss,
    late_fullscreen_after_dsr_accumulation
};

enum class bloom_scene_bridge_result : std::uint8_t {
    exact_construction = 0,
    source_domain_mismatch,
    terminal_storage_not_closed,
    writer_set_not_closed,
    blend_history_not_closed,
    q8_storage_not_verified,
    source_freshness_not_verified,
    history_preserving_placement_not_verified,
    late_fullscreen_history_loss,
    graph_handoff_not_verified
};

struct bloom_scene_bridge_carrier {
    bloom_scene_source_domain source_domain =
        bloom_scene_source_domain::unknown;
    bloom_scene_history_proof history_proof =
        bloom_scene_history_proof::unknown;
    bloom_scene_capture_placement capture_placement =
        bloom_scene_capture_placement::unknown;

    bool q8_a8r8g8b8_storage_verified = false;
    bool source_freshness_verified = false;
    bool graph_handoff_verified = false;
};

inline bloom_scene_bridge_result validate_bloom_scene_bridge_carrier(
    const bloom_scene_bridge_carrier &c) noexcept
{
    if (c.source_domain !=
        bloom_scene_source_domain::ptde_normalized_scene_history_q8)
        return bloom_scene_bridge_result::source_domain_mismatch;

    if (c.history_proof == bloom_scene_history_proof::unknown)
        return bloom_scene_bridge_result::terminal_storage_not_closed;

    if (c.history_proof ==
        bloom_scene_history_proof::terminal_sat_and_q8_storage_closed)
        return bloom_scene_bridge_result::writer_set_not_closed;

    if (c.history_proof == bloom_scene_history_proof::writer_set_closed)
        return bloom_scene_bridge_result::blend_history_not_closed;

    if (!c.q8_a8r8g8b8_storage_verified)
        return bloom_scene_bridge_result::q8_storage_not_verified;

    if (!c.source_freshness_verified)
        return bloom_scene_bridge_result::source_freshness_not_verified;

    if (c.capture_placement ==
        bloom_scene_capture_placement::late_fullscreen_after_dsr_accumulation)
        return bloom_scene_bridge_result::late_fullscreen_history_loss;

    if (c.capture_placement !=
        bloom_scene_capture_placement::history_preserving_pre_loss)
        return bloom_scene_bridge_result::
            history_preserving_placement_not_verified;

    if (!c.graph_handoff_verified)
        return bloom_scene_bridge_result::graph_handoff_not_verified;

    return bloom_scene_bridge_result::exact_construction;
}

} // namespace dsrrl::operators::postprocess
