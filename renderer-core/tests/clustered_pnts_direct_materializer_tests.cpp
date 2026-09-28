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

    std::uint32_t spc_plans = 0u;
    std::uint32_t nospc_plans = 0u;

    for (const auto &plan : generated::k_clustered_pnts_plans_v1) {
        if (plan.spc)
            ++spc_plans;
        else
            ++nospc_plans;

        const auto expected =
            clustered_pnts_required_composed_shader_owners(
                plan.spc);
        const auto common =
            core::operator_bit(
                core::operator_id::diffuse_material_domain) |
            core::operator_bit(
                core::operator_id::pointlight_pnts_attenuation) |
            core::operator_bit(
                core::operator_id::terminal_sat_rgb);

        if ((expected & common) != common ||
            (plan.spc &&
             (expected & core::operator_bit(
                 core::operator_id::envspec_nospc_delete)) != 0u) ||
            (!plan.spc &&
             (expected & core::operator_bit(
                 core::operator_id::envspec_nospc_delete)) == 0u))
            return 2;

        if (plan.original_sha256 == nullptr ||
            plan.replacement_sha256 == nullptr ||
            plan.stock_size == 0u ||
            plan.replacement_size == 0u ||
            plan.op_count != 24u)
            return 3;

        if (plan.first_op >
                generated::k_clustered_pnts_journal_ops_v1.size() ||
            plan.op_count >
                generated::k_clustered_pnts_journal_ops_v1.size() -
                    plan.first_op)
            return 6;
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

    if (spc_plans != 24u ||
        nospc_plans != 12u)
        return 7;

    std::cout <<
        "clustered_pnts_direct_materializer_tests: PASS 36 plans, "
        "exact composed shader ownership 24 Spc + 12 NoSpc\n";
    return 0;
}
