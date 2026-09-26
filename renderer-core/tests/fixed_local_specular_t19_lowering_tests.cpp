#include "dsrrl/operators/point_light/fixed_local_specular_t19_lowering.hpp"

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
    fixed_local_specular_t19_decl decl{};
    CHECK(emit_fixed_local_specular_t19_decl(decl)==
        fixed_local_specular_t19_emit_result::exact);
    constexpr std::array<std::uint32_t,4> expected_decl{{
        0x040000a2u,0x00107000u,19u,16u
    }};
    CHECK(decl.words==expected_decl);

    fixed_local_specular_t19_load load{};
    CHECK(emit_fixed_local_specular_t19_load(19u,3u,load)==
        fixed_local_specular_t19_emit_result::exact);
    constexpr std::array<std::uint32_t,11> expected_load{{
        0x8b0000a7u,0x80018302u,0x00199983u,
        0x001000f2u,19u,
        0x00004001u,3u,
        0x00004001u,0u,
        0x00107e46u,19u
    }};
    CHECK(load.words==expected_load);

    CHECK(emit_fixed_local_specular_t19_load(0u,4u,load)==
        fixed_local_specular_t19_emit_result::fail_invalid_light_ordinal);
    CHECK(emit_fixed_local_specular_t19_load(4096u,0u,load)==
        fixed_local_specular_t19_emit_result::fail_invalid_register);

    std::cout<<"fixed_local_specular_t19_lowering_tests: PASS\n";
    return 0;
}
