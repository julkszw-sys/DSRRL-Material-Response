#pragma once

#include "dsrrl/operators/known_operator_island.hpp"

namespace dsrrl::operators::sfx {

inline constexpr known_operator_island native_boundary{core::operator_id::dsr_native_sfx};
inline constexpr known_operator_island inverse_tonemap{core::operator_id::dsr_sfx_inverse_tonemap};

} // namespace dsrrl::operators::sfx
