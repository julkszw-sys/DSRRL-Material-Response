#pragma once

#include "dsrrl/operators/known_operator_island.hpp"
#include "dsrrl/operators/lightbank/upper_lower.hpp"

namespace dsrrl::operators::lightbank {

inline constexpr known_operator_island upper_lower{core::operator_id::upper_lower};
inline constexpr known_operator_island hemdir3{core::operator_id::hemdir3};

} // namespace dsrrl::operators::lightbank
