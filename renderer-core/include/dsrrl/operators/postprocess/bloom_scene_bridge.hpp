#pragma once

#include <cstdint>

namespace dsrrl::operators::postprocess {

// Capture-free semantic boundary for the shared pre-Bloom/pre-HDR scene source.
// PTDE's +0x5C/+0x60 A8R8G8B8 target is an accumulated render history. Exact
// construction therefore requires not only terminal Q8 semantics but coverage
// of every writer class that can participate in the target history.
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

enum bloom_scene_writer_class : std::uint32_t {
    bloom_writer_none = 0,
    bloom_writer_main_flver_material = 1u << 0,
    bloom_writer_fx_sfx_target_scene = 1u << 1
};

constexpr std::uint32_t bloom_known_writer_classes =
    bloom_writer_main_flver_material | bloom_writer_fx_sfx_target_scene;

enum class bloom_scene_bridge_result : std::uint8_t {
    exact_construction = 0,
    source_domain_mismatch,
    terminal_storage_not_closed,
    writer_set_not_closed,
    writer_class_coverage_incomplete,
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
    std::uint32_t proven_writer_classes = bloom_writer_none;
    bool writer_set_exhaustiveness_proven = false;
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
    if (!c.writer_set_exhaustiveness_proven ||
        (c.proven_writer_classes & bloom_known_writer_classes) != bloom_known_writer_classes)
        return bloom_scene_bridge_result::writer_class_coverage_incomplete;
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
