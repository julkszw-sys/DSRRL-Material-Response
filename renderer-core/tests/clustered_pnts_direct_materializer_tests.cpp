#include "dsrrl/operators/point_light/clustered_pnts_direct_materializer.hpp"
#include "dsrrl/operators/point_light/generated_clustered_pnts_direct_v1.hpp"
#include "dsrrl/operators/lightbank/generated_upper_lower_phn_pnts_v1.hpp"

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

    std::size_t composed_pairs = 0u;
    for (const auto &clustered :
         generated::k_clustered_pnts_plans_v1) {
        const dsrrl::operators::lightbank::generated_pnts::
            upper_lower_phn_pnts_plan *ul = nullptr;

        for (const auto &candidate :
             dsrrl::operators::lightbank::generated_pnts::
                 k_upper_lower_phn_pnts_plans) {
            if (candidate.stock_size ==
                    clustered.stock_size &&
                candidate.stock_sha256 ==
                    clustered.original_sha256) {
                if (ul != nullptr)
                    return 6;
                ul = &candidate;
            }
        }
        if (ul == nullptr)
            return 7;

        const bool ul_spc =
            ul->stratum ==
            dsrrl::operators::lightbank::generated_pnts::
                upper_lower_phn_pnts_stratum::spc;
        if (ul_spc != clustered.spc ||
            ul->shader_index !=
                clustered.representative_shader_index)
            return 8;

        const std::array<std::uint32_t,3> slots{
            ul->u_slot_word,
            ul->d_slot_word_0,
            ul->d_slot_word_1
        };

        for (const auto slot : slots) {
            for (std::uint32_t i = 0u;
                 i < clustered.op_count;
                 ++i) {
                const auto &op =
                    generated::k_clustered_pnts_journal_ops_v1[
                        clustered.first_op + i];

                // Both operand words must survive the clustered journal.
                for (std::uint32_t word = slot;
                     word <= slot + 1u;
                     ++word) {
                    if (op.old_count != 0u &&
                        word >= op.start &&
                        word < op.end)
                        return 9;
                }
            }
        }

        ++composed_pairs;
    }
    if (composed_pairs != 36u)
        return 10;

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
        "clustered_pnts_direct_materializer_tests: PASS 36 plans + 36 exact U/L composition pairs\n";
    return 0;
}
