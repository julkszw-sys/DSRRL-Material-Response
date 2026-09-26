#include "dsrrl/operators/point_light/fixed_local_specular_pow_lowering.hpp"

#include <array>
#include <cstdint>
#include <iostream>

using namespace dsrrl::operators::point_light;

namespace {
bool check(bool c,const char *e,int l)
{
    if(c) return true;
    std::cerr<<"CHECK FAILED line "<<l<<": "<<e<<'\n';
    return false;
}
#define CHECK(e) do { if(!check(static_cast<bool>(e),#e,__LINE__)) return 1; } while(false)
}

int main()
{
    fixed_local_specular_pow_lowering stock{};
    CHECK(emit_stock_dsr_water_specular_pow(
        0u,0u,stock)==
        fixed_local_specular_pow_emit_result::exact);

    constexpr std::array<std::uint32_t,26> expected_stock{{
        0x08000034u,
        0x00100012u,0x00000000u,
        0x0010000au,0x00000000u,
        0x00004001u,0x00000000u,
        0x0500002fu,
        0x00100012u,0x00000000u,
        0x0010000au,0x00000000u,
        0x08000038u,
        0x00100012u,0x00000000u,
        0x0010000au,0x00000000u,
        0x0020800au,0x00000000u,0x0000000bu,
        0x05000019u,
        0x00100012u,0x00000000u,
        0x0010000au,0x00000000u
    }};
    CHECK(stock.words==expected_stock);

    fixed_local_specular_pow_lowering ptde{};
    CHECK(emit_fixed_local_specular_ptde_pow(
        0u,0u,ptde)==
        fixed_local_specular_pow_emit_result::exact);

    auto expected_ptde=expected_stock;
    expected_ptde[17]=0x0020803au;
    expected_ptde[18]=0x0000000cu;
    expected_ptde[19]=0x00000000u;
    CHECK(ptde.words==expected_ptde);

    fixed_local_specular_pow_lowering z{};
    CHECK(emit_fixed_local_specular_ptde_pow(
        7u,2u,z)==
        fixed_local_specular_pow_emit_result::exact);
    CHECK(z.words[1]==0x00100042u);
    CHECK(z.words[2]==7u);
    CHECK(z.words[3]==0x0010002au);
    CHECK(z.words[4]==7u);
    CHECK(z.words[17]==0x0020803au);
    CHECK(z.words[18]==12u);
    CHECK(z.words[19]==0u);

    CHECK(emit_fixed_local_specular_ptde_pow(
        0u,4u,z)==
        fixed_local_specular_pow_emit_result::fail_invalid_component);

    std::cout<<"fixed_local_specular_pow_lowering_tests: PASS\n";
    return 0;
}
