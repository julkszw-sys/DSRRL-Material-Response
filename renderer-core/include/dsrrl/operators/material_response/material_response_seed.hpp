#pragma once

#include "material_response_island.hpp"

#include <cstddef>

namespace dsrrl::operators::material_response {

std::size_t register_confirmed_material_routes_v1(material_response_island &island);
std::size_t register_confirmed_material_receivers_v1(material_response_island &island);

} // namespace dsrrl::operators::material_response
