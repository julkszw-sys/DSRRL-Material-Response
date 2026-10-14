// V276: TRUE FINAL R17 donor->production-materializer->DXBC exporter.
// Local, offline, owner-original game shader input ONLY. Never check in
// extracted game DXBC or distribute a modified retail shader in a CI artifact.
//
// Unlike stock-RX audits, this invokes the exact same production C++ R17
// chain (generic diffuse-v1 -> EnvSpec -> EnvDiffuse -> material workflow ->
// SpecRGB -> terminal) that an eligible runtime shader receives.
#include "dsrrl/core/feature_registry.hpp"
#include "dsrrl/core/types.hpp"
#include "dsrrl/operators/env_spec/pmetal_rgba_materializer.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include <Windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>
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
#include <string_view>
#include <vector>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace {
using namespace dsrrl;
namespace H = operators::legacy_plan::hashing;
namespace L = operators::legacy_plan::dxbc;
struct donor {
  const char* name;
  const char* sha;
  const char* final_sha;
  unsigned rx;
  std::size_t fog;
  std::size_t glow;
};
constexpr donor k_rx[]={
  {"RX33","35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24", "b8a9f32010dd279ffae5dd3c787dceb4b06c696c3fe4007bd87ba8f3303e25ca",33,2470,2813},
  {"RX34","d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837", "abb86881378803d3389bb42d63c97750865a62e23b1ede205bfb63795855ec30",34,2389,2732},
  {"RX35","7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701", "023ad52a81399ff81066e55ff223ac9e4a90be7a64e21eb760ac5ba68f38c23b",35,2042,2385}
};
void require(bool condition,const char* why){
  if(!condition)throw std::runtime_error(why);
}
std::vector<std::uint8_t> read(const std::filesystem::path& path){
  std::ifstream f(path,std::ios::binary|std::ios::ate);
  require(bool(f),"missing original RX donor");
  auto n=f.tellg();
  require(n>0 && n<1024*1024,"unexpected DXBC input size");
  std::vector<std::uint8_t> data(static_cast<std::size_t>(n));
  f.seekg(0);
  f.read(reinterpret_cast<char*>(data.data()),
         static_cast<std::streamsize>(n));
  require(bool(f),"truncated original RX donor");
  return data;
}
void write(const std::filesystem::path& path,
           const std::vector<std::uint8_t>& data){
  std::ofstream f(path,std::ios::binary|std::ios::trunc);
  require(bool(f),"cannot write local output");
  f.write(reinterpret_cast<const char*>(data.data()),
          static_cast<std::streamsize>(data.size()));
  require(bool(f),"failed to write output DXBC");
}
std::string hex(const H::sha256_digest& digest){
  static constexpr char digits[]="0123456789abcdef";
  std::string s;
  s.reserve(64);
  for(const auto v:digest){s.push_back(digits[v>>4]);s.push_back(digits[v&15]);}
  return s;
}
std::vector<std::uint32_t> shex(const std::vector<std::uint8_t>& blob){
  require(blob.size()>=36 && std::memcmp(blob.data(),"DXBC",4)==0 &&
          L::checksum_container_valid(blob.data(),blob.size()),"DXBC checksum invalid");
  const auto count=L::read_u32(blob.data()+28);
  require(count>0 && count<64 && 32ull+4ull*count<=blob.size(),
          "DXBC chunk table invalid");
  std::vector<std::uint32_t> found;
  for(unsigned i=0;i<count;i++){
    const auto off=L::read_u32(blob.data()+32+4*i);
    require(off+8<=blob.size(),"DXBC chunk offset outside container");
    const auto size=L::read_u32(blob.data()+off+4);
    require(size<=blob.size()-off-8,"DXBC chunk size outside container");
    if(std::memcmp(blob.data()+off,"SHEX",4)==0){
      require(found.empty() && size%4==0,"SHEX missing/duplicated");
      found.resize(size/4);
      for(std::size_t k=0;k<found.size();++k)
        found[k]=L::read_u32(blob.data()+off+8+4*k);
    }
  }
  require(found.size()>=4 && found[1]==found.size(),"SHEX token length invalid");
  return found;
}
std::vector<std::size_t> boundaries(const std::vector<std::uint32_t>& w){
  std::vector<std::size_t> starts;
  for(std::size_t i=2;i<w.size();){
    const auto len=(w[i]>>24)&127u;
    require(len>0 && i+len<=w.size(),"malformed SHEX instructions");
    starts.push_back(i);i+=len;
  }
  return starts;
}
std::size_t unique_window(const std::vector<std::uint32_t>& source,
    const std::vector<std::uint32_t>& final,
    const std::vector<std::size_t>& instruction_starts,
    std::size_t offset,std::size_t length){
  require(offset+length<=source.size(),"original stock window overflow");
  std::size_t hit=static_cast<std::size_t>(-1),count=0;
  for(const auto at:instruction_starts){
    if(at+length<=final.size() &&
       std::equal(source.begin()+static_cast<std::ptrdiff_t>(offset),
                  source.begin()+static_cast<std::ptrdiff_t>(offset+length),
                  final.begin()+static_cast<std::ptrdiff_t>(at))){
      hit=at;++count;
    }
  }
  require(count==1,"original stock Fog/Glow byte window altered or duplicated");
  return hit;
}
void check_signature(ID3D11ShaderReflection* original,
                     ID3D11ShaderReflection* result){
  D3D11_SHADER_DESC a{},b{};
  require(SUCCEEDED(original->GetDesc(&a))&&SUCCEEDED(result->GetDesc(&b)),
          "D3DReflect GetDesc failed");
  require(a.InputParameters==b.InputParameters&&a.OutputParameters==b.OutputParameters,
          "input/output signature counts differ from stock");
  for(UINT kind=0;kind<2;++kind){
    const auto n=kind==0?a.InputParameters:a.OutputParameters;
    for(UINT i=0;i<n;++i){
      D3D11_SIGNATURE_PARAMETER_DESC sa{},sb{};
      HRESULT ha=kind==0?original->GetInputParameterDesc(i,&sa):
                          original->GetOutputParameterDesc(i,&sa);
      HRESULT hb=kind==0?result->GetInputParameterDesc(i,&sb):
                        result->GetOutputParameterDesc(i,&sb);
      require(SUCCEEDED(ha)&&SUCCEEDED(hb),"D3DReflect parameter failed");
      require(_stricmp(sa.SemanticName,sb.SemanticName)==0 &&
              sa.SemanticIndex==sb.SemanticIndex && sa.Register==sb.Register &&
              sa.Mask==sb.Mask && sa.ComponentType==sb.ComponentType,
              "real RX ISGN/OSGN ABI is not preserved");
    }
  }
  require(b.OutputParameters==2,"real RX output1 (Glow) was lost");
  D3D11_SHADER_INPUT_BIND_DESC bind{};
  require(SUCCEEDED(result->GetResourceBindingDescByName(
            "DSRRL_MaterialCarrier",&bind)) &&
          bind.BindPoint==12 && bind.BindCount==1 &&
          bind.Type==D3D_SIT_CBUFFER,
          "R17 exact b12 carrier not present");
  D3D11_SHADER_BUFFER_DESC cb{};
  auto* carrier=result->GetConstantBufferByName("DSRRL_MaterialCarrier");
  require(carrier&&SUCCEEDED(carrier->GetDesc(&cb))&&cb.Size==96,
          "final R17 b12 must contain six float4s = 96 bytes");
}
void driver_accept(const std::vector<std::uint8_t>& shader){
  ID3D11Device* device=nullptr;
  ID3D11DeviceContext* ctx=nullptr;
  D3D_FEATURE_LEVEL level{};
  const auto hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
      nullptr,0,D3D11_SDK_VERSION,&device,&level,&ctx);
  require(SUCCEEDED(hr)&&device,"D3D11 WARP device create failed");
  ID3D11PixelShader* ps=nullptr;
  const auto compiled=device->CreatePixelShader(
      shader.data(),shader.size(),nullptr,&ps);
  if(ps)ps->Release();
  if(ctx)ctx->Release();
  device->Release();
  require(SUCCEEDED(compiled),"real final materialized R17 PS rejected by WARP");
}
bool configure(core::feature_registry& features){
  using core::operator_id;
  constexpr std::array<operator_id,12> enabled{{
      operator_id::material_response,operator_id::spec_rgb,
      operator_id::diffuse,operator_id::normal,operator_id::env_spec,
      operator_id::terminal_sat_rgb,operator_id::diffuse_material_domain,
      operator_id::point_light,operator_id::local_specular_legacy,
      operator_id::pointlight_pnts_attenuation,
      operator_id::envspec_nospc_delete,operator_id::fixed_postfog_identity
  }};
  for(auto op:enabled)if(!features.set(op,true))return false;
  return features.set(operator_id::upper_lower,false)&&
    features.set(operator_id::hemdir3,false)&&
    features.set(operator_id::subsurface,false);
}
void check_exact(const donor& d,const std::vector<std::uint8_t>& orig,
                 const std::vector<std::uint8_t>& patched,
                 std::size_t& fog_new,std::size_t& glow_new){
  auto src=shex(orig),dst=shex(patched);
  auto src_starts=boundaries(src),dst_starts=boundaries(dst);
  require(std::find(src_starts.begin(),src_starts.end(),d.fog)!=src_starts.end(),
          "retail original Fog is not an instruction boundary");
  fog_new=unique_window(src,dst,dst_starts,d.fog,212);
  glow_new=unique_window(src,dst,dst_starts,d.glow,5);
  const auto alpha_new=unique_window(src,dst,dst_starts,d.glow+10,5);
  require(fog_new+212<=glow_new && glow_new<alpha_new,
          "stock Fog/Glow order changed");
  auto verify_reflection=[&](const std::vector<std::uint8_t>& v){
    ID3D11ShaderReflection* reflection=nullptr;
    auto hr=D3DReflect(v.data(),v.size(),IID_ID3D11ShaderReflection,
                       reinterpret_cast<void**>(&reflection));
    require(SUCCEEDED(hr)&&reflection,"real DXBC D3DReflect failed");
    return reflection;
  };
  auto* a=verify_reflection(orig);
  auto* b=verify_reflection(patched);
  try{check_signature(a,b);}catch(...){a->Release();b->Release();throw;}
  a->Release();b->Release();
  driver_accept(patched);
}
}
int main(int argc,char** argv){
  try{
    core::feature_registry features;
    require(configure(features),"production feature policy setup failed");
    if(argc==4 && std::strcmp(argv[1],"--driver-compare")==0){
      const auto original=read(argv[2]),candidate=read(argv[3]);
      (void)shex(original);
      (void)shex(candidate);
      ID3D11ShaderReflection* a=nullptr;
      ID3D11ShaderReflection* b=nullptr;
      auto first=D3DReflect(original.data(),original.size(),
          IID_ID3D11ShaderReflection,reinterpret_cast<void**>(&a));
      auto second=D3DReflect(candidate.data(),candidate.size(),
          IID_ID3D11ShaderReflection,reinterpret_cast<void**>(&b));
      require(SUCCEEDED(first)&&SUCCEEDED(second)&&a&&b,
              "V300 owner driver compare reflection failed");
      try{check_signature(a,b);}
      catch(...){a->Release();b->Release();throw;}
      a->Release();b->Release();
      driver_accept(candidate);
      std::cout<<"V300_OWNER_FINAL_IO_SIGNATURE_WARP_PS_PASS\n";
      return 0;
    }
    if(argc==2 && std::strcmp(argv[1],"--self-test")==0){
      std::vector<std::uint8_t> out{0xBEu};
      const auto result=operators::env_spec::materialize_pmetal_rgba_receiver(
          features,nullptr,0,false,out);
      require(result.result==operators::env_spec::
              pmetal_rgba_materialize_result::fail_invalid_dxbc &&
              out.empty(),"production R17 null-donor fail-open regression");
      std::cout<<"V276_REAL_R17_MATERIALIZER_LINKED_SELFTEST_PASS\n";
      return 0;
    }
    require(argc==5,
      "usage: r17-final-export --self-test OR <rx33.dxb> <rx34.dxb> <rx35.dxb> <local-outdir>");
    const std::filesystem::path dir(argv[4]);
    require(!std::filesystem::exists(dir), "owner output must not exist");
    require(std::filesystem::create_directories(dir), "owner output directory create failed");
    for(int i=0;i<3;++i){
      const auto& d=k_rx[i];
      const auto input=read(argv[i+1]);
      require(H::matches_hex(H::sha256(input.data(),input.size()),d.sha),
              "owner original donor SHA does not match exact RX");
      std::vector<std::uint8_t> output;
      const auto outcome=operators::env_spec::materialize_pmetal_rgba_receiver(
          features,input.data(),input.size(),false,output);
      require(outcome.result==operators::env_spec::
                pmetal_rgba_materialize_result::applied &&
              outcome.receiver_id==d.rx && !output.empty(),
              "actual production R17 chain failed to materialize this receiver");
      std::size_t new_fog=0,new_glow=0;
      check_exact(d,input,output,new_fog,new_glow);
      const auto hash=hex(H::sha256(output.data(),output.size()));
      require(H::matches_hex(H::sha256(output.data(),output.size()), d.final_sha),
              "Stable final R17 full SHA mismatch; refusing uncertified shader");
      write(dir/(std::string("final_r17_")+d.name+".dxbc"),output);
      std::cout<<"V276_REAL_FINAL_R17_APPLIED "<<d.name<<" original_sha="<<d.sha
        <<" final_sha="<<hash<<" final_bytes="<<output.size()
        <<" stock_fog_word="<<d.fog<<" final_fog_word="<<new_fog
        <<" stock_glow_word="<<d.glow<<" final_glow_word="<<new_glow
        <<" RDEF_b12=96 ABI=STOCK WARP_CREATE=PASS\n";
    }
    std::cout<<"V276_THREE_REAL_R17_FINAL_SHADER_DONORS_EXPORT_PASS\n";
    return 0;
  }catch(const std::exception& e){
    std::cerr<<"V276_FINAL_R17_EXPORT_FAIL "<<e.what()<<'\n';
    return 2;
  }
}
