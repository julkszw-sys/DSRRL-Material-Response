#include "dsrrl/runtime/generated_normal_routes_v12.hpp"
#include "dsrrl/runtime/generated_diffuse_routes_v12.hpp"

#include <cassert>
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

    const auto &n0 = k_normal_tuples_v12.front();
    const auto &n1 = k_normal_tuples_v12.back();
    assert(normal_tuple_allowed_v12(n0.diffuse_hash, n0.spec_hash, n0.normal_hash));
    assert(normal_tuple_allowed_v12(n1.diffuse_hash, n1.spec_hash, n1.normal_hash));
    assert(normal_target_hash_allowed_v12(n0.normal_hash));
    assert(!normal_tuple_allowed_v12(0u, 0u, 0u));
    assert(!normal_target_hash_allowed_v12(0u));

    const auto &d0 = k_diffuse_pairs_v12.front();
    const auto &d1 = k_diffuse_pairs_v12.back();
    assert(diffuse_pair_allowed_v12(d0.spec_hash, d0.diffuse_hash));
    assert(diffuse_pair_allowed_v12(d1.spec_hash, d1.diffuse_hash));
    assert(diffuse_target_hash_allowed_v12(d0.diffuse_hash));
    assert(!diffuse_pair_allowed_v12(0u, 0u));
    assert(!diffuse_target_hash_allowed_v12(0u));

    return 0;
}
