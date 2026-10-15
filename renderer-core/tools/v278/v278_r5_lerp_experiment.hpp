#pragma once
// V278 experimental A/B test: re-use the Stable R5 PTDE N/T/B operator on
// already exactly authenticated Lerp RX33-35, before draw-time routing.
// Source changes are transient and discarded by nested one-addon builder.
#include <cstdint>
#include <vector>
namespace dsrrl::operators::env_spec {
bool v278_apply_existing_stable_r5_on_exact_lerp(
    std::vector<std::uint8_t> &payload) noexcept;
}
