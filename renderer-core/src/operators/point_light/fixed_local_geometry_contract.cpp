#include "dsrrl/operators/point_light/fixed_local_geometry_contract.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace dsrrl::operators::point_light {
namespace {
constexpr std::uint32_t k_shex=0x58454853u,k_shdr=0x52444853u;
constexpr std::uint16_t k_add=0u,k_dp3=16u,k_if=31u,k_lt=49u,k_custom=53u,k_sqrt=75u;
constexpr std::size_t k_max_words=8192u,k_max_ins=4096u;
struct ins{std::uint32_t s=0,e=0;std::uint16_t op=0;};
std::uint32_t rd(const std::uint8_t*p){std::uint32_t v=0;std::memcpy(&v,p,4);return v;}
bool words(const void*code,std::size_t size,std::array<std::uint32_t,k_max_words>&w,std::size_t&n){
 n=0;if(code==nullptr||size<36u||!legacy_plan::dxbc::checksum_container_valid(static_cast<const std::uint8_t*>(code),size))return false;
 auto*b=static_cast<const std::uint8_t*>(code);auto cc=rd(b+28u);if(cc==0u||cc>64u||32ull+4ull*cc>size)return false;
 bool hit=false;for(std::uint32_t i=0;i<cc;++i){auto off=static_cast<std::size_t>(rd(b+32u+4u*i));if(off>size||size-off<8u)return false;auto tag=rd(b+off);if(tag!=k_shex&&tag!=k_shdr)continue;if(hit)return false;hit=true;auto sz=static_cast<std::size_t>(rd(b+off+4u));if((sz&3u)||sz>size-off-8u)return false;n=sz/4u;if(n<3u||n>w.size())return false;std::memcpy(w.data(),b+off+8u,sz);if(w[1]!=n)return false;}return hit;
}
bool decode(const std::uint32_t*w,std::size_t n,std::array<ins,k_max_ins>&v,std::size_t&c){
 c=0;std::size_t at=2;while(at<n){if(c>=v.size())return false;auto op=static_cast<std::uint16_t>(w[at]&0x7ffu);std::uint32_t len=op==k_custom?(at+1<n?w[at+1]:0u):((w[at]>>24u)&0x7fu);if(len==0u||len>n-at)return false;v[c++]={static_cast<std::uint32_t>(at),static_cast<std::uint32_t>(at+len),op};at+=len;}return at==n;
}
}
fixed_local_geometry_contract attest_fixed_local_geometry_contract(const void*code,std::size_t size) noexcept{
 fixed_local_geometry_contract out;out.island=build_fixed_local_specular_island_plan(code,size);
 if(out.island.result==fixed_local_specular_island_plan_result::pass_not_fixed_local_specular){out.result=fixed_local_geometry_result::pass_not_fixed_local_specular;return out;}
 if(out.island.result!=fixed_local_specular_island_plan_result::ready){out.result=fixed_local_geometry_result::fail_island_plan;return out;}
 std::array<std::uint32_t,k_max_words>w{};std::size_t n=0;std::array<ins,k_max_ins>v{};std::size_t c=0;
 if(!words(code,size,w,n)||!decode(w.data(),n,v,c)){out.result=fixed_local_geometry_result::fail_invalid_dxbc;return out;}
 out.light_count=out.island.light_count;
 for(std::uint8_t l=0;l<out.light_count;++l){auto sw=out.island.output_cut.operands.plan.lights[l].microfacet_window.start_word;std::size_t j=c;for(std::size_t i=0;i<c;++i)if(v[i].s==sw){j=i;break;}
  if(j<4u||j>=c||v[j].op!=k_if||v[j-4].op!=k_add||v[j-3].op!=k_dp3||v[j-2].op!=k_sqrt||v[j-1].op!=k_lt){out.result=fixed_local_geometry_result::fail_prefix_shape;return out;}
  out.lights[l]={v[j-4].s,v[j-3].s,v[j-2].s,v[j-1].s,v[j].s};
 }
 out.result=fixed_local_geometry_result::exact;return out;
}
} // namespace dsrrl::operators::point_light
