#pragma once

#include "dsrrl/operators/known_operator_island.hpp"

namespace dsrrl::operators::postprocess {

inline constexpr known_operator_island bloom{core::operator_id::post_bloom};
inline constexpr known_operator_island hdr{core::operator_id::post_hdr};

} // namespace dsrrl::operators::postprocess
