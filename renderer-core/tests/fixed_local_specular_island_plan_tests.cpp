#include "dsrrl/operators/point_light/fixed_local_specular_island_plan.hpp"

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
    auto plan=build_fixed_local_specular_island_plan(nullptr,0u);
    CHECK(plan.result==
        fixed_local_specular_island_plan_result::
            pass_not_fixed_local_specular);
    CHECK(!plan.replace_complete_local_specular);

    std::array<std::uint8_t,64> unrelated{};
    plan=build_fixed_local_specular_island_plan(
        unrelated.data(),unrelated.size());
    CHECK(plan.result==
        fixed_local_specular_island_plan_result::
            pass_not_fixed_local_specular);
    CHECK(!plan.stock_microfacet_dead_at_output_cut);

    std::cout<<"fixed_local_specular_island_plan_tests: PASS\n";
    return 0;
}
