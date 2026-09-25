#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime {

struct flver_identity_telemetry {
    std::uint64_t inserts = 0;
    std::uint64_t lookups = 0;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t erases = 0;
    std::uint64_t invalid_raw = 0;
};

bool flver_identity_observe_parse(
    const void *model,
    const void *raw,
    std::size_t readable_bytes) noexcept;
void flver_identity_observe_destroy(const void *model) noexcept;
bool flver_identity_lookup(
    const void *selector_container,
    std::array<std::uint8_t, 32> &sha256) noexcept;
void flver_identity_reset() noexcept;
flver_identity_telemetry flver_identity_stats() noexcept;

} // namespace dsrrl::runtime
