#pragma once

#include <cstdint>

namespace dsrrl::runtime::dof {

struct tonemap_handoff_telemetry {
    std::uint64_t pass13_calls = 0u;
    std::uint64_t dof_source_hits = 0u;
    std::uint64_t dof_source_misses = 0u;
    bool hook_ready = false;
};

bool register_tonemap_handoff_scope_runtime() noexcept;
void unregister_tonemap_handoff_scope_runtime() noexcept;

// True only inside the exact HDR/ToneMap executor at RVA 0x457E50 and only
// when its primary descriptor resource equals ImageState+0x104, the confirmed
// DoF output alias consumed by ToneMap pass 0x13.
bool inside_exact_tonemap_dof_handoff() noexcept;

tonemap_handoff_telemetry tonemap_handoff_status() noexcept;

} // namespace dsrrl::runtime::dof
