#pragma once

#include <array>
#include <cstdint>

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
    bool steady_carrier_active = false;
    bool blend_carrier_active = false;
    bool quarantined = false;
    bool restore_failed = false;
};

class pmetal_env_source_runtime {
public:
    bool install() noexcept;
    void uninstall() noexcept;
    bool latest(pmetal_envspec_source &out) const noexcept;
    pmetal_env_source_runtime_telemetry telemetry() const noexcept;
    void reset() noexcept;
};

} // namespace dsrrl::runtime
