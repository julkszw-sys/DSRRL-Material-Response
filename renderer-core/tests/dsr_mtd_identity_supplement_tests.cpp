#include "dsrrl/operators/material_response/generated_dsr_mtd_identity_supplement_v1.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"

#include <array>
#include <iostream>
#include <cstdint>

#define CHECK(x) do { if (!(x)) { std::cerr << "CHECK failed: " #x "\n"; return 1; } } while(false)

int main()
{
    namespace mr = dsrrl::operators::material_response;
    namespace gen = dsrrl::operators::material_response::generated;

    static_assert(!gen::k_dsr_mtd_identity_supplement_source_complete);
    static_assert(gen::k_dsr_mtd_identity_supplement.size() == 26u);

    std::array<std::uint8_t,32> resolved{};
    const auto semantic =
        mr::mtd_semantic_hash("Ps_Body[DSBT].mtd");

    CHECK(gen::dsr_mtd_identity_supplement_resolve(
        semantic,
        resolved));

    constexpr std::array<std::uint8_t,32> expected = {
        0x27,0x06,0x08,0x0f,0x2b,0x30,0x62,0x45,
        0xb9,0x0b,0xf9,0xd3,0xfd,0xfa,0x38,0x62,
        0x4b,0x2e,0xd0,0x45,0x2a,0x9b,0x1a,0xff,
        0x83,0x0c,0xf5,0x9d,0xe6,0x9b,0x37,0xd4
    };
    CHECK(resolved == expected);

    std::array<std::uint8_t,32> nospc{};
    CHECK(gen::dsr_mtd_identity_supplement_resolve(
        mr::mtd_semantic_hash("A10_Sky[Dn]_LS.mtd"),
        nospc));

    constexpr std::array<std::uint8_t,32> expected_nospc = {
        0x9e,0x15,0xf7,0x56,0x9b,0x6e,0x68,0x24,
        0x5e,0x4b,0xca,0xe5,0x6c,0xf6,0x20,0x4d,
        0x5c,0x27,0x84,0xcb,0xee,0x13,0x2c,0xa7,
        0xf9,0xda,0x9a,0xa9,0x37,0x51,0x69,0x0f
    };
    CHECK(nospc == expected_nospc);

    std::array<std::uint8_t,32> unknown{};
    CHECK(!gen::dsr_mtd_identity_supplement_resolve(
        mr::mtd_semantic_hash("Not_Certified.mtd"),
        unknown));

    return 0;
}
