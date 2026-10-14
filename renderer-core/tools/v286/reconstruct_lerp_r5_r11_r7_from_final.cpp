// V286 owner-local Stable R17 -> Lerp R19 R5 + R11 + R7 operator chain.
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
 const auto sh=parse(r5), inst=starts(sh.w);
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


static std::vector<std::uint8_t> port_r7(
 const receiver& x,const std::vector<std::uint8_t>& stable,
 const std::vector<std::uint8_t>&r5r11){
 // Full-source authorization is provided by reconstruct() upstream;
 // re-validate the Stable R17 donor SHA at this exact cut.
 need(S::matches_hex(S::sha256(stable.data(),stable.size()),x.stable_sha),
      "Stable R17 R7 donor not authorized");
 auto d=parse(stable), h=parse(r5r11);
 const auto ds=starts(d.w),hs=starts(h.w);
 const auto ids=[&](const std::vector<std::uint32_t>&w,
                    const std::vector<std::size_t>&st,
                    bool comparison){
  std::vector<std::size_t> hits;
  for(const auto at:st){
   if(at+13u>=w.size())continue;
   if(comparison){
    if(w[at]==0x8e000046u || ((w[at]&0x7ffu)==0x46u &&
       ((w[at]>>24u)&127u)==14u &&
       w[at+8u]==0x00107006u&&w[at+9u]==7u&&
       w[at+10u]==0x00106000u&&w[at+11u]==7u))
      hits.push_back(at);
   }else{
    if(w[at]==0x8b000045u && w[at+7u]==0x00107e46u &&
       w[at+8u]==7u&&w[at+9u]==0x00106000u&&w[at+10u]==7u)
      hits.push_back(at);
   }
  }
  return hits;
 };
 // Exact owner Stable R17 contains a 559-DWORD manually filtered R7;
 // an original Lerp R19 source still contains 176-DWORD nine-tap SAMPLE_C.
 auto donor_samples=ids(d.w,ds,false),stock_samples=ids(h.w,hs,true);
 need(donor_samples.size()==16u&&stock_samples.size()==9u,
      "R7 PTDE donor / DSR host sampling census differs");
 need(donor_samples.front()>=10u,"R7 donor prefix missing");
 const std::size_t dbegin=donor_samples.front()-10u;
 const std::size_t dend=dbegin+559u;
 const std::size_t hbegin=stock_samples.front();
 need(dend<=d.w.size()&&start(ds,dbegin)&&
      d.w[dbegin]==0x0a000000u&&
      donor_samples.front()==dbegin+10u,
      "R7 donor 559-DWORD kernel beginning mismatch");
 constexpr std::array<std::uint32_t,7> k_terminal{
    0x07000033u,0x00100082u,2u,0x0010003au,2u,
    0x00004001u,0x3f800000u};
 need(std::equal(k_terminal.begin(),k_terminal.end(),
                 d.w.begin()+static_cast<std::ptrdiff_t>(dend-7u)) &&
      start(ds,dend-7u)&&
      std::all_of(donor_samples.begin(),donor_samples.end(),
                  [dbegin,dend](std::size_t at){return at>dbegin&&at<dend;}),
      "R7 donor SAT/producer boundary mismatch");
 // Lerp original 9 x SAMPLE_C -> terminal SAT after last tap.
 std::size_t end=0,hits=0;
 for(auto at:hs) if(at>stock_samples.back()&&
                   at<=stock_samples.back()+64u&&
                   at+7u<=h.w.size()&&
                   std::equal(k_terminal.begin(),k_terminal.end(),
                              h.w.begin()+static_cast<std::ptrdiff_t>(at))){
  end=at+7u;++hits;
 }
 need(hits==1u&&end-hbegin==176u,
      "R7 host 176-DWORD source window not exact");
 // Authenticate the entire original source operator, not individual opcodes.
 std::vector<std::uint8_t> src_bytes(176u*4u);
 for(std::size_t i=0;i<176u;++i)
  C::write_u32(src_bytes.data()+i*4u,h.w[hbegin+i]);
 const char* window_sha = x.id==33
  ? "682a1d4cb910d5720f96427386f034970f3bc9435d44b6b45b558e5eb668b254"
  : "0cbd2ab2cc5e8037e43c59e037d63dca81c98983d3db94ae5fb660eb98ddcf54";
 need(S::matches_hex(S::sha256(src_bytes.data(),src_bytes.size()),
                     window_sha),"R7 stock Lerp source hash changed");
 // Original source and donor use the very same geometric shadow coordinates.
 const auto host_base=h.w[stock_samples[0]+7u];
 const auto donor_base=d.w[dbegin+4u];
 need(host_base==donor_base&&host_base==(x.id==33?6u:5u),
      "R7 source coordinate register mismatch");
 // One DCL temps13, one comparison s7 -> donor temps18, regular s7.
 std::size_t temp=0,samp=0;unsigned th=0,sh=0;
 for(const auto at:hs){
  if(h.w[at]==0x02000068u && h.w[at+1u]==13u){temp=at;++th;}
  if(h.w[at]==0x0300085au&&h.w[at+1u]==0x00106000u&&
     h.w[at+2u]==7u){samp=at;++sh;}
 }
 need(th==1u&&sh==1u&&temp<hbegin&&samp<hbegin,
      "R7 host temps13/comparison s7 declaration mismatch");
 const auto donor_temp=std::count_if(ds.begin(),ds.end(),
    [&](std::size_t at){return d.w[at]==0x02000068u&&d.w[at+1u]==18u;});
 const auto donor_samp=std::count_if(ds.begin(),ds.end(),
    [&](std::size_t at){return d.w[at]==0x0300005au&&
      d.w[at+1u]==0x00106000u&&d.w[at+2u]==7u;});
 need(donor_temp==1u&&donor_samp==1u,
      "Stable R17 R7 donor ABI differs from PTDE sampler/scratch");
 constexpr auto extra=559u-176u;
 auto words=h.w;
 words[temp+1u]=18u;
 words[samp]=0x0300005au;
 words.erase(words.begin()+static_cast<std::ptrdiff_t>(hbegin),
             words.begin()+static_cast<std::ptrdiff_t>(end));
 words.insert(words.begin()+static_cast<std::ptrdiff_t>(hbegin),
   d.w.begin()+static_cast<std::ptrdiff_t>(dbegin),
   d.w.begin()+static_cast<std::ptrdiff_t>(dend));
 words[1]=static_cast<std::uint32_t>(words.size());
 need(words.size()==h.w.size()+extra &&
   std::equal(d.w.begin()+static_cast<std::ptrdiff_t>(dbegin),
              d.w.begin()+static_cast<std::ptrdiff_t>(dend),
              words.begin()+static_cast<std::ptrdiff_t>(hbegin)),
   "R7 donor bytes differ in target");
 // All other instructions must remain exactly V284; only two DCL values
 // and the certified 176->559 R7 operator are authorized differences.
 for(std::size_t at=2;at<hbegin;++at){
  if(at==temp+1u||at==samp)continue;
  need(words[at]==h.w[at],"R7 changed upstream operator");
 }
 need(std::equal(h.w.begin()+static_cast<std::ptrdiff_t>(end),
                   h.w.end(),
                   words.begin()+static_cast<std::ptrdiff_t>(hbegin+559u)),
      "R7 changed independent downstream Fog/LS/Glow");
 // Rebuild with one SHEX length delta; all other byte ranges remain intact
 // except exact sampler s7 flags 3 -> 1 in RDEF binding table.
 auto result=r5r11;
 const auto begin_byte=h.shex+8u+hbegin*4u;
 result.erase(result.begin()+static_cast<std::ptrdiff_t>(begin_byte),
              result.begin()+static_cast<std::ptrdiff_t>(begin_byte+176u*4u));
 result.insert(result.begin()+static_cast<std::ptrdiff_t>(begin_byte),
               extra*4u+176u*4u,std::uint8_t{0});
 for(std::size_t i=0;i<words.size();++i)
  C::write_u32(result.data()+h.shex+8u+i*4u,words[i]);
 C::write_u32(result.data()+h.shex+4u,
              C::read_u32(r5r11.data()+h.shex+4u)+extra*4u);
 C::write_u32(result.data()+24u,static_cast<std::uint32_t>(result.size()));
 const auto chunkcount=C::read_u32(result.data()+28u);
 unsigned rdef_count=0u,sampler_count=0u,tex_count=0u;
 for(unsigned k=0;k<chunkcount;++k){
  const std::size_t entry=32u+k*4u;
  auto at=C::read_u32(result.data()+entry);
  if(at>h.shex){at+=extra*4u;C::write_u32(result.data()+entry,
                                  static_cast<std::uint32_t>(at));}
  need(at+8u<result.size(),"R7 chunk offset overflow");
  if(std::memcmp(result.data()+at,"RDEF",4)!=0)continue;
  ++rdef_count;
  const auto base=at+8u;
  const auto count=C::read_u32(result.data()+base+8u);
  const auto offset=C::read_u32(result.data()+base+12u);
  need(count>0u&&count<=256u&&
       base+offset+static_cast<std::size_t>(count)*32u<=result.size(),
       "R7 RDEF binding table invalid");
  for(unsigned j=0;j<count;++j){
   const auto p=base+offset+j*32u;
   if(C::read_u32(result.data()+p+20u)!=7u||
      C::read_u32(result.data()+p+24u)!=1u)continue;
   auto type=C::read_u32(result.data()+p+4u);
   auto flags=C::read_u32(result.data()+p+28u);
   if(type==3u&&flags==3u){
    C::write_u32(result.data()+p+28u,1u);++sampler_count;
   } else if(type==2u&&
             C::read_u32(result.data()+p+12u)==4u&&flags==13u)
    ++tex_count;
   else need(false,"R7 incompatible RDEF t7/s7 binding");
  }
 }
 need(rdef_count==1u&&sampler_count==1u&&tex_count==1u,
      "R7 missing/duplicated RDEF sampler binding");
 need(C::fix_checksum(result.data(),result.size()),
      "R7 rebuilt DXBC checksum failed");
 need(parse(result).w==words,"R7 final SHEX verification failed");
 // Explicitly verify other chunks (RDEF only sampler flag is allowed).
 for(unsigned k=0;k<chunkcount;++k){
  auto oldat=C::read_u32(r5r11.data()+32u+k*4u);
  auto newat=C::read_u32(result.data()+32u+k*4u);
  auto oldlen=C::read_u32(r5r11.data()+oldat+4u);
  auto newlen=C::read_u32(result.data()+newat+4u);
  if(oldat==h.shex)continue;
  need(oldlen==newlen,"R7 modified an unrelated chunk length");
  if(std::memcmp(r5r11.data()+oldat,"RDEF",4u)==0)continue;
  need(std::memcmp(r5r11.data()+oldat,result.data()+newat,
                   oldlen+8u)==0,
       "R7 modified ISGN/OSGN/STAT/other non-SHEX chunk");
 }
 return result;
}

