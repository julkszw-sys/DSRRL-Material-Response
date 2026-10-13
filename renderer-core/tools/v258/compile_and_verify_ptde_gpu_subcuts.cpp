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
int main() {
  try {
    const auto env=compile(
      L"renderer-core/tools/v258/ptde_equipment_hemenvlerp_gpu_subcut.hlsl",
      "ENVIRONMENT_LOCAL",9u*16u,true);
    const auto term=compile(
      L"renderer-core/tools/v258/ptde_phn_terminal_gpu_subcut.hlsl",
      "PHN_TERMINAL",2u*16u,false);
    save(env,"v258_ptde_env_local_subcut.dxbc");
    save(term,"v258_ptde_phn_terminal_subcut.dxbc");
    std::puts("V258_DXBC_RESOURCE_ABI_AND_2_SHADER_CONSTRUCTION_PASS");
    std::puts("V258_NOT_READY_FOR_LIVE_DSR_RECEIVER_BIND");
    return 0;
  }catch(const std::exception& e){
    std::fprintf(stderr,"V258_DXBC_ERROR %s\n",e.what());return 2;
  }
}
