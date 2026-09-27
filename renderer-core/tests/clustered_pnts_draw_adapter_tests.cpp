#include "dsrrl/core/draw_transaction_policy.hpp"

#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << "CHECK failed: " #x "\n"; return 1; } } while(false)

int main()
{
    using namespace dsrrl;

    const auto point =
        core::operator_bit(core::operator_id::point_light);
    const auto local =
        core::operator_bit(core::operator_id::local_specular_legacy);
    const auto mr =
        core::operator_bit(core::operator_id::material_response);

    CHECK(core::draw_additional_owner_masks_valid(
        core::operator_id::local_specular_legacy,
        point | mr,
        point | mr,
        point | mr,
        point,
        0u));

    CHECK(core::draw_additional_owner_masks_valid(
        core::operator_id::point_light,
        mr,
        mr,
        mr,
        0u,
        0u));

    CHECK(!core::draw_additional_owner_masks_valid(
        core::operator_id::local_specular_legacy,
        point | mr | local,
        point | mr | local,
        point | mr | local,
        point | local,
        0u));

    CHECK(!core::draw_additional_owner_masks_valid(
        core::operator_id::point_light,
        point | mr,
        point | mr,
        point | mr,
        point,
        0u));

    CHECK(!core::draw_additional_owner_masks_valid(
        core::operator_id::point_light,
        mr,
        mr,
        mr,
        local,
        0u));

    CHECK(core::draw_policy(core::operator_id::point_light).mode ==
          core::draw_transaction_mode::draw_required);
    CHECK(core::draw_policy(core::operator_id::local_specular_legacy).mode ==
          core::draw_transaction_mode::draw_required);

    std::cout << "clustered_pnts_draw_adapter_tests: PASS\n";
    return 0;
}
