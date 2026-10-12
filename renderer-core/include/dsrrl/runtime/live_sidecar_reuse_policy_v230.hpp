#pragma once
// V230: pure decision boundary for reusing live PTDE sidecar SRVs.
// Exact logical-name equality, native D3D11 device and complete required
// class coverage are mandatory; hash alone is never authorization.
// No new GPU lifetime: runtime cache entries are owned by stock resource
// views and are removed on destroy_resource_view / destroy_device.
#include <cstdint>
#include <string_view>

namespace dsrrl::runtime {
struct live_sidecar_reuse_candidate_v230 {
    std::uintptr_t native_device = 0u;
    std::wstring_view exact_logical_name{};
    bool has_specular = false;
    bool has_diffuse = false;
    bool has_normal = false;
};
struct live_sidecar_reuse_need_v230 {
    std::uintptr_t native_device = 0u;
    std::wstring_view exact_logical_name{};
    bool specular = false;
    bool diffuse = false;
    bool normal = false;
};
constexpr bool may_reuse_live_sidecar_v230(
    const live_sidecar_reuse_candidate_v230 &c,
    const live_sidecar_reuse_need_v230 &n) noexcept
{
    return c.native_device != 0u &&
           c.native_device == n.native_device &&
           !c.exact_logical_name.empty() &&
           c.exact_logical_name == n.exact_logical_name &&
           (n.specular || n.diffuse || n.normal) &&
           (!n.specular || c.has_specular) &&
           (!n.diffuse || c.has_diffuse) &&
           (!n.normal || c.has_normal);
}
} // namespace dsrrl::runtime
