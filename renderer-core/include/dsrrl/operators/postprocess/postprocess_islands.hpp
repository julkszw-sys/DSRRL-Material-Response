#pragma once

#include "dsrrl/operators/known_operator_island.hpp"
#include "dsrrl/operators/postprocess/future_runtime_preflight.hpp"

namespace dsrrl::operators::postprocess {

inline constexpr known_operator_island bloom{core::operator_id::post_bloom};
inline constexpr known_operator_island hdr{core::operator_id::post_hdr};

} // namespace dsrrl::operators::postprocess
