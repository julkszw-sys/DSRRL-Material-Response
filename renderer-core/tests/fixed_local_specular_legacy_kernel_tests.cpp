#include "dsrrl/operators/point_light/fixed_local_specular_legacy_kernel.hpp"
#include <iostream>
using namespace dsrrl::operators::point_light;
int main(){
 fixed_local_specular_light_operands in{};
 in.normal.token={{0x00100246u,5u}}; in.view.token={{0x00100246u,6u}}; in.light.token={{0x00100246u,10u}};
 in.primary_scalar_dst.token={{0x00100042u,3u}}; in.auxiliary_scalar_dst.token={{0x00100082u,3u}}; in.ndotl_scalar_dst.token={{0x00100082u,4u}};
 fixed_local_specular_legacy_kernel out{};
 if(emit_fixed_local_specular_legacy_kernel(in,out)!=fixed_local_specular_legacy_kernel_result::exact) return 1;
 if(out.words.size()!=69u || out.result_temp_register!=3u || out.result_component!=2u) return 2;
 if(out.words[0]!=0x07002010u || out.words[44]!=0x07000034u) return 3;
 if(out.words[61]!=0x0020803au || out.words[62]!=12u || out.words[63]!=0u) return 4;
 std::size_t cursor=0u, instructions=0u;
 while(cursor<out.words.size()){
  const auto length=(out.words[cursor]>>24u)&0x7fu;
  if(length==0u || length>out.words.size()-cursor) return 5;
  cursor+=length; ++instructions;
 }
 if(cursor!=out.words.size() || instructions!=10u) return 6;
 std::cout<<"fixed_local_specular_legacy_kernel_tests: PASS\n"; return 0;
}
