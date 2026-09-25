#pragma once

namespace dsrrl::runtime::assets {

// Positive Diffuse/Normal resource replacement must be authorized either by
// an authenticated ordinary FLVER/material owner tuple, or by a separately
// certified exact-material route whose proof does not depend on that tuple
// corpus (currently body/P_Metal special routes only).
struct material_owner_authorization {
    bool owner_tuple_authenticated = false;
    bool independent_exact_material_route = false;
};

constexpr bool material_owner_authorized(
    const material_owner_authorization &authorization) noexcept
{
    return authorization.owner_tuple_authenticated ||
           authorization.independent_exact_material_route;
}

} // namespace dsrrl::runtime::assets
