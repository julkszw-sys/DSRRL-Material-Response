#pragma once

namespace dsrrl::runtime::assets {

// Positive material/resource replacement can be authorized by one of three
// independently proven routes:
//   1) an authenticated live FLVER/material owner tuple,
//   2) a separately certified exact-material route (body/P_Metal), or
//   3) an operator-specific pre-certified exact application-bound resource
//      tuple. Diffuse and Normal tuple authority are intentionally separate.
struct material_owner_authorization {
    bool owner_tuple_authenticated = false;
    bool independent_exact_material_route = false;
    bool diffuse_safe_exact_resource_tuple = false;
    bool normal_safe_exact_resource_tuple = false;
};

constexpr bool material_owner_authorized(
    const material_owner_authorization &authorization) noexcept
{
    return authorization.owner_tuple_authenticated ||
           authorization.independent_exact_material_route;
}

constexpr bool diffuse_route_authorized(
    const material_owner_authorization &authorization) noexcept
{
    return material_owner_authorized(authorization) ||
           authorization.diffuse_safe_exact_resource_tuple;
}

constexpr bool normal_route_authorized(
    const material_owner_authorization &authorization) noexcept
{
    return material_owner_authorized(authorization) ||
           authorization.normal_safe_exact_resource_tuple;
}

} // namespace dsrrl::runtime::assets
