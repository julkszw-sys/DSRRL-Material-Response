// V311 source-only: WARP / D3DReflect gate for private V300 owner DXBC.
// This program does NOT distribute shaders, patch the game or prove pixels.
// Build (MSVC developer prompt): cl /nologo /std:c++17 /EHsc /W4 /WX /utf-8
// renderer-core/tools/v311/v311_warp_gate.cpp /Fe:v311-warp-gate.exe
// d3d11.lib d3dcompiler.lib
#include <Windows.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")
using Microsoft::WRL::ComPtr;

static bool check(ID3D11Device* dev,ID3D11DeviceContext* ctx,
                  const std::vector<std::uint8_t>& bytes,
                  unsigned expected_b12,const std::string& label) {
    if(bytes.size()<128 || std::memcmp(bytes.data(),"DXBC",4)!=0) return false;
    ComPtr<ID3D11ShaderReflection> refl;
    HRESULT hr=D3DReflect(bytes.data(),bytes.size(),
                         IID_PPV_ARGS(refl.GetAddressOf()));
    if(FAILED(hr) || !refl){std::cerr<<label<<" D3DReflect "<<std::hex<<hr<<"\n";return false;}
    D3D11_SHADER_DESC desc{};
    hr=refl->GetDesc(&desc);
    // D3DReflect validates the container; CreatePixelShader enforces PS stage.
    // Shader model major is encoded in version bits [7:4].
    if(FAILED(hr) || ((desc.Version >> 4u) & 0xFu) != 5u)return false;
    bool rt0=false,rt1=false;
    for(UINT i=0;i<desc.OutputParameters;++i){
        D3D11_SIGNATURE_PARAMETER_DESC param{};
        if(FAILED(refl->GetOutputParameterDesc(i,&param)))return false;
        if(param.SemanticName && _stricmp(param.SemanticName,"SV_Target")==0 && param.SemanticIndex==0u && param.Register==0u && param.Mask==15u)rt0=true;
        if(param.SemanticName && _stricmp(param.SemanticName,"SV_Target")==0 && param.SemanticIndex==1u && param.Register==1u && param.Mask==15u)rt1=true;
    }
    if(!rt0 || !rt1){std::cerr<<label<<" missing RT0 or Glow RT1\n";return false;}
    D3D11_SHADER_INPUT_BIND_DESC resource{};
    if(FAILED(refl->GetResourceBindingDescByName("DSRRL_MaterialCarrier",&resource)) ||
       resource.Type!=D3D_SIT_CBUFFER || resource.BindPoint!=12u ||
       resource.BindCount!=1u)return false;
    ID3D11ShaderReflectionConstantBuffer* cb=
        refl->GetConstantBufferByName("DSRRL_MaterialCarrier");
    D3D11_SHADER_BUFFER_DESC cbd{};
    if(!cb || FAILED(cb->GetDesc(&cbd)) || cbd.Size!=expected_b12)return false;
    ComPtr<ID3D11PixelShader> shader;
    hr=dev->CreatePixelShader(bytes.data(),bytes.size(),nullptr,shader.GetAddressOf());
    if(FAILED(hr) || !shader){std::cerr<<label<<" CreatePixelShader "<<std::hex<<hr<<"\n";return false;}
    D3D11_BUFFER_DESC bufferdesc{};
    bufferdesc.ByteWidth=96u; // R19 upload, Stable consumes the first 64 bytes.
    bufferdesc.Usage=D3D11_USAGE_DEFAULT;
    bufferdesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> buffer;
    if(FAILED(dev->CreateBuffer(&bufferdesc,nullptr,buffer.GetAddressOf())))return false;
    ID3D11Buffer* ptr=buffer.Get();
    ctx->PSSetConstantBuffers(12u,1u,&ptr);
    ctx->PSSetShader(shader.Get(),nullptr,0u);
    ComPtr<ID3D11Buffer> readback;
    ctx->PSGetConstantBuffers(12u,1u,readback.GetAddressOf());
    const bool bound=readback.Get()==buffer.Get();
    ptr=nullptr;
    ctx->PSSetShader(nullptr,nullptr,0u);
    ctx->PSSetConstantBuffers(12u,1u,&ptr);
    if(!bound)return false;
    std::cout<<"PASS "<<label<<" WARP CreatePixelShader RDEF_b12="
             <<expected_b12<<" physical_b12=96 RT0_RT1=1\n";
    return true;
}
int main(int argc,char** argv){
    constexpr D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL actual{};
    ComPtr<ID3D11Device> dev;
    ComPtr<ID3D11DeviceContext> ctx;
    const HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        levels,1,D3D11_SDK_VERSION,dev.GetAddressOf(),&actual,ctx.GetAddressOf());
    if(FAILED(hr)||!dev||!ctx||actual<D3D_FEATURE_LEVEL_11_0){std::cerr<<"WARP_DEVICE_FAIL\n";return 5;}
    if(argc==2 && std::string(argv[1])=="--self-test"){
        const char source[]=
            "cbuffer DSRRL_MaterialCarrier:register(b12){float4 v[6];};"
            "struct O{float4 color:SV_Target0;float4 glow:SV_Target1;};"
            "O main(float4 p:SV_Position){O o;o.color=v[0]+p.x*v[5];o.glow=v[1];return o;}";
        ComPtr<ID3DBlob> code,errors;
        const HRESULT h=D3DCompile(source,sizeof(source)-1,"v311-test",nullptr,nullptr,
            "main","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,
            code.GetAddressOf(),errors.GetAddressOf());
        if(FAILED(h)||!code){
            if(errors)std::cerr<<static_cast<const char*>(errors->GetBufferPointer());
            return 6;
        }
        const auto* first=static_cast<const std::uint8_t*>(code->GetBufferPointer());
        std::vector<std::uint8_t> bytes(first,first+code->GetBufferSize());
        return check(dev.Get(),ctx.Get(),bytes,96u,"SELFTEST")?0:7;
    }
    if(argc!=4){std::cerr<<"usage: v311-warp-gate <private-dxbc> 64|96 <stage_rx>\n";return 2;}
    unsigned expected=0;
    try{expected=static_cast<unsigned>(std::stoul(argv[2]));}catch(...){return 2;}
    if(expected!=64u && expected!=96u)return 2;
    std::ifstream in(std::filesystem::u8path(argv[1]),std::ios::binary);
    if(!in){std::cerr<<"Cannot open private DXBC\n";return 3;}
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    if(!check(dev.Get(),ctx.Get(),bytes,expected,argv[3])){std::cerr<<"OWNER_SHADER_FAIL\n";return 8;}
    return 0;
}
