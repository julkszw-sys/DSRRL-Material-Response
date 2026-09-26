#pragma once

#include <cstdint>

namespace dsrrl::operators::resource_bridges {

// Construction proof for create-time replacement shader ownership. This is
// deliberately separate from operator activation and pixel equivalence.
enum class replacement_shader_lifecycle_result : std::uint8_t {
    closed = 0,
    persistent_code_storage_missing,
    init_identity_attestation_missing,
    pipeline_ownership_missing,
    bind_reachability_missing,
    pipeline_retirement_missing,
    device_retirement_missing
};

struct replacement_shader_lifecycle_carrier {
    bool persistent_code_storage = false;
    bool init_identity_attestation = false;
    bool pipeline_ownership = false;
    bool bind_reachability = false;
    bool pipeline_retirement = false;
    bool device_retirement = false;
};

inline replacement_shader_lifecycle_result
validate_replacement_shader_lifecycle(
    const replacement_shader_lifecycle_carrier &c) noexcept
{
    if (!c.persistent_code_storage)
        return replacement_shader_lifecycle_result::persistent_code_storage_missing;
    if (!c.init_identity_attestation)
        return replacement_shader_lifecycle_result::init_identity_attestation_missing;
    if (!c.pipeline_ownership)
        return replacement_shader_lifecycle_result::pipeline_ownership_missing;
    if (!c.bind_reachability)
        return replacement_shader_lifecycle_result::bind_reachability_missing;
    if (!c.pipeline_retirement)
        return replacement_shader_lifecycle_result::pipeline_retirement_missing;
    if (!c.device_retirement)
        return replacement_shader_lifecycle_result::device_retirement_missing;
    return replacement_shader_lifecycle_result::closed;
}

} // namespace dsrrl::operators::resource_bridges
