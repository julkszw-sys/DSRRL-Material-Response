#pragma once

#include <cstdint>

namespace dsrrl::operators::postprocess {

// Capture-free PTDE Bloom graph identity. Pass 0x04 is intentionally split
// into two different resources: the first invocation writes packed depth;
// the second invocation is the pre-BrightPass color-copy producer. Stock DSR
// pass 0x04 is depth-only and therefore cannot authenticate the color-copy
// edge merely because the pass id matches.
enum class bloom_pass04_role : std::uint8_t {
    unknown = 0,
    packed_depth_first,
    pre_brightpass_color_copy_second,
    dsr_depth_only_host
};

struct bloom_legacy_graph_carrier {
    bool q8_scene_1024x720_ready = false;
    bool packed_depth_256x180_ready = false;
    bool fixed_rgba_256x180_ready = false;
    bool fixed_rgba_128x90_ready = false;

    bloom_pass04_role first_pass04 = bloom_pass04_role::unknown;
    bloom_pass04_role second_pass04 = bloom_pass04_role::unknown;
    bool second_pass04_source_is_q8_scene = false;
    bool second_pass04_destination_is_rgba_256x180 = false;

    bool pass18_brightpass_ready = false;
    bool pass19_blur_h_ready = false;
    bool pass1a_blur_v_ready = false;
    bool hdr_t1_consumes_rgba_128x90 = false;
};

enum class bloom_legacy_graph_result : std::uint8_t {
    exact_construction = 0,
    missing_resource,
    first_pass04_role_mismatch,
    second_pass04_role_mismatch,
    pre_brightpass_copy_edge_not_closed,
    downstream_graph_not_closed
};

inline bloom_legacy_graph_result validate_bloom_legacy_graph_carrier(
    const bloom_legacy_graph_carrier &c) noexcept
{
    if (!c.q8_scene_1024x720_ready || !c.packed_depth_256x180_ready ||
        !c.fixed_rgba_256x180_ready || !c.fixed_rgba_128x90_ready)
        return bloom_legacy_graph_result::missing_resource;
    if (c.first_pass04 != bloom_pass04_role::packed_depth_first)
        return bloom_legacy_graph_result::first_pass04_role_mismatch;
    if (c.second_pass04 !=
        bloom_pass04_role::pre_brightpass_color_copy_second)
        return bloom_legacy_graph_result::second_pass04_role_mismatch;
    if (!c.second_pass04_source_is_q8_scene ||
        !c.second_pass04_destination_is_rgba_256x180)
        return bloom_legacy_graph_result::pre_brightpass_copy_edge_not_closed;
    if (!c.pass18_brightpass_ready || !c.pass19_blur_h_ready ||
        !c.pass1a_blur_v_ready || !c.hdr_t1_consumes_rgba_128x90)
        return bloom_legacy_graph_result::downstream_graph_not_closed;
    return bloom_legacy_graph_result::exact_construction;
}

} // namespace dsrrl::operators::postprocess
