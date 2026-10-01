#pragma once
#include <cstdint>
#include "dsrrl/operators/material_response/material_response_island.hpp"

namespace dsrrl::runtime::flver_identity_transport {

struct hook_status {
    bool provenance_ok = false;
    bool parser_armed = false;
    bool selector_armed = false;
    bool builder_armed = false;
    bool destructor_armed = false;
    bool mtd_armed = false;
    bool restore_failed = false;
    bool selector_owner_enrichment = false;
    bool exact_runtime_material_carrier = false;
    bool runtime_mtd_classified = false;
    bool runtime_mtd_cache_hit = false;
    bool runtime_mtd_selection_published = false;
    bool upper_lower_selector_enabled = false;
    bool hemdir3_selector_enabled = false;
};

// The clustered PointLight builder hook is independent from the FLVER
// parser/selector/MTD identity transport used by MR/P_Metal. Keep it optional
// so profiles without draw-time clustered PointLight do not detour the hot
// renderer builder at all.
bool install(
    bool enable_clustered_builder = true,
    bool enable_upper_lower_selector = true,
    bool enable_hemdir3_selector = true) noexcept;
void uninstall() noexcept;
hook_status status() noexcept;

struct selector_owner_telemetry {
    std::uint64_t selector_events = 0;
    std::uint64_t owner_sha_hits = 0;
    std::uint64_t owner_mtd_hits = 0;
    std::uint64_t selector_identity_cache_hits = 0;
    std::uint64_t selector_identity_cache_misses = 0;
    std::uint64_t exact_owner_ready = 0;
    std::uint64_t owner_fail_open = 0;
    std::uint64_t runtime_material_hits = 0;
    std::uint64_t runtime_material_ready = 0;
    std::uint64_t consumed = 0;
    std::uint64_t consume_misses = 0;
};

bool consume_selector_owner_candidate(
    operators::material_response::material_identity &material) noexcept;

selector_owner_telemetry selector_owner_stats() noexcept;

} // namespace dsrrl::runtime::flver_identity_transport
