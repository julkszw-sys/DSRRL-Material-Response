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
            dsrrl::core::operator_bit(
                dsrrl::core::operator_id::diffuse_material_domain) |
            dsrrl::core::operator_bit(
                dsrrl::core::operator_id::pointlight_pnts_attenuation) |
            dsrrl::core::operator_bit(
                dsrrl::core::operator_id::terminal_sat_rgb);

        if ((expected & common) != common ||
            (plan.spc &&
             (expected & dsrrl::core::operator_bit(
                 dsrrl::core::operator_id::envspec_nospc_delete)) != 0u) ||
            (!plan.spc &&
             (expected & dsrrl::core::operator_bit(
                 dsrrl::core::operator_id::envspec_nospc_delete)) == 0u))
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

        std::uint32_t legacy_c100 = 0u;
        std::uint32_t legacy_c101 = 0u;
        std::uint32_t legacy_c102 = 0u;
        for (std::uint32_t i=0u;
             i<plan.op_count;
             ++i) {
            const auto &op =
                generated::k_clustered_pnts_journal_ops_v1[
                    plan.first_op+i];
            for (std::uint32_t n=0u;
                 n+2u<op.new_count;
                 ++n) {
                const auto at = op.new_offset+n;
                const auto token =
                    generated::k_clustered_pnts_journal_tokens_v1[at];
                const auto slot =
                    generated::k_clustered_pnts_journal_tokens_v1[at+1u];
                const auto row =
                    generated::k_clustered_pnts_journal_tokens_v1[at+2u];
                if (slot != 12u)
                    continue;
                if (token == 0x00208396u && row == 1u)
                    ++legacy_c100;
                else if (token == 0x00208246u && row == 0u)
                    ++legacy_c101;
                else if (token == 0x0020803au && row == 0u)
                    ++legacy_c102;
            }
        }

        if (legacy_c100 != 1u ||
            legacy_c101 != (plan.spc ? 1u : 0u) ||
            legacy_c102 != (plan.spc ? 1u : 0u))
            return 8;
    }

    {
        std::vector<std::uint32_t> words{
            0u,0u,
            0x00208246u,12u,0u,
            0xdeadbeefu,
            0x0020803au,12u,0u,
            0xcafebabeu,
            0x00208396u,12u,1u
        };
        if (!migrate_clustered_pnts_legacy_b12_words(
                words,
                true))
            return 9;
        if (words[4] != 2u ||
            words[6] != 0x0020803au ||
            words[10] != 0x00208246u)
            return 10;
    }

    {
        std::vector<std::uint32_t> words{
            0u,0u,
            0x00208396u,12u,1u
        };
        if (!migrate_clustered_pnts_legacy_b12_words(
                words,
                false) ||
            words[2] != 0x00208246u)
            return 11;
    }

    {
        std::vector<std::uint32_t> words{
            0u,0u,
            0x00208396u,12u,1u,
            0xffffffffu,
            0x00208396u,12u,1u
        };
        if (migrate_clustered_pnts_legacy_b12_words(
                words,
                false))
            return 12;
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
        "exact composed shader ownership + current b12 migration 24 Spc + 12 NoSpc\n";
    return 0;
}
