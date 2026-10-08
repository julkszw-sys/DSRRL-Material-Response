#pragma once

#include <cstdint>

#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"

namespace dsrrl::runtime {

// Runtime v2 producer state: exact selector publication owns the semantic
// lifetime. Consumers read a generation-stamped immutable value and never
// dereference LightBank engine pointers.
void pmetal_producer_state_clear() noexcept;

// Marks one exact P_Metal selector opportunity as pending. Any synchronized
// fallback from an older selector for the same exact material is invalid until
// the matching publish succeeds.
void pmetal_producer_state_begin(
    const operators::material_response::material_identity &material,
    std::uint64_t epoch) noexcept;

void pmetal_producer_state_publish(
    const operators::material_response::material_identity &material,
    const pmetal_envspec_source &source,
    std::uint64_t epoch) noexcept;

// Structural, observation-only reason returned with the same exact material
// query. Never grants permission to consume a stale or foreign source.
enum class pmetal_producer_lookup_reason : std::uint8_t {
    tls_hit = 0u,
    synchronized_hit = 1u,
    no_material_bucket = 2u,
    wrong_material_bucket = 3u,
    source_revoked = 4u,
    epoch_mismatch = 5u,
    record_identity_mismatch = 6u
};

bool pmetal_producer_state_latest(
    const operators::material_response::material_identity &material,
    std::uint64_t epoch,
    pmetal_envspec_source &out,
    pmetal_producer_lookup_reason *diagnostic_reason = nullptr) noexcept;

bool pmetal_producer_state_valid() noexcept;

} // namespace dsrrl::runtime
