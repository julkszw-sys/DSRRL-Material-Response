#pragma once
// V364: only originally witnessed DSR legacy EnvDiffuse names + SHA256,
// paired to exact original PTDE DDS donor texels extracted from retail archive.
// EnvSpec t12/t14 is NEVER a source of EnvDiffuse identity.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dsrrl::operators::env_spec {
struct exact_ptde_legacy_envdiffuse {
    const char *dsr_name;
    std::uint16_t selector;
    std::uint32_t edge;
    std::uint32_t bytes;
    std::array<std::uint8_t,32> dsr_creation_sha;
    std::array<std::uint8_t,32> ptde_asset_sha;
    const char *relative_path;
};
inline constexpr std::array<exact_ptde_legacy_envdiffuse,2>
k_v364_m18_legacy_envdiffuse = {{{entries}
}};
inline const exact_ptde_legacy_envdiffuse *v364_exact_legacy_diffuse(
    const char *name, const std::array<std::uint8_t,32> &dsr_sha) noexcept {
    if(name==nullptr)return nullptr;
    for(const auto &entry:k_v364_m18_legacy_envdiffuse)
        if(entry.dsr_creation_sha==dsr_sha &&
           std::strcmp(name,entry.dsr_name)==0)
            return &entry;
    return nullptr;
}
inline const exact_ptde_legacy_envdiffuse *v364_legacy_by_selector(
    std::uint16_t selector) noexcept {
    for(const auto &entry:k_v364_m18_legacy_envdiffuse)
        if(entry.selector==selector)return &entry;
    return nullptr;
}
} // namespace dsrrl::operators::env_spec
