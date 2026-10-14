// V284 owner-local Stable R17 -> Lerp R19 R5 + R11 operator chain.
// The EXACT stable output DXBC is the code donor for a 43-DWORD PTDE R5
// cross(N,T)*handedness. No shader blob or retail asset is shipped.
// Does not bind shared RX or patch DarkSoulsRemastered.exe. Fail-open.
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
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
namespace C=dsrrl::operators::legacy_plan::dxbc;
namespace S=dsrrl::operators::legacy_plan::hashing;
struct receiver {
 unsigned id; const char *stable_sha,*lerp_sha;
 std::size_t stable_face,stable_cross,stable_minus;
 std::size_t lerp_face,lerp_bitangent,lerp_minus;
};
constexpr receiver rx[]={
 {33,"b8a9f32010dd279ffae5dd3c787dceb4b06c696c3fe4007bd87ba8f3303e25ca",
     "1d9fed8a55689e09bacf55a66a88f2c3474f7c3f9a5604fe5151b0506b06caab",
     207,315,358,221,329,348},
 {34,"abb86881378803d3389bb42d63c97750865a62e23b1ede205bfb63795855ec30",
     "8f86b1aec73b3b9ec77c10ce6de2308d0b0d4533a1430373ea92b297ee387f97",
     210,318,361,224,332,351},
 {35,"023ad52a81399ff81066e55ff223ac9e4a90be7a64e21eb760ac5ba68f38c23b",
     "b5f94ccec4e01e7d98c4329c2b28aecfd65e103729be969a53f486b182c55b70",
     200,308,351,214,322,341}
};
static void need(bool v,const char*why){if(!v)throw std::runtime_error(why);}
static std::vector<std::uint8_t> read(const std::filesystem::path&p){
 std::ifstream f(p,std::ios::binary);
 need(bool(f),"original owner shader missing");
 return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
struct dxbc {std::size_t shex=std::size_t(-1);std::vector<std::uint32_t>w,chunk_offsets;};
static dxbc parse(const std::vector<std::uint8_t>&b){
 need(b.size()>36&&C::checksum_container_valid(b.data(),b.size()),"DXBC envelope invalid");
 auto n=C::read_u32(b.data()+28);
 need(n>=2&&n<64&&32ull+4ull*n<=b.size(),"DXBC chunk count invalid");
 dxbc out;
 for(unsigned j=0;j<n;j++){
  auto at=C::read_u32(b.data()+32+4*j);
  need(at+8<=b.size(),"DXBC chunk offset invalid");
  auto len=C::read_u32(b.data()+at+4);
  need(len<=b.size()-at-8,"DXBC chunk overflow");
  out.chunk_offsets.push_back(at);
  if(std::memcmp(b.data()+at,"SHEX",4)==0){
   need(out.shex==std::size_t(-1)&&len%4==0,"missing/duplicate SHEX");
   out.shex=at;out.w.resize(len/4);
   for(std::size_t k=0;k<out.w.size();k++)
    out.w[k]=C::read_u32(b.data()+at+8+4*k);
  }
 }
 need(out.shex!=std::size_t(-1)&&out.w.size()>1800&&out.w[1]==out.w.size(),
      "DXBC SHEX length invalid");
 return out;
}
static std::vector<std::size_t> starts(const std::vector<std::uint32_t>&w){
 std::vector<std::size_t> result;
 for(std::size_t i=2;i<w.size();){
  auto z=(w[i]>>24)&127u; need(z>0&&i+z<=w.size(),"bad SHEX instruction");
  result.push_back(i);i+=z;
 }
 return result;
}
static bool start(const std::vector<std::size_t>&s,std::size_t at){
 return std::find(s.begin(),s.end(),at)!=s.end();
}
static std::vector<std::uint8_t> reconstruct(
 const receiver&x,const std::vector<std::uint8_t>&stable,
 const std::vector<std::uint8_t>&lerp){
 need(S::matches_hex(S::sha256(stable.data(),stable.size()),x.stable_sha),
      "Stable R17 SHA not authorized");
 need(S::matches_hex(S::sha256(lerp.data(),lerp.size()),x.lerp_sha),
      "Lerp final R19 SHA not authorized");
 auto a=parse(stable),b=parse(lerp);
 const auto stable_starts=starts(a.w),lerp_starts=starts(b.w);
 need(start(stable_starts,x.stable_face)&&start(stable_starts,x.stable_cross)
    &&start(stable_starts,x.stable_minus)
    &&start(lerp_starts,x.lerp_face)&&start(lerp_starts,x.lerp_bitangent)
    &&start(lerp_starts,x.lerp_minus),"original normal cut instruction shifted");
 need(x.stable_cross+43==x.stable_minus &&
      x.lerp_bitangent+19==x.lerp_minus,
      "PTDE R5 cross versus DSR bitangent cut lengths changed");
 need(a.w[x.stable_face]==0x0a000037u&&
      b.w[x.lerp_face]==0x0a000037u&&
      a.w[x.stable_face+8]==1u&&
      b.w[x.lerp_face+8]==0x41u&&
      a.w[x.stable_face+6]==b.w[x.lerp_face+6],
      "not exact same geometric normal or expected frontface operator");
 const auto tangent_stable=x.stable_cross-19, tangent_lerp=x.lerp_bitangent-19;
 need(start(stable_starts,tangent_stable)&&start(lerp_starts,tangent_lerp)
    &&a.w[tangent_stable]==0x07000010u
    &&b.w[tangent_lerp]==0x07000010u
    &&a.w[tangent_stable+4]==b.w[tangent_lerp+4],
    "stable and Lerp normal/tangent inputs differ");
 need(b.w[x.lerp_bitangent]==0x07000010u&&
      b.w[x.lerp_bitangent+7]==0x05000044u&&
      b.w[x.lerp_bitangent+12]==0x07000038u&&
      b.w[x.lerp_bitangent+14]==6u,
      "DSR interpolated bitangent preimage changed");
 // Exact recovered stable R5 payload, NOT independently guessed shader math.
 std::array<std::uint32_t,43> ptde{};
 std::copy_n(a.w.begin()+static_cast<std::ptrdiff_t>(x.stable_cross),43,ptde.begin());
 need(ptde[0]==0x07000038u&&ptde[7]==0x0a000032u&&
      ptde[17]==0x07000038u&&ptde[24]==0x07000010u&&
      ptde[31]==0x05000044u&&ptde[36]==0x07000038u,
      "Stable R17 R5 43-DWORD payload not authenticated");
 auto out=b.w;
 out[x.lerp_face+8]=1u;
 out.erase(out.begin()+static_cast<std::ptrdiff_t>(x.lerp_bitangent),
           out.begin()+static_cast<std::ptrdiff_t>(x.lerp_bitangent+19));
 out.insert(out.begin()+static_cast<std::ptrdiff_t>(x.lerp_bitangent),
            ptde.begin(),ptde.end());
 out[1]=static_cast<std::uint32_t>(out.size());
 need(out.size()==b.w.size()+24u &&
      std::equal(b.w.begin()+2,b.w.begin()+static_cast<std::ptrdiff_t>(x.lerp_face+8),out.begin()+2) &&
      std::equal(b.w.begin()+static_cast<std::ptrdiff_t>(x.lerp_face+9),
                 b.w.begin()+static_cast<std::ptrdiff_t>(x.lerp_bitangent),
                 out.begin()+static_cast<std::ptrdiff_t>(x.lerp_face+9)) &&
      std::equal(ptde.begin(),ptde.end(),
                 out.begin()+static_cast<std::ptrdiff_t>(x.lerp_bitangent)) &&
      std::equal(b.w.begin()+static_cast<std::ptrdiff_t>(x.lerp_bitangent+19),b.w.end(),
                 out.begin()+static_cast<std::ptrdiff_t>(x.lerp_bitangent+43)),
      "unexpected nonlocal R19 instruction modification");
 auto modified=lerp;
 auto insert_at=b.shex+8u+x.lerp_bitangent*4u;
 modified.erase(modified.begin()+static_cast<std::ptrdiff_t>(insert_at),
                modified.begin()+static_cast<std::ptrdiff_t>(insert_at+19u*4u));
 modified.insert(modified.begin()+static_cast<std::ptrdiff_t>(insert_at),
                 43u*4u,std::uint8_t{0});
 for(std::size_t k=0;k<out.size();k++)
  C::write_u32(modified.data()+b.shex+8u+4u*k,out[k]);
 C::write_u32(modified.data()+b.shex+4u,C::read_u32(lerp.data()+b.shex+4u)+96u);
 C::write_u32(modified.data()+24u,static_cast<std::uint32_t>(modified.size()));
 for(std::size_t j=0;j<b.chunk_offsets.size();j++){
  auto off=b.chunk_offsets[j];
  if(off>b.shex)C::write_u32(modified.data()+32u+4u*j,off+96u);
 }
 need(C::fix_checksum(modified.data(),modified.size()),"DXBC checksum fail");
 need(parse(modified).w==out,"rebuilt R19 SHEX differs");
 for(std::size_t j=0;j<b.chunk_offsets.size();j++){
  auto oldat=b.chunk_offsets[j],newat=C::read_u32(modified.data()+32+4*j);
  if(oldat==b.shex)continue;
  const auto len=C::read_u32(lerp.data()+oldat+4);
  need(newat+8ull+len<=modified.size()&&
       std::memcmp(lerp.data()+oldat,modified.data()+newat,8ull+len)==0,
       "native RDEF/ISGN/OSGN/Fog/Glow non-SHEX chunk changed");
 }
 return modified;
}

static std::vector<std::uint8_t> port_r11(const receiver &x,
 const std::vector<std::uint8_t> &r5) {
 // V281 R5 candidate hashes were independently reproduced on owner-final
 // Stable R17 and Lerp R19; require their exact bytes, not generic HemEnvLerp.
 const char *expected =
    x.id == 33 ? "552ae40c58300ec63a398a4d92e5e5c926694623756a95d2c5df7e7e36e1af1a" :
    x.id == 34 ? "2bdfeac13fcce953bfa39b8fb299e79dfdb99cd2bb72d6a20a9d66ce052efc22" :
                 "fd364e4eaef350f2cbd86bb6fc4aa4b56a5e40f08e8e646b7951897174e772ca";
 need(S::matches_hex(S::sha256(r5.data(),r5.size()),expected),
      "V281 R5 candidate SHA not authorized");
 const auto sh=parse(r5);
 const auto inst=starts(sh.w);
 std::size_t matched=0, sample=0, multiply=0;
 for(std::size_t j=0;j+1<inst.size();++j){
  const std::size_t at=inst[j], next=inst[j+1];
  if((sh.w[at]&0x7ffu)!=0x48u ||
     ((sh.w[at]>>24u)&127u)!=13u ||
     sh.w[at+8]!=8u || sh.w[at+10]!=8u ||
     next!=at+13u || (sh.w[next]&0x7ffu)!=0x38u ||
     ((sh.w[next]>>24u)&127u)!=7u)
     continue;
  if(sh.w[next+1]!=0x001000e2u || sh.w[next+2]!=1u ||
     sh.w[next+3]!=0x00100e56u || sh.w[next+4]!=1u ||
     sh.w[next+5]!=0x00100006u ||
     sh.w[next+6]!=sh.w[at+4])
    continue;
  ++matched;sample=at;multiply=next;
 }
 need(matched==1,"R11 SAO t8 -> r1.xyz multiplier absent or ambiguous");
 const auto sample_operand=sh.w[multiply+6];
 need(sample_operand==sh.w[sample+4],"R11 incorrect sampled SAO scalar");
 auto out=r5;
 C::write_u32(out.data()+sh.shex+8u+4u*(multiply+5u),0x00004001u);
 C::write_u32(out.data()+sh.shex+8u+4u*(multiply+6u),0x3f800000u);
 need(C::fix_checksum(out.data(),out.size()),"R11 DXBC checksum failed");
 const auto after=parse(out);
 need(after.w.size()==sh.w.size(),"R11 SHEX length changed");
 std::size_t changed=0;
 for(std::size_t i=0;i<sh.w.size();++i){
  if(sh.w[i]==after.w[i])continue;
  need((i==multiply+5u&&after.w[i]==0x00004001u) ||
       (i==multiply+6u&&after.w[i]==0x3f800000u),
       "R11 nonlocal instruction mutation");
  ++changed;
 }
 need(changed==2u,"R11 not exactly two operand words");
 // Validate no changes to RDEF/ISGN/OSGN and no Fog/LS/Glow bytes touched.
 need(std::equal(r5.begin()+20,r5.begin()+static_cast<std::ptrdiff_t>(sh.shex+8u),out.begin()+20) &&
      std::equal(r5.begin()+static_cast<std::ptrdiff_t>(sh.shex+8u+sh.w.size()*4u),r5.end(),
                 out.begin()+static_cast<std::ptrdiff_t>(sh.shex+8u+sh.w.size()*4u)),
      "R11 changed unrelated DXBC content");
 return out;
}

int main(int argc,char**argv){try{
 need(argc==8,"usage: v281 stable33 stable34 stable35 lerp33 lerp34 lerp35 output-dir");
 const std::filesystem::path outdir(argv[7]);
 std::filesystem::create_directories(outdir);
 for(int i=0;i<3;i++){
  const auto a=read(argv[i+1]),b=read(argv[i+4]);
  const auto r5=reconstruct(rx[i],a,b);
  const auto patched=port_r11(rx[i],r5);
  const auto target=outdir/("v284_R5_R11_LerpRX"+std::to_string(rx[i].id)+".dxbc");
  need(!std::filesystem::exists(target),"owner output exists; refuse overwrite");
  std::ofstream f(target,std::ios::binary);
  f.write(reinterpret_cast<const char*>(patched.data()),
          static_cast<std::streamsize>(patched.size()));
  need(bool(f),"candidate DXBC output not written");
  std::cout<<"V284_R5_R11_LERP_OPERATOR_RX"<<rx[i].id
           <<"_PASS bytes="<<patched.size()
           <<" live_PS_bind=OFF pixel=OPEN\n";
 }
 return 0;
}catch(const std::exception&e){
 std::cerr<<"V284_R5_R11_FAIL_OPEN "<<e.what()<<"\n";return 2;
}}
