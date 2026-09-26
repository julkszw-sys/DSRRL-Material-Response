#include "dsrrl/operators/point_light/local_specular_receiver_registry.hpp"
#include "dsrrl/operators/point_light/generated_local_specular_receivers_v1.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

namespace dsrrl::operators::point_light {
namespace {

local_specular_receiver_class map_class(
    generated::local_specular_generated_class value) noexcept
{
    switch (value) {
    case generated::local_specular_generated_class::clustered_spc_pnts:
        return local_specular_receiver_class::clustered_spc_pnts;
    case generated::local_specular_generated_class::fixed_spc_pntss:
        return local_specular_receiver_class::fixed_spc_pntss;
    case generated::local_specular_generated_class::fixed_spc_pntssss:
        return local_specular_receiver_class::fixed_spc_pntssss;
    }
    return local_specular_receiver_class::unsupported;
}

} // namespace

bool local_specular_receiver_for_digest(
    const std::array<std::uint8_t,32> &sha256,
    std::size_t code_size,
    local_specular_receiver_identity &identity) noexcept
{
    identity = {};

    if (!generated::local_specular_candidate_size(code_size))
        return false;

    for (const auto &record : generated::k_local_specular_receivers) {
        if (record.code_size != code_size ||
            record.sha256 != sha256)
            continue;

        identity.receiver_class =
            map_class(record.receiver_class);
        identity.representative_shader_index =
            record.representative_shader_index;
        identity.alias_count = record.alias_count;
        return identity.receiver_class !=
            local_specular_receiver_class::unsupported;
    }

    return false;
}

bool local_specular_receiver_for_shader(
    const void *pixel_shader_code,
    std::size_t code_size,
    local_specular_receiver_identity &identity) noexcept
{
    identity = {};
    if (pixel_shader_code == nullptr ||
        code_size == 0u ||
        !generated::local_specular_candidate_size(code_size))
        return false;

    const auto digest =
        dsrrl::operators::legacy_plan::hashing::sha256(
            static_cast<const std::uint8_t *>(pixel_shader_code),
            code_size);

    return local_specular_receiver_for_digest(
        digest,
        code_size,
        identity);
}

std::size_t local_specular_receiver_count() noexcept
{
    return generated::k_local_specular_receivers.size();
}

} // namespace dsrrl::operators::point_light
