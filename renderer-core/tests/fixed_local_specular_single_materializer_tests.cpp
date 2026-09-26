#include "dsrrl/operators/point_light/fixed_local_specular_single_materializer.hpp"
#include "dsrrl/core/feature_registry.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace dsrrl;
int main(){
 core::feature_registry features;
 std::vector<std::uint8_t> output;
 auto r=operators::point_light::materialize_fixed_local_specular_single(
     features,nullptr,0u,output);
 if(r.result!=operators::point_light::fixed_local_single_materialize_result::
        pass_not_fixed_local_specular) return 1;
 if(!output.empty()) return 2;
 std::array<std::uint8_t,64> unrelated{};
 r=operators::point_light::materialize_fixed_local_specular_single(
     features,unrelated.data(),unrelated.size(),output);
 if(r.result!=operators::point_light::fixed_local_single_materialize_result::
        pass_not_fixed_local_specular) return 3;
 if(!output.empty()) return 4;
 std::cout<<"fixed_local_specular_single_materializer_tests: PASS\n";
 return 0;
}
