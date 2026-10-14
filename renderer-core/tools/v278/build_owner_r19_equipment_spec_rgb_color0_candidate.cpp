// V278 owner-local final-R19 PTDE SpecRGB*COLOR0 material suboperator.
// Offline only. Never auto-bind across shared RX shader families. Output is
// a candidate for exact FLVER+MTD+slot+logical t1/PTDE-sidecar draw admission.
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
namespace D=dsrrl::operators::legacy_plan::dxbc;
namespace H=dsrrl::operators::legacy_plan::hashing;
struct scope {
    unsigned rx; const char* sha;
    std::size_t t10,c101,workflow_if,fog,glow;
    std::uint32_t reg,color;
};
constexpr scope k_scope[]={
 {33,"1d9fed8a55689e09bacf55a66a88f2c3474f7c3f9a5604fe5151b0506b06caab",1089,1100,1108,2579,2922,7,6},
 {34,"8f86b1aec73b3b9ec77c10ce6de2308d0b0d4533a1430373ea92b297ee387f97",998,1009,1017,2498,2841,6,7},
 {35,"b5f94ccec4e01e7d98c4329c2b28aecfd65e103729be969a53f486b182c55b70",658,669,677,2151,2494,5,6}
};
void must(bool b,const char* why){if(!b)throw std::runtime_error(why);}
std::vector<std::uint8_t> read(const std::string& p){
 std::ifstream f(p,std::ios::binary);must(bool(f),"original final R19 shader is missing");
 return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
std::vector<std::uint32_t> shex(const std::vector<std::uint8_t>& b,std::size_t& pos){
 must(D::checksum_container_valid(b.data(),b.size()),"invalid DXBC");
 auto n=D::read_u32(b.data()+28u);
 must(n>0&&n<64&&32ull+4ull*n<=b.size(),"invalid DXBC directory");
 pos=static_cast<std::size_t>(-1);
 for(std::uint32_t i=0;i<n;++i){
  auto at=D::read_u32(b.data()+32+4*i);
  must(at+8<=b.size(),"invalid chunk offset");
  if(std::memcmp(b.data()+at,"SHEX",4)==0){
   must(pos==static_cast<std::size_t>(-1),"duplicate SHEX");
   pos=at;
  }
 }
 must(pos!=static_cast<std::size_t>(-1),"missing SHEX");
 auto len=D::read_u32(b.data()+pos+4u);
 must(len%4==0&&pos+8ull+len<=b.size(),"invalid SHEX payload");
 std::vector<std::uint32_t> w(len/4);
 for(std::size_t i=0;i<w.size();++i)w[i]=D::read_u32(b.data()+pos+8u+4u*i);
 must(w.size()>2000&&w[1]==w.size(),"invalid SHEX length");
 return w;
}
std::size_t length(const std::vector<std::uint32_t>& w,std::size_t pos){
 must(pos<w.size(),"instruction beyond SHEX");
 auto n=(w[pos]>>24u)&127u;
 must(n>0&&pos+n<=w.size(),"bad instruction");
 return n;
}
void authorize(const std::vector<std::uint32_t>& w,const scope& rx){
 std::vector<bool> start(w.size(),false);
 for(std::size_t at=2;at<w.size();){start[at]=true;at+=length(w,at);}
 must(start[rx.t10]&&start[rx.c101]&&start[rx.workflow_if]&&
      start[rx.fog]&&start[rx.glow],"expected original operator boundaries changed");
 must(length(w,rx.t10)==11&&length(w,rx.c101)==8&&
      rx.t10+11==rx.c101&&rx.c101+8==rx.workflow_if,
      "t10 sample/c101/workflow are not adjacent");
 must(w[rx.t10+3]==0x001000e2u&&w[rx.t10+4]==rx.reg&&
      w[rx.t10+8]==10u,"wrong PTDE t10 sample RGB/slot");
 const std::array<std::uint32_t,8> c101={{
      0x08000038u,0x00100072u,rx.reg,0x00100246u,rx.reg,
      0x00208246u,12u,0u}};
 must(std::equal(c101.begin(),c101.end(),w.begin()+static_cast<std::ptrdiff_t>(rx.c101)),
      "PTDE c101 raw material multiplier changed");
 const std::array<std::uint32_t,4> ifz={{
      0x0400001fu,0x0020800au,0u,85u}};
 must(std::equal(ifz.begin(),ifz.end(),w.begin()+static_cast<std::ptrdiff_t>(rx.workflow_if)),
      "MaterialWorkflow zero branch changed");
 must(rx.fog+212<w.size()&&rx.glow+15<w.size(),
      "stock Fog or Glow preimage length invalid");
}
std::vector<std::uint8_t> make(const scope& rx,const std::vector<std::uint8_t>& b){
 must(b.size()>40&&H::matches_hex(H::sha256(b.data(),b.size()),rx.sha),
      "unverified true final R19 receiver SHA; fail-open");
 std::size_t pos=0;
 auto original=shex(b,pos);
 authorize(original,rx);
 const std::array<std::uint32_t,7> color={{
  0x07000038u,0x00100072u,rx.reg,
  0x00100246u,rx.reg,0x00101246u,rx.color}};
 auto w=original;
 w.insert(w.begin()+static_cast<std::ptrdiff_t>(rx.workflow_if),color.begin(),color.end());
 w[1]=static_cast<std::uint32_t>(w.size());
 const auto delta=static_cast<std::uint32_t>(color.size()*4);
 const auto where=pos+8u+rx.workflow_if*4u;
 std::vector<std::uint8_t> out=b;
 out.insert(out.begin()+static_cast<std::ptrdiff_t>(where),delta,std::uint8_t{0});
 for(std::size_t i=0;i<w.size();++i)D::write_u32(out.data()+pos+8u+4u*i,w[i]);
 D::write_u32(out.data()+pos+4u,D::read_u32(b.data()+pos+4u)+delta);
 D::write_u32(out.data()+24u,static_cast<std::uint32_t>(out.size()));
 const auto count=D::read_u32(b.data()+28u);
 for(std::uint32_t i=0;i<count;++i){
  auto at=32u+4u*i;
  auto off=D::read_u32(b.data()+at);
  if(off>pos)D::write_u32(out.data()+at,off+delta);
 }
 must(D::fix_checksum(out.data(),out.size()),"DXBC checksum error");
 std::size_t verify=0;const auto result=shex(out,verify);
 must(verify==pos&&result.size()==original.size()+7u&&
      result[0]==original[0]&&result[1]==original[1]+7u&&
      std::equal(original.begin()+2,original.begin()+static_cast<std::ptrdiff_t>(rx.workflow_if),result.begin()+2)&&
      std::equal(color.begin(),color.end(),result.begin()+static_cast<std::ptrdiff_t>(rx.workflow_if))&&
      std::equal(original.begin()+static_cast<std::ptrdiff_t>(rx.workflow_if),original.end(),
                 result.begin()+static_cast<std::ptrdiff_t>(rx.workflow_if+7u)),
      "any original R19 opcode changed outside the material-local COLOR0 insertion");
 for(std::uint32_t i=0;i<count;++i){
  auto at=32u+4u*i;
  auto oldoff=D::read_u32(b.data()+at);
  auto newoff=D::read_u32(out.data()+at);
  if(oldoff==pos)continue;
  auto length=D::read_u32(b.data()+oldoff+4u);
  must(newoff+8ull+length<=out.size()&&
       std::memcmp(b.data()+oldoff,out.data()+newoff,8u+length)==0,
       "RDEF/ISGN/OSGN or other DSR chunk changed");
 }
 return out;
}
int main(int argc,char** argv){
 try{
  must(argc==4,"usage: v278-spec-color RX_ID original_final_R19.dxbc candidate.dxbc");
  const auto rx=static_cast<unsigned>(std::stoul(argv[1]));
  const scope* site=nullptr;
  for(const auto& a:k_scope)if(a.rx==rx)site=&a;
  must(site!=nullptr,"RX not authorized");
  must(std::string(argv[2])!=std::string(argv[3]),"refuse in-place game shader overwrite");
  const auto source=read(argv[2]);
  const auto candidate=make(*site,source);
  std::ofstream f(argv[3],std::ios::binary|std::ios::trunc);
  must(bool(f),"output unavailable");
  f.write(reinterpret_cast<const char*>(candidate.data()),
          static_cast<std::streamsize>(candidate.size()));
  must(bool(f),"writing candidate failed");
  std::cout<<"V278_REAL_R19_EQUIPMENT_SPEC_RGB_COLOR0_LOCAL_CANDIDATE_PASS rx="
   <<rx<<" bytes="<<candidate.size()
   <<" receiver_full_SHA_verified=1 other_R19_ops_unchanged=1"
   <<" stock_Fog_LS_Glow_unchanged=1 draw_PS_bind=OFF pixel=OPEN\n";
  return 0;
 }catch(const std::exception& e){
  std::cerr<<"V278_FAIL_OPEN "<<e.what()<<'\n';
  return 2;
 }
}
