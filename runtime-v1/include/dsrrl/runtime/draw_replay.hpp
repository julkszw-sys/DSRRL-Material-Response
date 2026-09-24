#pragma once

#include <cstdint>

namespace dsrrl::runtime::mr {

enum class indexed_replay_kind : std::uint8_t {
    draw_indexed = 0,
    draw_indexed_instanced
};

// ReShade reports DrawIndexed and DrawIndexedInstanced through the same callback
// shape. Collapsing to DrawIndexed is semantics-preserving only for the single
// instance / StartInstanceLocation==0 case. In particular, instance_count==0
// must remain a zero-instance instanced draw and first_instance must never be
// discarded.
constexpr indexed_replay_kind choose_indexed_replay(
    std::uint32_t instance_count,
    std::uint32_t first_instance) noexcept
{
    return instance_count == 1u && first_instance == 0u
        ? indexed_replay_kind::draw_indexed
        : indexed_replay_kind::draw_indexed_instanced;
}

} // namespace dsrrl::runtime::mr
