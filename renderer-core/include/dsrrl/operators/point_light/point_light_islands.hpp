#pragma once

#include "dsrrl/operators/known_operator_island.hpp"
#include "dsrrl/operators/point_light/pnts_attenuation.hpp"

namespace dsrrl::operators::point_light {

inline constexpr known_operator_island full{core::operator_id::point_light};
inline constexpr known_operator_island pnts_attenuation{core::operator_id::pointlight_pnts_attenuation};
inline constexpr known_operator_island local_specular{core::operator_id::local_specular_legacy};

} // namespace dsrrl::operators::point_light
