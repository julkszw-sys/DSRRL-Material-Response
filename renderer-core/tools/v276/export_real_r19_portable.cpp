// V276 portable offline R19 materializer runner.
// REQUIRES exact source revision and the same R19 CMake flag subset.
// Native GPU WARP acceptance is separately checked by Windows exporter.
// Game-owned RX input files are NEVER embedded or uploaded.
#include "dsrrl/core/feature_registry.hpp"
#include "dsrrl/core/types.hpp"
#include "dsrrl/operators/env_spec/pmetal_rgba_lerp_materializer.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace dsrrl;
namespace H=dsrrl::operators::legacy_plan::hashing;
namespace L=dsrrl::operators::legacy_plan::dxbc;
struct row{const char* sha;std::uint32_t rx;std::size_t fog,glow;};
constexpr row rows[]={
{"7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85",33,2552,2895},
{"857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db",34,2471,2814},
{"9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8",35,2124,2467}
};
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
static std::vector<std::uint8_t> read(const std::filesystem::path& p){
 std::ifstream f(p,std::ios::binary|std::ios::ate);
 check(bool(f),"input DXBC not found");const auto n=f.tellg();
 check(n>0 && n<1000000,"bad DXBC length");
 std::vector<std::uint8_t> b(static_cast<std::size_t>(n));
 f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(n));
 check(bool(f),"DXBC input truncated");return b;
}
static std::string hex(const H::sha256_digest& b){
 constexpr char digits[]="0123456789abcdef";
 std::string result;
 for(auto v:b){result+=digits[v>>4];result+=digits[v&0x0fu];}
 return result;
}
static std::vector<std::uint32_t> shex(const std::vector<std::uint8_t>& b){
 check(b.size()>=36 && std::memcmp(b.data(),"DXBC",4)==0 &&
   L::checksum_container_valid(b.data(),b.size()),"invalid original/final DXBC checksum");
 auto n=L::read_u32(b.data()+28);
 check(n>0 && n<64 && 32ull+4ull*n<=b.size(),"bad DXBC chunk count");
 std::vector<std::uint32_t> out;
 for(std::uint32_t k=0;k<n;k++){
  auto p=L::read_u32(b.data()+32+4*k);
  check(p+8<=b.size(),"bad chunk offset");
  auto z=L::read_u32(b.data()+p+4);
  check(z<=b.size()-p-8,"bad chunk length");
  if(std::memcmp(b.data()+p,"SHEX",4)==0){
   check(out.empty()&&z%4==0,"bad SHEX count");
   out.resize(z/4);
   for(std::size_t j=0;j<out.size();j++)
    out[j]=L::read_u32(b.data()+p+8+4*j);
  }
 }
 check(out.size()>2000 && out[1]==out.size(),"invalid SHEX token stream");
 return out;
}
static std::vector<std::size_t> starts(const std::vector<std::uint32_t>& w){
 std::vector<std::size_t> out;
 for(std::size_t i=2;i<w.size();){
  auto n=(w[i]>>24)&127u;
  check(n>0 && i+n<=w.size(),"bad opcode stream");
  out.push_back(i);i+=n;
 }
 return out;
}
static std::size_t retained(const std::vector<std::uint32_t>& a,
    const std::vector<std::uint32_t>& b,const std::vector<std::size_t>& starts,
    std::size_t off,std::size_t len){
 check(off+len<=a.size(),"original window overflow");
 std::size_t hit=0,count=0;
 for(auto k:starts)
  if(k+len<=b.size()&&std::equal(a.begin()+static_cast<std::ptrdiff_t>(off),
       a.begin()+static_cast<std::ptrdiff_t>(off+len),
       b.begin()+static_cast<std::ptrdiff_t>(k))){
    hit=k;++count;
  }
 check(count==1,"stock Fog/Glow no longer identical or unique");
 return hit;
}
static bool features(core::feature_registry& f){
 using core::operator_id;
 constexpr std::array<operator_id,12> enabled{{
  operator_id::material_response,operator_id::spec_rgb,
  operator_id::diffuse,operator_id::normal,operator_id::env_spec,
  operator_id::terminal_sat_rgb,operator_id::diffuse_material_domain,
  operator_id::point_light,operator_id::local_specular_legacy,
  operator_id::pointlight_pnts_attenuation,
  operator_id::envspec_nospc_delete,operator_id::fixed_postfog_identity}};
 for(auto op:enabled)if(!f.set(op,true))return false;
 return f.set(operator_id::upper_lower,false) &&
        f.set(operator_id::hemdir3,false) &&
        f.set(operator_id::subsurface,false);
}
int main(int argc,char** argv){
 try{
  check(argc==5,"usage: exporter original_rx33 original_rx34 original_rx35 local_dir");
  core::feature_registry f;check(features(f),"feature registry init failed");
  const std::filesystem::path dir(argv[4]);
  std::filesystem::create_directories(dir);
  for(std::size_t i=0;i<3;++i){
   auto donor=read(argv[i+1]);
   check(H::matches_hex(H::sha256(donor.data(),donor.size()),rows[i].sha),
         "original retail exact shader SHA mismatch");
   std::vector<std::uint8_t> result;
   const auto outcome=operators::env_spec::materialize_pmetal_rgba_lerp_receiver(
      f,donor.data(),donor.size(),result);
   check(outcome.result==operators::env_spec::
           pmetal_rgba_lerp_materialize_result::applied &&
         outcome.semantic_receiver_id==rows[i].rx && !result.empty(),
         "actual production R19 materializer rejected donor");
   auto a=shex(donor),b=shex(result),locations=starts(b);
   const auto fog=retained(a,b,locations,rows[i].fog,212);
   const auto glow=retained(a,b,locations,rows[i].glow,5);
   const auto alpha=retained(a,b,locations,rows[i].glow+10,5);
   check(fog+212<=glow && glow<alpha,"Fog/Glow order changed");
   const auto path=dir/(std::string("final_R19_RX")+std::to_string(rows[i].rx)+".dxbc");
   check(!std::filesystem::exists(path),"refuse overwrite of existing output");
   std::ofstream out(path,std::ios::binary);
   check(bool(out),"output file not writable");
   out.write(reinterpret_cast<const char*>(result.data()),
             static_cast<std::streamsize>(result.size()));
   check(bool(out),"output truncated");
   std::cout<<"V276_REAL_FINAL_MATERIALIZED_RX"<<rows[i].rx
    <<" final_sha256="<<hex(H::sha256(result.data(),result.size()))
    <<" original_sha256="<<rows[i].sha
    <<" final_bytes="<<result.size()
    <<" fog_word="<<fog<<" glow_word="<<glow
    <<" sha_and_fog_glow=PASS\n";
  }
  std::cout<<"V276_ALL_THREE_REAL_R19_FINAL_DXBC_CAPTURE_PASS\n";return 0;
 }catch(const std::exception& e){
  std::cerr<<"V276_FINAL_R19_FAIL: "<<e.what()<<'\n';return 2;
 }
}
