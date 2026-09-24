#pragma once

#include "dsrrl/operators/known_operator_island.hpp"
#include "dsrrl/operators/surface/terminal_sat_rgb_patch.hpp"
#include "dsrrl/operators/surface/diffuse_material_domain.hpp"
#include "dsrrl/operators/surface/fixed_postfog_identity.hpp"

namespace dsrrl::operators::surface {

inline constexpr known_operator_island diffuse_material_domain{core::operator_id::diffuse_material_domain};
inline constexpr known_operator_island terminal_sat_rgb{core::operator_id::terminal_sat_rgb};
inline constexpr known_operator_island terminal_sat_rgba{core::operator_id::terminal_sat_rgba};
inline constexpr known_operator_island fixed_postfog_identity{core::operator_id::fixed_postfog_identity};
inline constexpr known_operator_island faceeye_shadow_legacy{core::operator_id::faceeye_shadow_legacy};

} // namespace dsrrl::operators::surface
