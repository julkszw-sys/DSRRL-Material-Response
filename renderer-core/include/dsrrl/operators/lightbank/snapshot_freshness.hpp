#pragma once

#include <cstdint>

namespace dsrrl::operators::lightbank {

// Canonical immutable LightBank snapshot freshness fingerprint.
//
// Owner identity and the assignment tuple are deliberately separate semantic
// requirements. Owner equality alone cannot make stale lighting current, and
// tuple equality alone cannot identify the render/shader-context owner.
struct lightbank_snapshot_fingerprint {
    std::uintptr_t owner = 0;
    std::uint16_t selector_a = 0;
    std::uint16_t selector_b = 0;
    std::uint32_t beta_bits = 0;
};

constexpr bool lightbank_assignment_tuple_matches(
    const lightbank_snapshot_fingerprint &a,
    const lightbank_snapshot_fingerprint &b) noexcept
{
    return a.selector_a == b.selector_a &&
           a.selector_b == b.selector_b &&
           a.beta_bits == b.beta_bits;
}

constexpr bool lightbank_snapshot_matches_draw(
    const lightbank_snapshot_fingerprint &snapshot,
    const lightbank_snapshot_fingerprint &draw) noexcept
{
    return snapshot.owner != 0u &&
           draw.owner != 0u &&
           snapshot.owner == draw.owner &&
           lightbank_assignment_tuple_matches(snapshot, draw);
}

} // namespace dsrrl::operators::lightbank
