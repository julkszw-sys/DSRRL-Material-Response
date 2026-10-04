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

bool pmetal_producer_state_latest(
    const operators::material_response::material_identity &material,
    std::uint64_t epoch,
    pmetal_envspec_source &out) noexcept;

bool pmetal_producer_state_valid() noexcept;

} // namespace dsrrl::runtime
