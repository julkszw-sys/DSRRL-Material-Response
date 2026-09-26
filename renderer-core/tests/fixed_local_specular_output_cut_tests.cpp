#include "dsrrl/operators/point_light/fixed_local_specular_output_cut.hpp"

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
    auto out=locate_fixed_local_specular_output_cut(nullptr,0u);
    CHECK(out.result==
        fixed_local_specular_output_cut_result::pass_not_fixed_local_specular);

    std::array<std::uint8_t,64> unrelated{};
    out=locate_fixed_local_specular_output_cut(
        unrelated.data(),unrelated.size());
    CHECK(out.result==
        fixed_local_specular_output_cut_result::pass_not_fixed_local_specular);

    std::cout<<"fixed_local_specular_output_cut_tests: PASS\n";
    return 0;
}
