#include "dsrrl/runtime/generated_normal_routes_v12.hpp"
#include "dsrrl/runtime/generated_diffuse_routes_v12.hpp"
#include "dsrrl/runtime/generated_spec_routes_v12.hpp"
#include "dsrrl/runtime/generated_spec_material_routes.hpp"
#include "dsrrl/runtime/generated_ul_stable_hashes.hpp"
#include "dsrrl/runtime/material_owner_authorization.hpp"

#include <cstdint>

int main()
{
    using namespace dsrrl::runtime::generated;
    using dsrrl::runtime::assets::material_owner_authorization;
    using dsrrl::runtime::assets::material_owner_authorized;

    static_assert(!material_owner_authorized(material_owner_authorization{}));
    static_assert(material_owner_authorized(
        material_owner_authorization{true,false,false,false}));
    static_assert(material_owner_authorized(
        material_owner_authorization{false,true,false,false}));
    static_assert(!material_owner_authorized(
        material_owner_authorization{false,false,true,false}));
    static_assert(!material_owner_authorized(
        material_owner_authorization{false,false,false,true}));

    static_assert(diffuse_route_authorized(
        material_owner_authorization{false,false,true,false}));
    static_assert(!normal_route_authorized(
        material_owner_authorization{false,false,true,false}));
    static_assert(normal_route_authorized(
        material_owner_authorization{false,false,false,true}));
    static_assert(!diffuse_route_authorized(
        material_owner_authorization{false,false,false,true}));
    static_assert(diffuse_route_authorized(
        material_owner_authorization{true,false,false,false}));
    static_assert(normal_route_authorized(
        material_owner_authorization{false,true,false,false}));

    static_assert(k_normal_tuple_count_v12 == 557u);
    static_assert(k_normal_tuple_member_count_v12 == 1664u);
    static_assert(k_normal_target_count_v12 == 555u);
    static_assert(k_normal_safe_row_count_v12 == 3872u);

    static_assert(k_diffuse_pair_count_v12 == 528u);
    static_assert(k_diffuse_target_count_v12 == 524u);
    static_assert(k_diffuse_safe_row_count_v12 == 3446u);
    static_assert(k_spec_name_count_v12 == 769u);
    static_assert(k_spec_material_route_count == 34u);
    static_assert(k_ul_stable_hash_count == 24u);

    const auto &n0 = k_normal_tuples_v12.front();
    const auto &n1 = k_normal_tuples_v12.back();
    if (!normal_tuple_allowed_v12(n0.diffuse_hash, n0.spec_hash, n0.normal_hash) ||
        !normal_tuple_allowed_v12(n1.diffuse_hash, n1.spec_hash, n1.normal_hash) ||
        !normal_target_hash_allowed_v12(n0.normal_hash) ||
        normal_tuple_allowed_v12(0u, 0u, 0u) ||
        normal_target_hash_allowed_v12(0u))
        return 1;

    const auto &d0 = k_diffuse_pairs_v12.front();
    const auto &d1 = k_diffuse_pairs_v12.back();
    if (!diffuse_pair_allowed_v12(d0.spec_hash, d0.diffuse_hash) ||
        !diffuse_pair_allowed_v12(d1.spec_hash, d1.diffuse_hash) ||
        !diffuse_target_hash_allowed_v12(d0.diffuse_hash) ||
        diffuse_pair_allowed_v12(0u, 0u) ||
        diffuse_target_hash_allowed_v12(0u))
        return 2;

    const auto spec0 = k_spec_name_hashes_v12.front();
    const auto spec1 = k_spec_name_hashes_v12.back();
    if (!spec_name_hash_allowed_v12(spec0) ||
        !spec_name_hash_allowed_v12(spec1) ||
        spec_name_hash_allowed_v12(0u))
        return 3;

    const auto &route0 = k_spec_material_routes.front();
    if (!spec_material_route_allowed(route0.sha256, route0.receivers[0]) ||
        spec_material_route_allowed(route0.sha256, 0u) ||
        spec_material_route_allowed("nope", route0.receivers[0]))
        return 4;

    if (k_ul_stable_hashes.front().v29.size() != 64u ||
        k_ul_stable_hashes.front().v29_ul.size() != 64u ||
        k_ul_stable_hashes.back().v29.size() != 64u ||
        k_ul_stable_hashes.back().v29_ul.size() != 64u)
        return 5;

    return 0;
}
