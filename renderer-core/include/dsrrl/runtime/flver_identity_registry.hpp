#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "dsrrl/runtime/material_owner_producer.hpp"

namespace dsrrl::runtime {

struct flver_identity_telemetry {
    std::uint64_t inserts = 0;
    std::uint64_t lookups = 0;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t tls_hits = 0;
    std::uint64_t mutex_fallbacks = 0;
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

bool flver_identity_enrich_owner(
    const void *selector_container,
    std::uint32_t material_slot,
    actual_material_owner_observation &observation) noexcept;
void flver_identity_reset() noexcept;
flver_identity_telemetry flver_identity_stats() noexcept;

// Monotonic invalidation epoch for positive selector-owner identity caches.
// New unrelated model insertion intentionally does not invalidate existing
// identities; replacement/destruction of an existing model advances it.
std::uint64_t flver_identity_epoch() noexcept;

// Validate a cached positive selector identity against FLVER lifetime changes
// for this exact selector container. Unrelated model destruction/replacement
// may advance the global epoch without invalidating this model; same-model
// invalidation, reset, journal overflow or a concurrent writer fail closed.
bool flver_identity_cache_epoch_survives(
    const void *selector_container,
    std::uint64_t cached_epoch,
    std::uint64_t &current_epoch) noexcept;

} // namespace dsrrl::runtime
