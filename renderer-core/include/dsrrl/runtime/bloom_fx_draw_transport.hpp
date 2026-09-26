#pragma once

#include <cstdint>

namespace dsrrl::runtime::bloom_fx_draw_transport {

enum class fx_draw_entity_kind : std::uint8_t {
    unknown = 0,
    particle,
    cluster
};

struct fx_draw_snapshot {
    std::uint64_t generation = 0;
    fx_draw_entity_kind kind = fx_draw_entity_kind::unknown;
    void *entity = nullptr;
    void *appearance_state = nullptr;
    void *appearance_source_primary = nullptr;
    void *appearance_source_secondary = nullptr;
    void *draw_context = nullptr;
    std::uint32_t mode_token = 0;
    std::uint32_t appearance_semantic_word = 0;
    bool exact_entity_vtable = false;
    bool exact_appearance_vtable = false;
    bool appearance_state_ready = false;
    bool source_links_ready = false;
    bool ready = false;
};

struct telemetry {
    bool provenance_ok = false;
    bool particle_hook_armed = false;
    bool cluster_hook_armed = false;
    bool restore_failed = false;
    bool quarantined = false;

    std::uint64_t particle_events = 0;
    std::uint64_t cluster_events = 0;
    std::uint64_t exact_entity_hits = 0;
    std::uint64_t entity_rejects = 0;
    std::uint64_t state_ready_hits = 0;
    std::uint64_t state_missing = 0;
    std::uint64_t exact_state_hits = 0;
    std::uint64_t state_vtable_rejects = 0;
    std::uint64_t source_links_ready = 0;
    std::uint64_t source_links_missing = 0;
    std::uint64_t snapshot_hits = 0;
    std::uint64_t snapshot_misses = 0;
};

// Diagnostic-only transport at the homologous FXHG draw-entity semantic cut.
//
// PTDE collector vfunc index 10 dispatches through Particle 0x556CF0 /
// Cluster 0x5576D0 and both delegate through entity+0x30 appearance state.
// Retail DSR preserves the same index-10 contract at RVA 0xFFCF30 / 0xFFDCE0
// and the same entity+0x30 appearance-state carrier. DSR Particle appearance
// state retains source links at +0x30/+0x38; Cluster retains them at
// +0x50/+0x58 and an additional semantic word at +0x60.
//
// This transport authenticates the exact retail executable indirectly through
// the already source-complete FLVER provenance gate, then byte-attests both
// hook sites and exact Particle/Cluster vtables. It observes identity/liveness
// only: it does not authorize WaterWaveSfx, Q8 writes, Bloom, or pixels.
bool install() noexcept;
void uninstall() noexcept;

bool snapshot(fx_draw_snapshot &out) noexcept;
void consume() noexcept;

telemetry status() noexcept;
void reset_stats() noexcept;

} // namespace dsrrl::runtime::bloom_fx_draw_transport
