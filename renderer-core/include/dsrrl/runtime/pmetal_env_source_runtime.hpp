#pragma once

#include <array>
#include <cstdint>
#include "dsrrl/operators/material_response/material_response_island.hpp"

namespace dsrrl::runtime {

struct pmetal_envspec_source {
    std::array<float,3> a{};
    std::array<float,3> b{};
    // Diagnostic EnvDiffuse carrier captured at the exact DSR profile-packer
    // output before the independent per-draw gain. XYZ is inverse-translated
    // from the DSR q=p^2.2 producer back to the linear PTDE source domain.
    std::array<float,3> envdiffuse_a{};
    std::array<float,3> envdiffuse_b{};
    bool envdiffuse_linear_valid = false;
    float beta = 0.0f;
    // PTDE PHN terminal scene encoding k135=c135.x/c135.y. The carrier is
    // explicit even before producer RE is complete so consumers do not regress
    // to a hidden hardcoded gain. Invalid means publish unity/fail-open.
    float phn_k135 = 1.0f;
    bool phn_k135_valid = false;
    std::uint64_t bank_signature_a = 0u;
    std::uint64_t bank_signature_b = 0u;
    std::uint32_t row_id_a = 0u;
    std::uint32_t row_id_b = 0u;
    std::uint64_t serial = 0u;
    // Runtime v2 semantic generation. This increments only when the exact
    // producer payload/material identity changes, so upload consumers can
    // skip redundant GPU writes without draw-time reclassification.
    std::uint64_t generation = 0u;
    // Diagnostic provenance only: unkeyed V13 hook snapshot was consumed by
    // an opt-in SPC material. Never treat this flag as material authorization.
    bool unkeyed_hook_fallback = false;
};

struct pmetal_env_source_runtime_telemetry {
    std::uint64_t steady_seen = 0u;
    std::uint64_t blend_seen = 0u;
    std::uint64_t exact_publish = 0u;
    std::uint64_t bank_unknown = 0u;
    std::uint64_t decode_fail = 0u;
    std::uint64_t publish_busy_drop = 0u;
    std::uint64_t consumer_ok = 0u;
    std::uint64_t consumer_fail = 0u;
    std::uint64_t endpoint_cache_hit = 0u;
    std::uint64_t endpoint_cache_miss = 0u;
    std::uint64_t endpoint_cache_fill = 0u;
    std::uint64_t region_cache_hit = 0u;
    std::uint64_t region_cache_miss = 0u;
    std::uint64_t cache_generation = 0u;
    std::uint64_t hook_single_seen = 0u;
    std::uint64_t hook_blend_seen = 0u;
    std::uint64_t hook_publish = 0u;
    std::uint64_t hook_consume = 0u;
    std::uint32_t hook_decode_stage = 0u;
    std::uint32_t hook_decode_version = 0u;
    std::uint32_t hook_decode_count = 0u;
    std::uint32_t hook_decode_index = 0u;
    std::uint32_t hook_decode_row_id = 0u;
    std::uint64_t hook_decode_signature = 0u;
    // Structural LightBank identity frontier. Codes are diagnostic only:
    // 0 none, 1 header-range, 2 header-semantic, 3 table-range,
    // 4 layout-unknown, 9 success.
    std::uint64_t bank_signature_scan_count = 0u;
    std::uint32_t bank_signature_attempt = 0u;
    std::uint32_t bank_signature_stage = 0u;
    std::uint32_t bank_signature_entry = 0u;
    std::uint32_t bank_signature_name_offset = 0u;
    std::uint32_t bank_signature_consumed = 0u;
    // Exact vanilla-DSR table-layout identity. This is producer identity
    // machinery only; downstream donor state keeps the canonical V13 signature.
    std::uint64_t bank_layout_signature = 0u;
    // Diagnostic-only thread provenance. These fields never authorize source
    // reuse; they exist only to falsify the selector-thread TLS lifetime.
    std::uint32_t last_publish_tid = 0u;
    std::uint32_t last_consumer_tid = 0u;
    bool last_consumer_local_valid = false;
    // Monotonic diagnostic frontier for the exact FLVER-selector source path.
    // These are observations only and never authorize a donor.
    bool selector_exact_seen = false;
    bool parent_gate_ok = false;
    bool descriptor_gate_ok = false;
    bool endpoint_gate_ok = false;
    bool manager_gate_ok = false;
    bool source_a_decode_ok = false;
    bool source_b_decode_ok = false;
    bool selector_carrier_active = false;
    bool steady_carrier_active = false;
    bool blend_carrier_active = false;
    bool hook_single_armed = false;
    bool hook_blend_armed = false;
    bool quarantined = false;
    bool restore_failed = false;
};

void pmetal_env_source_selector_clear() noexcept;
void pmetal_env_source_cache_invalidate() noexcept;
void pmetal_env_source_selector_event(
    void *owner, void *return_address, void *r14, void *r15, const void *selector_stack,
    const operators::material_response::material_identity &material) noexcept;

class pmetal_env_source_runtime {
public:
    bool install() noexcept;
    void uninstall() noexcept;
    bool latest(const operators::material_response::material_identity &material, pmetal_envspec_source &out) const noexcept;
    // Exact producer first. SPC opt-in can additionally consume the same
    // unkeyed V13 latest_hook_source() used by baseline P_Metal.
    // The fallback is diagnostic and may inherit another source.
    bool latest_exact_material(const operators::material_response::material_identity &material, pmetal_envspec_source &out) const noexcept;
    pmetal_env_source_runtime_telemetry telemetry() const noexcept;
    void reset() noexcept;
};

} // namespace dsrrl::runtime
