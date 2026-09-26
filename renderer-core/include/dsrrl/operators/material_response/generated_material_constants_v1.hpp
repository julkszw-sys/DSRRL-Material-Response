#pragma once
#include <array>
#include <cstdint>

namespace dsrrl::operators::material_response::generated {

struct material_response_constants {
    std::uint32_t route_index;
    std::array<float,3> c100;
    std::array<float,3> c101_f0q;
    float ptde_specular_power;
    bool ptde_specular_power_verified;
};

inline constexpr std::array<material_response_constants,34> k_material_response_constants_v1 = {{
    {0u,{{0.5f,0.5f,0.5f}},{{1.20237927f,1.20237927f,1.20237927f}},4.0f,true},
    {1u,{{0.5f,0.5f,0.5f}},{{1.20237927f,1.20237927f,1.20237927f}},4.0f,true},
    {2u,{{0.5f,0.5f,0.5f}},{{1.51663761f,1.51663761f,1.51663761f}},8.5f,true},
    {3u,{{0.5f,0.5f,0.5f}},{{1.0f,1.0f,1.0f}},8.0f,true},
    {4u,{{0.5f,0.5f,0.5f}},{{1.51663761f,1.51663761f,1.51663761f}},8.5f,true},
    {5u,{{0.5f,0.5f,0.5f}},{{1.51663761f,1.51663761f,1.51663761f}},8.5f,true},
    {6u,{{0.5f,0.5f,0.5f}},{{1.0f,1.0f,1.0f}},2.0f,true},
    {7u,{{0.5f,0.5f,0.5f}},{{1.0f,1.0f,1.0f}},2.0f,true},
    {8u,{{0.5f,0.5f,0.5f}},{{1.37035098f,1.37035098f,1.37035098f}},60.0f,true},
    {9u,{{0.5f,0.5f,0.5f}},{{1.37035098f,1.37035098f,1.37035098f}},60.0f,true},
    {10u,{{0.5f,0.5f,0.5f}},{{1.0f,1.0f,1.0f}},2.0f,true},
    {11u,{{0.5f,0.5f,0.5f}},{{1.0f,1.0f,1.0f}},2.0f,true},
    {12u,{{0.5f,0.5f,0.5f}},{{1.20237927f,1.20237927f,1.20237927f}},4.0f,true},
    {13u,{{0.5f,0.5f,0.5f}},{{1.51663761f,1.51663761f,1.51663761f}},8.5f,true},
    {14u,{{0.5f,0.5f,0.5f}},{{1.0f,1.0f,1.0f}},2.0f,true},
    {15u,{{0.5f,0.5f,0.5f}},{{1.0f,1.0f,1.0f}},2.0f,true},
    {16u,{{0.5f,0.5f,0.5f}},{{1.37035098f,1.37035098f,1.37035098f}},15.0f,true},
    {17u,{{0.5f,0.5f,0.5f}},{{1.37035098f,1.37035098f,1.37035098f}},15.0f,true},
    {41u,{{0.5f,0.5f,0.5f}},{{1.20237927f,1.20237927f,1.20237927f}},4.0f,true},
    {44u,{{0.5f,0.5f,0.5f}},{{1.20237927f,1.20237927f,1.20237927f}},4.0f,true},
    {60u,{{0.5f,0.5f,0.5f}},{{1.0f,1.0f,1.0f}},2.0f,true},
    {88u,{{0.5f,0.5f,0.5f}},{{1.0f,1.0f,1.0f}},2.0f,true},
    {160u,{{0.5f,0.5f,0.5f}},{{1.0f,1.0f,1.0f}},2.0f,true},
    {167u,{{0.5f,0.5f,0.5f}},{{1.37035098f,1.37035098f,1.37035098f}},15.0f,true},
    {190u,{{1.5f,1.5f,1.5f}},{{1.37035098f,1.37035098f,1.37035098f}},15.0f,true},
    {197u,{{0.5f,0.5f,0.5f}},{{1.37035098f,1.37035098f,1.37035098f}},15.0f,true},
    {229u,{{0.5f,0.5f,0.5f}},{{1.51663761f,1.51663761f,1.51663761f}},8.5f,true},
    {231u,{{0.5f,0.5f,0.5f}},{{1.37035098f,1.37035098f,1.37035098f}},15.0f,true},
    {232u,{{0.5f,0.5f,0.5f}},{{1.0f,1.0f,1.0f}},8.0f,true},
    {276u,{{0.5f,0.5f,0.5f}},{{1.0f,1.0f,1.0f}},2.0f,true},
    {312u,{{0.5f,0.5f,0.5f}},{{1.0f,1.0f,1.0f}},8.0f,true},
    {331u,{{0.5f,0.5f,0.5f}},{{1.0f,1.0f,1.0f}},2.0f,true},
    {345u,{{0.5f,0.5f,0.5f}},{{1.51663761f,1.51663761f,1.51663761f}},8.5f,true},
    {359u,{{0.5f,0.5f,0.5f}},{{1.51663761f,1.51663761f,1.51663761f}},8.5f,true},
}};

constexpr const material_response_constants *find_material_response_constants(
    std::uint32_t route_index) noexcept
{
    const material_response_constants *hit = nullptr;
    for (const auto &row : k_material_response_constants_v1) {
        if (row.route_index != route_index)
            continue;
        if (hit != nullptr)
            return nullptr;
        hit = &row;
    }
    return hit;
}

} // namespace dsrrl::operators::material_response::generated
