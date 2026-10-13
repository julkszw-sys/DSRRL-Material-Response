// V258: standalone DXBC GPU shader generation and exact ABI census.
// Construction only: deliberately no DSR bind, no pixel-pass claim.
#include <Windows.h>
#include <initguid.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#pragma comment(lib,"d3dcompiler.lib")

struct result {
  std::vector<unsigned char> bytes;
};
static void check(bool test,const char* text) {
  if(!test)throw std::runtime_error(text);
}
static void binding(ID3D11ShaderReflection* r,const char* name,UINT slot,
                    D3D_SHADER_INPUT_TYPE type) {
  D3D11_SHADER_INPUT_BIND_DESC d{};
  check(SUCCEEDED(r->GetResourceBindingDescByName(name,&d)),
        "missing required shader binding");
  check(d.BindPoint==slot&&d.BindCount==1&&d.Type==type,
        "incorrect shader binding slot/type");
}
static result compile(const wchar_t* path,const char* label,UINT cbuffer_bytes,
                      bool env_resources) {
  ID3DBlob *binary=nullptr,*errors=nullptr;
  const UINT flags=D3DCOMPILE_ENABLE_STRICTNESS|
                   D3DCOMPILE_WARNINGS_ARE_ERRORS|
                   D3DCOMPILE_OPTIMIZATION_LEVEL3;
  const HRESULT h=D3DCompileFromFile(path,nullptr,
    D3D_COMPILE_STANDARD_FILE_INCLUDE,"main","ps_5_0",
    flags,0,&binary,&errors);
  if(FAILED(h)||!binary) {
    if(errors)std::fwrite(errors->GetBufferPointer(),1,
                          errors->GetBufferSize(),stderr);
    if(errors)errors->Release();
    throw std::runtime_error("D3DCompileFromFile failed");
  }
  if(errors)errors->Release();
  ID3D11ShaderReflection* reflect=nullptr;
  check(SUCCEEDED(D3DReflect(binary->GetBufferPointer(),
    binary->GetBufferSize(),IID_ID3D11ShaderReflection,
    reinterpret_cast<void**>(&reflect)))&&reflect,
    "D3DReflect failed");
  binding(reflect,env_resources?"V258_PTDE_HEMENVLERP_CB":
    "V258_PTDE_TERMINAL_CB",12,D3D_SIT_CBUFFER);
  ID3D11ShaderReflectionConstantBuffer* cb=reflect->GetConstantBufferByName(
    env_resources?"V258_PTDE_HEMENVLERP_CB":"V258_PTDE_TERMINAL_CB");
  D3D11_SHADER_BUFFER_DESC desc{};
  check(cb&&SUCCEEDED(cb->GetDesc(&desc))&&desc.Size==cbuffer_bytes,
        "constant buffer ABI size incorrect");
  D3D11_SHADER_DESC sh{};
  check(SUCCEEDED(reflect->GetDesc(&sh)),"shader reflection failed");
  for(UINT i=0;i<sh.BoundResources;++i) {
    D3D11_SHADER_INPUT_BIND_DESC d{};
    check(SUCCEEDED(reflect->GetResourceBindingDesc(i,&d)),
          "resource enumeration failed");
    check(!(d.Type==D3D_SIT_TEXTURE&&d.BindPoint==9),
          "stock DSR t9 PBL BRDF LUT leaked into PTDE island");
  }
  if(env_resources) {
    struct req{const char* name;UINT slot;};
    const req expected[]={
       {"ptdeDiffuseTex",0},{"ptdeSpecTex",10},
       {"ptdeEnvDiffuseA",11},{"ptdeEnvSpecA",12},
       {"ptdeEnvDiffuseB",13},{"ptdeEnvSpecB",14}};
    for(const req& v:expected)binding(reflect,v.name,v.slot,D3D_SIT_TEXTURE);
    binding(reflect,"ptdeSampler",0,D3D_SIT_SAMPLER);
    check(sh.InputParameters>=4,"PTDE GPU subcut input ABI incomplete");
  }else{
    check(sh.BoundResources==1,"late terminal unexpectedly has SRV/sampler");
  }
  const unsigned char* data=static_cast<const unsigned char*>(
    binary->GetBufferPointer());
  check(binary->GetBufferSize()>128&&std::memcmp(data,"DXBC",4)==0,
        "invalid DXBC binary");
  result out{};
  out.bytes.assign(data,data+binary->GetBufferSize());
  std::printf("V258_%s_DXBC_ABI_PASS bytes=%zu cb12_bytes=%u srvs=%u\n",
              label,out.bytes.size(),cbuffer_bytes,sh.BoundResources-1u);
  reflect->Release();
  binary->Release();
  return out;
}
static void save(const result& r,const char* path){
  std::ofstream f(path,std::ios::binary);
  check(static_cast<bool>(f),"unable to open DXBC destination");
  f.write(reinterpret_cast<const char*>(r.bytes.data()),
          static_cast<std::streamsize>(r.bytes.size()));
  check(static_cast<bool>(f),"unable to write complete DXBC");
}

