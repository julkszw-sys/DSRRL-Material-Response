#include "dsrrl/operators/point_light/fixed_local_geometry_contract.hpp"
#include <array>
#include <cstdint>
#include <iostream>
using namespace dsrrl::operators::point_light;
int main(){
 auto a=attest_fixed_local_geometry_contract(nullptr,0u);
 if(a.result!=fixed_local_geometry_result::pass_not_fixed_local_specular) return 1;
 std::array<std::uint8_t,64> b{};
 a=attest_fixed_local_geometry_contract(b.data(),b.size());
 if(a.result!=fixed_local_geometry_result::pass_not_fixed_local_specular) return 2;
 std::cout<<"fixed_local_geometry_contract_tests: PASS\n"; return 0;
}
