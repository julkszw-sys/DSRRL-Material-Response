#pragma once

#include <array>
#include <cstdint>
#include "dsrrl/operators/material_response/material_response_island.hpp"

namespace dsrrl::runtime {

struct pmetal_envspec_source {
    std::array<float,3> a{};
    std::array<float,3> b{};
    float beta = 0.0f;
    std::uint64_t bank_signature_a = 0u;
    std::uint64_t bank_signature_b = 0u;
    std::uint32_t row_id_a = 0u;
    std::uint32_t row_id_b = 0u;
    std::uint64_t serial = 0u;
    // Runtime v2 semantic generation. This increments only when the exact
    // producer payload/material identity changes, so upload consumers can
    // skip redundant GPU writes without draw-time reclassification.
    std::uint64_t generation = 0u;
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
    bool quarantined = false;
    bool restore_failed = false;
};

void pmetal_env_source_selector_clear() noexcept;
void pmetal_env_source_selector_event(
    void *owner, void *return_address, void *r14, void *r15, const void *selector_stack,
    const operators::material_response::material_identity &material) noexcept;

class pmetal_env_source_runtime {
public:
    bool install() noexcept;
    void uninstall() noexcept;
    bool latest(const operators::material_response::material_identity &material, pmetal_envspec_source &out) const noexcept;
    pmetal_env_source_runtime_telemetry telemetry() const noexcept;
    void reset() noexcept;
};

} // namespace dsrrl::runtime