int main(int argc,char**argv){try{
 need(argc==8,"usage: v281 stable33 stable34 stable35 lerp33 lerp34 lerp35 output-dir");
 const std::filesystem::path outdir(argv[7]);
 std::filesystem::create_directories(outdir);
 for(int i=0;i<3;i++){
  const auto a=read(argv[i+1]),b=read(argv[i+4]);
  const auto r5=reconstruct(rx[i],a,b);
  const auto r5r11=port_r11(rx[i],r5);
  const auto patched=(rx[i].id==35)?r5r11:port_r7(rx[i],a,r5r11);
  const auto target=outdir/("v286_R5_R11_R7_LerpRX"+std::to_string(rx[i].id)+".dxbc");
  need(!std::filesystem::exists(target),"owner output exists; refuse overwrite");
  std::ofstream f(target,std::ios::binary);
  f.write(reinterpret_cast<const char*>(patched.data()),
          static_cast<std::streamsize>(patched.size()));
  need(bool(f),"candidate DXBC output not written");
  std::cout<<"V286_R5_R11_R7_LERP_OPERATOR_RX"<<rx[i].id
           <<"_PASS bytes="<<patched.size()
           <<" live_PS_bind=OFF pixel=OPEN\n";
 }
 return 0;
}catch(const std::exception&e){
 std::cerr<<"V286_R5_R11_R7_FAIL_OPEN "<<e.what()<<"\n";return 2;
}}
