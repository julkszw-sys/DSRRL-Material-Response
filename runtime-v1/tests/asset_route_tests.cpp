#include "dsrrl/runtime/generated_normal_routes_v12.hpp"
#include "dsrrl/runtime/generated_diffuse_routes_v12.hpp"
#include "dsrrl/runtime/generated_spec_routes_v12.hpp"

#include <cstdint>

int main()
{
    using namespace dsrrl::runtime::generated;

    static_assert(k_normal_tuple_count_v12 == 557u);
    static_assert(k_normal_tuple_member_count_v12 == 1664u);
    static_assert(k_normal_target_count_v12 == 555u);
    static_assert(k_normal_safe_row_count_v12 == 3872u);

    static_assert(k_diffuse_pair_count_v12 == 528u);
    static_assert(k_diffuse_target_count_v12 == 524u);
    static_assert(k_diffuse_safe_row_count_v12 == 3446u);
    static_assert(k_spec_name_count_v12 == 769u);

    if (!spec_name_hash_allowed_v12(k_spec_name_hashes_v12.front()) ||
        !spec_name_hash_allowed_v12(k_spec_name_hashes_v12.back()) ||
        spec_name_hash_allowed_v12(0u))
        return 3;

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

    return 0;
}
