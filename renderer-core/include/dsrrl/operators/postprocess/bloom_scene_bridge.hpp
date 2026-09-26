#pragma once

#include <cstdint>

namespace dsrrl::operators::postprocess {

// Capture-free semantic boundary for the shared pre-Bloom/pre-HDR scene source.
// PTDE's +0x5C/+0x60 A8R8G8B8 target is an accumulated render history: the
// main plan produces stage 0x01000000 and ordinary authored materials can blend
// into that same target before UNORM8 storage. Because blending and Q8 storage
// are information-losing/non-commuting operations, exact construction cannot be
// authorized as a universal late fullscreen transform of DSR's final R11 scene.
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

enum class bloom_scene_construction_strategy : std::uint8_t {
    unknown = 0,
    late_fullscreen_reconstruction,
    pre_storage_history_replay,
    history_preserving_sidecar
};

enum class bloom_scene_bridge_result : std::uint8_t {
    exact_construction = 0,
    source_domain_mismatch,
    terminal_storage_not_closed,
    writer_set_not_closed,
    blend_history_not_closed,
    late_fullscreen_reconstruction_rejected,
    construction_strategy_not_closed,
    q8_storage_not_verified,
    source_freshness_not_verified,
    draw_local_handoff_not_verified
};

struct bloom_scene_bridge_carrier {
    bloom_scene_source_domain source_domain = bloom_scene_source_domain::unknown;
    bloom_scene_history_proof history_proof = bloom_scene_history_proof::unknown;
    bloom_scene_construction_strategy strategy = bloom_scene_construction_strategy::unknown;
    bool q8_a8r8g8b8_storage_verified = false;
    bool source_freshness_verified = false;
    bool draw_local_handoff_verified = false;
};

inline bloom_scene_bridge_result validate_bloom_scene_bridge_carrier(
    const bloom_scene_bridge_carrier &c) noexcept
{
    if (c.source_domain != bloom_scene_source_domain::ptde_normalized_scene_history_q8)
        return bloom_scene_bridge_result::source_domain_mismatch;
    if (c.history_proof == bloom_scene_history_proof::unknown)
        return bloom_scene_bridge_result::terminal_storage_not_closed;
    if (c.history_proof == bloom_scene_history_proof::terminal_sat_and_q8_storage_closed)
        return bloom_scene_bridge_result::writer_set_not_closed;
    if (c.history_proof == bloom_scene_history_proof::writer_set_closed)
        return bloom_scene_bridge_result::blend_history_not_closed;
    if (c.strategy == bloom_scene_construction_strategy::late_fullscreen_reconstruction)
        return bloom_scene_bridge_result::late_fullscreen_reconstruction_rejected;
    if (c.strategy != bloom_scene_construction_strategy::pre_storage_history_replay &&
        c.strategy != bloom_scene_construction_strategy::history_preserving_sidecar)
        return bloom_scene_bridge_result::construction_strategy_not_closed;
    if (!c.q8_a8r8g8b8_storage_verified)
        return bloom_scene_bridge_result::q8_storage_not_verified;
    if (!c.source_freshness_verified)
        return bloom_scene_bridge_result::source_freshness_not_verified;
    if (!c.draw_local_handoff_verified)
        return bloom_scene_bridge_result::draw_local_handoff_not_verified;
    return bloom_scene_bridge_result::exact_construction;
}

} // namespace dsrrl::operators::postprocess
