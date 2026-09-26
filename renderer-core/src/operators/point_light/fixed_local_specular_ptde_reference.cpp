#include "dsrrl/operators/point_light/fixed_local_specular_ptde_reference.hpp"
#include "dsrrl/operators/point_light/generated_fixed_local_specular_ptde_reference_v1.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

namespace dsrrl::operators::point_light {

bool fixed_local_specular_ptde_reference_for_digest(
    const std::array<std::uint8_t,32> &dsr_sha256,
    std::size_t dsr_size,
    fixed_local_specular_ptde_profile profile,
    fixed_local_specular_ptde_reference &reference) noexcept
{
    reference = {};
    const generated::fixed_ptde_reference_record *hit = nullptr;
    for (const auto &row : generated::k_fixed_ptde_references) {
        if (row.dsr_size != dsr_size || row.dsr_sha256 != dsr_sha256)
            continue;
        if (hit != nullptr)
            return false;
        hit = &row;
    }
    if (hit == nullptr)
        return false;

    reference.light_count = hit->light_count;
    if (profile == fixed_local_specular_ptde_profile::hemenv) {
        reference.ptde_sha256 = hit->ptde_hemenv_sha256;
        reference.ptde_size = hit->ptde_hemenv_size;
    } else if (profile == fixed_local_specular_ptde_profile::hemenvlerp) {
        reference.ptde_sha256 = hit->ptde_hemenvlerp_sha256;
        reference.ptde_size = hit->ptde_hemenvlerp_size;
    } else {
        reference = {};
        return false;
    }
    return reference.ptde_size != 0u;
}

bool fixed_local_specular_ptde_reference_for_shader(
    const void *pixel_shader_code,
    std::size_t code_size,
    fixed_local_specular_ptde_profile profile,
    fixed_local_specular_ptde_reference &reference) noexcept
{
    reference = {};
    if (pixel_shader_code == nullptr || code_size == 0u)
        return false;
    const auto digest =
        legacy_plan::hashing::sha256(
            static_cast<const std::uint8_t *>(pixel_shader_code),
            code_size);
    return fixed_local_specular_ptde_reference_for_digest(
        digest, code_size, profile, reference);
}

std::size_t fixed_local_specular_ptde_reference_count() noexcept
{
    return generated::k_fixed_ptde_references.size();
}

} // namespace dsrrl::operators::point_light
