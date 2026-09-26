#pragma once

#include <cstdint>

namespace dsrrl::operators::postprocess {

// Capture-free semantic boundary for the shared pre-Bloom/pre-HDR scene bridge.
// PTDE stores the scene in an A8R8G8B8/Q8 code domain before the second 0x04
// color-copy producer and legacy HDR. DSR's stock scene carrier is decoded
// positive-linear R11G11B10_FLOAT. Resource/format conversion alone is not a
// semantic bridge: an explicit inverse of the PTDE scene-code decode must be
// closed independently before a Q8 source can be authenticated.
enum class bloom_scene_source_domain : std::uint8_t {
    unknown = 0,
    dsr_decoded_positive_linear_r11g11b10,
    ptde_stored_scene_q8
};

enum class bloom_scene_encode_proof : std::uint8_t {
    unknown = 0,
    decode_consumer_closed,
    inverse_encode_closed
};

enum class bloom_scene_bridge_result : std::uint8_t {
    exact_construction = 0,
    source_domain_mismatch,
    decode_consumer_not_closed,
    inverse_encode_not_closed,
    q8_storage_not_verified,
    source_freshness_not_verified,
    draw_local_handoff_not_verified
};

struct bloom_scene_bridge_carrier {
    bloom_scene_source_domain source_domain = bloom_scene_source_domain::unknown;
    bloom_scene_encode_proof encode_proof = bloom_scene_encode_proof::unknown;
    bool q8_a8r8g8b8_storage_verified = false;
    bool source_freshness_verified = false;
    bool draw_local_handoff_verified = false;
};

inline bloom_scene_bridge_result validate_bloom_scene_bridge_carrier(
    const bloom_scene_bridge_carrier &c) noexcept
{
    if (c.source_domain !=
        bloom_scene_source_domain::dsr_decoded_positive_linear_r11g11b10)
        return bloom_scene_bridge_result::source_domain_mismatch;
    if (c.encode_proof == bloom_scene_encode_proof::unknown)
        return bloom_scene_bridge_result::decode_consumer_not_closed;
    if (c.encode_proof != bloom_scene_encode_proof::inverse_encode_closed)
        return bloom_scene_bridge_result::inverse_encode_not_closed;
    if (!c.q8_a8r8g8b8_storage_verified)
        return bloom_scene_bridge_result::q8_storage_not_verified;
    if (!c.source_freshness_verified)
        return bloom_scene_bridge_result::source_freshness_not_verified;
    if (!c.draw_local_handoff_verified)
        return bloom_scene_bridge_result::draw_local_handoff_not_verified;
    return bloom_scene_bridge_result::exact_construction;
}

} // namespace dsrrl::operators::postprocess
