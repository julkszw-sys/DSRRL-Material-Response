#include "dsrrl/operators/point_light/clustered_pnts_direct_materializer.hpp"
#include "dsrrl/operators/point_light/generated_clustered_pnts_direct_v1.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
    using namespace dsrrl::operators::point_light;

    if (generated::k_clustered_pnts_plans_v1.size() != 36u)
        return 1;

    for (const auto &plan : generated::k_clustered_pnts_plans_v1) {
        if (plan.original_sha256 == nullptr ||
            plan.replacement_sha256 == nullptr ||
            plan.stock_size == 0u ||
            plan.replacement_size == 0u ||
            plan.op_count != 24u)
            return 2;

        if (plan.first_op >
                generated::k_clustered_pnts_journal_ops_v1.size() ||
            plan.op_count >
                generated::k_clustered_pnts_journal_ops_v1.size() -
                    plan.first_op)
            return 3;
    }

    std::vector<std::uint8_t> output;
    const auto empty =
        materialize_clustered_pnts_direct_ptde(
            nullptr,
            0u,
            output);
    if (empty.result !=
            clustered_pnts_direct_materialize_result::
                pass_not_candidate ||
        !output.empty())
        return 4;

    std::array<std::uint8_t,64> unrelated{};
    const auto unknown =
        materialize_clustered_pnts_direct_ptde(
            unrelated.data(),
            unrelated.size(),
            output);
    if (unknown.result !=
            clustered_pnts_direct_materialize_result::
                pass_not_candidate ||
        !output.empty())
        return 5;

    std::cout <<
        "clustered_pnts_direct_materializer_tests: PASS 36 plans\n";
    return 0;
}
