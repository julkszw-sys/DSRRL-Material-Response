#pragma once

#include "dsrrl/operators/known_operator_island.hpp"
#include "dsrrl/operators/resource_bridges/spec_rgb_bridge.hpp"
#include "dsrrl/operators/resource_bridges/normal_bridge.hpp"
#include "dsrrl/operators/resource_bridges/subsurface_route.hpp"
#include "dsrrl/operators/resource_bridges/diffuse_bridge.hpp"

namespace dsrrl::operators::resource_bridges {

inline constexpr known_operator_island spec_rgb{core::operator_id::spec_rgb};
inline constexpr known_operator_island diffuse{core::operator_id::diffuse};
inline constexpr known_operator_island normal{core::operator_id::normal};
inline constexpr known_operator_island subsurface{core::operator_id::subsurface};
inline constexpr known_operator_island env_diffuse{core::operator_id::env_diffuse};

} // namespace dsrrl::operators::resource_bridges
