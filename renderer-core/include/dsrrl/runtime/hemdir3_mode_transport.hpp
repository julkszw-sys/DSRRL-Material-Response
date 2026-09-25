#pragma once

#include <cstdint>

namespace dsrrl::runtime::hemdir3_mode_transport {

struct telemetry {
    std::uint64_t selector_begin = 0;
    std::uint64_t incoming_mode2 = 0;
    std::uint64_t effective_observed = 0;
    std::uint64_t mode2_observed = 0;
    std::uint64_t snapshot_hits = 0;
    std::uint64_t snapshot_misses = 0;
    bool provenance_ok = false;
    bool lt5_hook_armed = false;
    bool selector_end_hook_armed = false;
    bool quarantined = false;
    bool restore_failed = false;
};

bool install() noexcept;
void uninstall() noexcept;

// Called at exact ordinary selector entry 0x14022BA20 before 0x140295F50.
// The effective-mode hooks below complete this draw-scoped semantic snapshot.
void selector_begin(
    std::uint32_t incoming_mode) noexcept;

// Read without consuming. The draw callback owns final consumption so all
// operator adapters see one coherent mode snapshot.
bool snapshot(
    std::uint32_t &effective_mode) noexcept;

void consume_draw_selection() noexcept;
telemetry status() noexcept;
void reset_stats() noexcept;

} // namespace dsrrl::runtime::hemdir3_mode_transport
