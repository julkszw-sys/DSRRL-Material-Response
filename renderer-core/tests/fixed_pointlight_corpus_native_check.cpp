// Optional external-corpus audit. Game shader bytes are not stored in the repo.
// Usage: dsrrl_fixed_pointlight_corpus_native_check CORPUS_DIR [--warp]
#include "dsrrl/operators/point_light/fixed_local_specular_single_materializer.hpp"
#include <d3d11.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
int main(int argc,char **argv) {
    if(argc<2)return 2;
    ID3D11Device *device=nullptr;ID3D11DeviceContext *context=nullptr;
    const auto driver=argc>2&&std::string(argv[2])=="--warp"?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE;
    if(FAILED(D3D11CreateDevice(nullptr,driver,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)))return 3;
    dsrrl::core::feature_registry features;
    for(unsigned i=0;i<dsrrl::core::operator_count;++i)features.set(static_cast<dsrrl::core::operator_id>(i),true);
    unsigned applied=0,untouched=0,failed=0;
    for(const auto &entry:std::filesystem::directory_iterator(argv[1])) {
        if(!entry.is_regular_file()||entry.path().extension()!=".fpo")continue;
        std::ifstream in(entry.path(),std::ios::binary);
        std::vector<std::uint8_t> source((std::istreambuf_iterator<char>(in)),{}),output;
        const auto result=dsrrl::operators::point_light::materialize_fixed_local_specular_single(features,source.data(),source.size(),output);
        using status=dsrrl::operators::point_light::fixed_local_single_materialize_result;
        if(result.result==status::pass_not_fixed_local_specular){++untouched;continue;}
        if(result.result!=status::applied){++failed;continue;}
        // Flush the current member before entering the native parser so a
        // watchdog can attribute a regression that stalls inside the driver.
        std::cout<<entry.path().filename().string()<<std::endl;
        ID3D11PixelShader *shader=nullptr;
        if(FAILED(device->CreatePixelShader(output.data(),output.size(),nullptr,&shader)))++failed;
        else ++applied;
        if(shader)shader->Release();
    }
    context->Release();device->Release();
    std::cout<<"applied="<<applied<<" untouched="<<untouched<<" failed="<<failed<<std::endl;
    return applied==96u&&untouched==192u&&failed==0u?0:1;
}
