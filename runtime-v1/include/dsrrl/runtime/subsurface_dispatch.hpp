#pragma once
#include "dsrrl/operators/resource_bridges/subsurface_route.hpp"
#include <string_view>

namespace dsrrl::runtime {
// Exact shader identity is necessary but never sufficient to bypass SSS.
inline int subsurface_target(std::string_view shader_sha) noexcept
{
    for(const auto &route:operators::resource_bridges::k_subsurface_receiver_routes)
        if(route.dsr_receiver_sha256==shader_sha)
            return static_cast<int>(route.target_plain_receiver_id);
    return -1;
}
inline bool subsurface_dispatch_ready(int receiver, bool exact_body_material,
    bool feature_enabled, bool complete_surface_assets, bool spec_shader_ready) noexcept
{
    return receiver>=33 && receiver<=35 && exact_body_material && feature_enabled &&
        complete_surface_assets && spec_shader_ready;
}
} // namespace dsrrl::runtime
