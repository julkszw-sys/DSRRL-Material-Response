#pragma once

#include "dsrrl/operators/dof/dof_island.hpp"

#include <cstdint>

namespace dsrrl::runtime::dof {

struct plain_rate_telemetry {
    std::uint64_t preflight_ok = 0u;
    std::uint64_t preflight_fail = 0u;
    std::uint64_t activate_ok = 0u;
    std::uint64_t activate_fail = 0u;
    std::uint64_t restore_ok = 0u;
    std::uint64_t restore_fail = 0u;
    bool exact_preimage_verified = false;
    bool active = false;
};

bool register_plain_rate_runtime() noexcept;
void unregister_plain_rate_runtime() noexcept;

// Activation is deliberately coupled to the full operator contract.
// A caller cannot switch DSR 0xDF0 -> 0xDEF merely because the patch site
// exists; the exact receiver/resource/continuation gates must all pass.
bool activate_plain_rate_runtime(
    const operators::dof::activation_context &context) noexcept;

void deactivate_plain_rate_runtime() noexcept;
bool plain_rate_preflight_ready() noexcept;
plain_rate_telemetry plain_rate_status() noexcept;

} // namespace dsrrl::runtime::dof
