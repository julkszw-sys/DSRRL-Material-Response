// V283: owner-local, SHA-locked Stable R17 MaterialWorkflow=0 GAMMA DIAGNOSTIC.
// Does NOT bind shaders or alter the addon at runtime. Preserve protected
// working R43 RC1, shared receiver material gating, stock Fog/LS/Glow.
// No game DXBC distributed. First-branch gamma is NOT yet a proven specular cause.
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace C=dsrrl::operators::legacy_plan::dxbc;
namespace H=dsrrl::operators::legacy_plan::hashing;
struct receiver {int id;const char*sha;};
constexpr receiver target[]={
 {33,"b8a9f32010dd279ffae5dd3c787dceb4b06c696c3fe4007bd87ba8f3303e25ca"},
 {34,"abb86881378803d3389bb42d63c97750865a62e23b1ede205bfb63795855ec30"},
 {35,"023ad52a81399ff81066e55ff223ac9e4a90be7a64e21eb760ac5ba68f38c23b"}
};
static void require(bool ok,const char*msg){if(!ok)throw std::runtime_error(msg);}
struct instruction {std::size_t at,len;unsigned op;};
struct shader {std::size_t shex=static_cast<std::size_t>(-1);std::vector<std::uint32_t>w;};
static shader read_shex(const std::vector<std::uint8_t>&src){
 require(src.size()>36&&C::checksum_container_valid(src.data(),src.size()),
         "DXBC size/envelope invalid");
 auto n=C::read_u32(src.data()+28);
 require(n>0&&n<=32&&32ull+4ull*n<=src.size(),"bad DXBC chunk table");
 shader s;
 for(unsigned k=0;k<n;++k){
  const auto at=static_cast<std::size_t>(C::read_u32(src.data()+32+4*k));
  require(at+8<=src.size(),"bad DXBC chunk offset");
  const auto len=static_cast<std::size_t>(C::read_u32(src.data()+at+4));
  require(len<=src.size()-at-8,"DXBC chunk overflow");
  if(std::memcmp(src.data()+at,"SHEX",4)==0){
   require(s.shex==static_cast<std::size_t>(-1)&&len%4==0,
           "duplicate or misaligned SHEX");
   s.shex=at;s.w.resize(len/4);
   for(std::size_t j=0;j<s.w.size();++j)
    s.w[j]=C::read_u32(src.data()+at+8+4*j);
  }
 }
 require(s.shex!=static_cast<std::size_t>(-1)&&s.w.size()>1200u&&
         s.w[1]==s.w.size(),"unrecognized SHEX");
 return s;
}
static std::vector<instruction> instructions(const shader&s){
 std::vector<instruction>v;
 for(std::size_t at=2;at<s.w.size();){
  const auto n=(s.w[at]>>24u)&127u;
  require(n&&at+n<=s.w.size(),"malformed instruction");
  v.push_back({at,n,s.w[at]&2047u});at+=n;
 }
 return v;
}
static std::size_t find_true_gamma(const shader&s){
 constexpr std::uint32_t two2=0x400ccccdu;
 const auto ins=instructions(s);
 struct branch {bool material;bool is_true;};
 std::vector<branch>stack;
 unsigned material_if_hits=0u,gamma_hits=0u;
 std::size_t gamma=0u;
 for(std::size_t k=0;k<ins.size();++k){
  const auto &x=ins[k];
  if(x.op==0x1fu){
   const bool mat=x.len==4&&
    s.w[x.at+1]==0x0020800au&&s.w[x.at+2]==0u&&s.w[x.at+3]==85u;
   if(mat)++material_if_hits;
   stack.push_back({mat,true});
  }else if(x.op==0x12u){
   require(!stack.empty(),"orphan ELSE");
   stack.back().is_true=false;
  }else if(x.op==0x15u){
   require(!stack.empty(),"orphan ENDIF");
   stack.pop_back();
  }
  if(x.op!=0x38u||x.len!=10u||s.w[x.at+5]!=0x00004002u||
     s.w[x.at+6]!=two2||s.w[x.at+7]!=two2||s.w[x.at+8]!=two2)
   continue;
  const bool in_true=std::any_of(stack.begin(),stack.end(),[](const branch&b){
   return b.material&&b.is_true;
  });
  if(!in_true)continue;
  require(k>0&&k+1<ins.size(),"incomplete gamma chain");
  const auto &before=ins[k-1],&after=ins[k+1];
  require(before.op==0x2fu&&after.op==0x19u&&
          before.at+before.len==x.at&&x.at+x.len==after.at,
          "not exact LOG-MUL-EXP");
  require(s.w[x.at+1]==0x00100072u&&s.w[x.at+3]==0x00100246u&&
          s.w[x.at+2]==s.w[x.at+4]&&
          s.w[before.at+2]==s.w[x.at+2]&&
          s.w[after.at+3]==0x00100246u&&
          s.w[after.at+4]==s.w[x.at+2],
          "gamma same-register topology changed");
  gamma=x.at;++gamma_hits;
 }
 require(stack.empty()&&material_if_hits==1u&&gamma_hits==1u,
         "MaterialWorkflow=0 gamma not unique");
 return gamma;
}
static std::vector<std::uint8_t> readfile(const std::filesystem::path&p){
 std::ifstream f(p,std::ios::binary);
 require(bool(f),"missing owner-final Stable R17 DXBC");
 return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
static std::vector<std::uint8_t> candidate(
 const receiver&r,const std::vector<std::uint8_t>&input){
 require(H::matches_hex(H::sha256(input.data(),input.size()),r.sha),
         "FINAL R17 full SHA mismatch; fail-open");
 const auto sh=read_shex(input);
 const auto gamma=find_true_gamma(sh);
 std::vector<std::uint8_t>out=input;
 // Branch-local one-variable intervention: pow(abs(x),2.2)->pow(abs(x),1).
 // Exactly 3 literals; do NOT remove other gammas or touch postprocess.
 for(unsigned i=6;i<9;++i)
  C::write_u32(out.data()+sh.shex+8+4*(gamma+i),0x3f800000u);
 require(C::fix_checksum(out.data(),out.size()),"DXBC checksum failed");
 const auto patched=read_shex(out);
 require(patched.shex==sh.shex&&patched.w.size()==sh.w.size(),
         "rebuild changed SHEX size");
 unsigned edits=0;
 for(std::size_t i=0;i<sh.w.size();++i)
  if(sh.w[i]!=patched.w[i]){
   ++edits;
   require(i>=gamma+6&&i<=gamma+8&&patched.w[i]==0x3f800000u,
           "nonlocal shader mutation");
  }
 require(edits==3,"failed to isolate all 3 RGB exponents");
 require(std::equal(input.begin()+20,input.begin()+static_cast<std::ptrdiff_t>(sh.shex+8),
                    out.begin()+20)&&
         std::equal(input.begin()+static_cast<std::ptrdiff_t>(sh.shex+8+4*sh.w.size()),
                    input.end(),
                    out.begin()+static_cast<std::ptrdiff_t>(sh.shex+8+4*sh.w.size())),
         "non-SHEX or stock Fog/Glow bytes altered");
 return out;
}
int main(int argc,char**argv){try{
 require(argc==5,"usage: v283 ownerFINAL33 ownerFINAL34 ownerFINAL35 empty-output-dir");
 const std::filesystem::path dir(argv[4]);
 require(!std::filesystem::exists(dir)||std::filesystem::is_empty(dir),
         "refuse output overwrite");
 std::array<std::vector<std::uint8_t>,3> outputs;
 for(unsigned i=0;i<3;++i)
  outputs[i]=candidate(target[i],readfile(argv[i+1]));
 std::filesystem::create_directories(dir);
 for(unsigned i=0;i<3;++i){
  auto file=dir/("V283_Stable_RX"+std::to_string(target[i].id)+"_workflow0_DIAGNOSTIC_ONLY.dxbc");
  require(!std::filesystem::exists(file),"candidate already exists");
  std::ofstream o(file,std::ios::binary);
  o.write(reinterpret_cast<const char*>(outputs[i].data()),
          static_cast<std::streamsize>(outputs[i].size()));
  require(bool(o),"candidate write failed");
  std::cout<<"V283_STABLE_RX"<<target[i].id<<"_CONSTRUCTION_PASS "
           <<"bytes="<<outputs[i].size()
           <<" runtime_bind=OFF pixel=OPEN\n";
 }
 return 0;
}catch(const std::exception&e){
 std::cerr<<"V283_FAIL_OPEN "<<e.what()<<"\n";
 return 2;
}}
