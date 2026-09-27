#include "dsrrl/operators/material_response/generated_dsr_mtd_identity_supplement_v1.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"

#include <array>
#include <cassert>
#include <cstdint>

int main()
{
    namespace mr = dsrrl::operators::material_response;
    namespace gen = dsrrl::operators::material_response::generated;

    static_assert(!gen::k_dsr_mtd_identity_supplement_source_complete);
    static_assert(gen::k_dsr_mtd_identity_supplement.size() == 1u);

    std::array<std::uint8_t,32> resolved{};
    const auto semantic =
        mr::mtd_semantic_hash("Ps_Body[DSBT].mtd");

    assert(gen::dsr_mtd_identity_supplement_resolve(
        semantic,
        resolved));

    constexpr std::array<std::uint8_t,32> expected = {
        0x27,0x06,0x08,0x0f,0x2b,0x30,0x62,0x45,
        0xb9,0x0b,0xf9,0xd3,0xfd,0xfa,0x38,0x62,
        0x4b,0x2e,0xd0,0x45,0x2a,0x9b,0x1a,0xff,
        0x83,0x0c,0xf5,0x9d,0xe6,0x9b,0x37,0xd4
    };
    assert(resolved == expected);

    std::array<std::uint8_t,32> unknown{};
    assert(!gen::dsr_mtd_identity_supplement_resolve(
        mr::mtd_semantic_hash("Not_Certified.mtd"),
        unknown));

    return 0;
}
