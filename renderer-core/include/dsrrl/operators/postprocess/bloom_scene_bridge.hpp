#pragma once

#include <cstdint>

namespace dsrrl::operators::postprocess {

// Capture-free semantic boundary for the shared pre-Bloom/pre-HDR scene source.
// PTDE does not expose a proven nonlinear "scene-code decoder" here. Direct
// shader evidence instead shows HDR reconstructing Base as
// saturate(scene_q8 * c56.z), while scene writers terminate in SAT and store
// through A8R8G8B8 UNORM. Therefore a universal late R11G11B10->Q8 inverse
// transform must NOT be invented. Exact construction requires proof that the
// PTDE normalized scene history has been preserved/replayed before Bloom/HDR.
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

enum class bloom_scene_bridge_result : std::uint8_t {
    exact_construction = 0,
    source_domain_mismatch,
    terminal_storage_not_closed,
    writer_set_not_closed,
    blend_history_not_closed,
    q8_storage_not_verified,
    source_freshness_not_verified,
    draw_local_handoff_not_verified
};

struct bloom_scene_bridge_carrier {
    bloom_scene_source_domain source_domain = bloom_scene_source_domain::unknown;
    bloom_scene_history_proof history_proof = bloom_scene_history_proof::unknown;
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
    if (!c.q8_a8r8g8b8_storage_verified)
        return bloom_scene_bridge_result::q8_storage_not_verified;
    if (!c.source_freshness_verified)
        return bloom_scene_bridge_result::source_freshness_not_verified;
    if (!c.draw_local_handoff_verified)
        return bloom_scene_bridge_result::draw_local_handoff_not_verified;
    return bloom_scene_bridge_result::exact_construction;
}

} // namespace dsrrl::operators::postprocess