// V267: independent compilation oracle against exact retail RX33/34/35
// semantic register/mask census. Compiled fixtures are NEVER embedded or bound.
// Matching the input/output ABI is NECESSARY, not sufficient for pixel equivalence.
struct v267_semantic {
  const char *name;
  UINT index, reg, mask, component_type;
};
static void native_abi_fixture(const char *entry, bool shadow, bool ptde = false) {
  ID3DBlob *blob=nullptr,*errors=nullptr;
  const HRESULT h=D3DCompileFromFile(
    ptde ? L"renderer-core/tools/v258/v268_native_ptde_rx_environment_fixture.hlsl" :
           L"renderer-core/tools/v258/v267_rx_abi_fixture.hlsl",nullptr,
    D3D_COMPILE_STANDARD_FILE_INCLUDE,entry,"ps_5_0",
    D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_WARNINGS_ARE_ERRORS|
    D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);
  if(FAILED(h)||!blob) {
    if(errors)std::fwrite(errors->GetBufferPointer(),1,
                           errors->GetBufferSize(),stderr);
    if(errors)errors->Release();
    throw std::runtime_error("V267 HLSL receiver ABI fixture compile failed");
  }
  if(errors)errors->Release();
  ID3D11ShaderReflection *reflection=nullptr;
  check(SUCCEEDED(D3DReflect(blob->GetBufferPointer(),blob->GetBufferSize(),
        IID_ID3D11ShaderReflection,reinterpret_cast<void**>(&reflection)))
        && reflection,"V267 D3DReflect failure");
  const v267_semantic expected_regular[]={
    {"SV_Position",0,0,15,3},{"TEXCOORD",0,1,15,3},
    {"TEXCOORD",2,2,15,3},{"TEXCOORD",3,3,15,3},
    {"TEXCOORD",4,4,15,3},{"TEXCOORD",5,5,7,3},
    {"COLOR",0,6,15,3},{"TEXCOORD",6,7,3,3},
    {"SV_IsFrontFace",0,8,1,1},
  };
  const v267_semantic expected_shadow[]={
    {"SV_Position",0,0,15,3},{"TEXCOORD",0,1,15,3},
    {"TEXCOORD",1,2,15,3},{"TEXCOORD",2,3,15,3},
    {"TEXCOORD",3,4,15,3},{"TEXCOORD",4,5,15,3},
    {"TEXCOORD",5,6,7,3},{"COLOR",0,7,15,3},
    {"TEXCOORD",6,8,3,3},{"SV_IsFrontFace",0,9,1,1},
  };
  const auto* expected=shadow?expected_shadow:expected_regular;
  const UINT count=shadow?10u:9u;
  D3D11_SHADER_DESC desc{};
  check(SUCCEEDED(reflection->GetDesc(&desc))&&
        desc.InputParameters==count&&desc.OutputParameters==2u,
        "V267 RX semantic input count or dual render target missing");
  for(UINT i=0;i<count;++i) {
    D3D11_SIGNATURE_PARAMETER_DESC d{};
    check(SUCCEEDED(reflection->GetInputParameterDesc(i,&d)),
          "V267 native input reflection unavailable");
    const auto &e=expected[i];
    check(_stricmp(d.SemanticName,e.name)==0 &&
          d.SemanticIndex==e.index && d.Register==e.reg &&
          d.Mask==e.mask && static_cast<UINT>(d.ComponentType)==e.component_type,
          "V267 HLSL native input ABI differs from pinned retail DSR RX");
  }
  for(UINT i=0;i<2u;++i) {
    D3D11_SIGNATURE_PARAMETER_DESC d{};
    check(SUCCEEDED(reflection->GetOutputParameterDesc(i,&d)),
          "V267 native output reflection unavailable");
    check(_stricmp(d.SemanticName,"SV_Target")==0 &&
          d.SemanticIndex==i && d.Register==i && d.Mask==15u,
          "V267 dual target signature not retail DSR equivalent");
  }
  if (ptde) {
    // Real GPU sampling of the native RX interpolants through shared
    // PTDE EnvLocal, but synthetic RT1 makes this a NON-LIVE fixture.
    binding(reflection,"V268_PTDE_EnvLocal",12,D3D_SIT_CBUFFER);
    D3D11_SHADER_BUFFER_DESC cb_desc{};
    auto *cb=reflection->GetConstantBufferByName("V268_PTDE_EnvLocal");
    check(cb&&SUCCEEDED(cb->GetDesc(&cb_desc))&&cb_desc.Size==8u*16u,
          "V268 native PTDE fixture b12 ABI mismatch");
    struct v268_slot { const char *name; UINT slot; };
    const v268_slot textures[]={
      {"ptdeDiffuseTex",0u},{"ptdeSpecTex",10u},
      {"ptdeEnvDiffuseA",11u},{"ptdeEnvSpecA",12u},
      {"ptdeEnvDiffuseB",13u},{"ptdeEnvSpecB",14u}
    };
    for (const auto &t:textures)
      binding(reflection,t.name,t.slot,D3D_SIT_TEXTURE);
    binding(reflection,"ptdeSampler",0u,D3D_SIT_SAMPLER);
    // V269: original retail RX normal map is stock resource t2 with its
    // own s2 sampler. Host normal-strength producer remains UNVERIFIED.
    binding(reflection,"stockNormalTex",2u,D3D_SIT_TEXTURE);
    binding(reflection,"stockNormalSampler",2u,D3D_SIT_SAMPLER);
    binding(reflection,"V269_UNVERIFIED_StockNormalScale",13u,D3D_SIT_CBUFFER);
    D3D11_SHADER_BUFFER_DESC norm_desc{};
    auto *norm_cb=reflection->GetConstantBufferByName(
        "V269_UNVERIFIED_StockNormalScale");
    check(norm_cb&&SUCCEEDED(norm_cb->GetDesc(&norm_desc))&&
          norm_desc.Size==16u,"V269 host normal-scale CB ABI mismatch");

    std::printf("V269_%s_DSR_T2_NORMAL_AWARE_PTDE_ENVLOCAL_COMPILED_NOT_BOUND_PASS\n",entry);
  }
  reflection->Release();
  blob->Release();
  std::printf("V267_%s_NATIVE_INPUT_DUAL_TARGET_SIGNATURE_PASS\n",entry);
}

int main() {
  try {
    const auto env=compile(
      L"renderer-core/tools/v258/ptde_equipment_hemenvlerp_gpu_subcut.hlsl",
      "ENVIRONMENT_LOCAL",9u*16u,true);
    const auto term=compile(
      L"renderer-core/tools/v258/ptde_phn_terminal_gpu_subcut.hlsl",
      "PHN_TERMINAL",2u*16u,false);
    native_abi_fixture("rx33",false);
    native_abi_fixture("rx34",true);
    native_abi_fixture("rx35",false);
    native_abi_fixture("rx33_v268",false,true);
    native_abi_fixture("rx34_v268",true,true);
    native_abi_fixture("rx35_v268",false,true);
    save(env,"v258_ptde_env_local_subcut.dxbc");
    save(term,"v258_ptde_phn_terminal_subcut.dxbc");
    std::puts("V267_NATIVE_RX33_34_35_SIGNATURE_AND_V258_GPU_ISLAND_CONSTRUCTION_PASS");
    std::puts("V258_NOT_READY_FOR_LIVE_DSR_RECEIVER_BIND");
    return 0;
  }catch(const std::exception& e){
    std::fprintf(stderr,"V258_DXBC_ERROR %s\n",e.what());return 2;
  }
}
