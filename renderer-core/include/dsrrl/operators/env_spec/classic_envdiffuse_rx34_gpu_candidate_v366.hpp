#pragma once
// V366: exact RX34 operator SHADER staging only; no stock draw modification.
// The caller must have authenticated the original 19556-byte RX34 DXBC and
// generated the 19604-byte independent shader candidate with V365.
#include <d3d11.h>
#include <reshade.hpp>
#include <cstdint>
#include <mutex>
#include <vector>
namespace dsrrl::operators::env_spec::classic_rx34_gpu_v366 {
enum class verdict { ready, missing, wrong_device, failed };
class cache final {
public:
    cache()=default;
    ~cache() { reset_all(); }
    cache(const cache&)=delete;
    cache& operator=(const cache&)=delete;

    verdict stage(reshade::api::device *host,
                  const std::vector<std::uint8_t>& bytecode,
                  std::uint32_t& native_hr) noexcept
    {
        native_hr=0u;
        if(host==nullptr ||
           host->get_api()!=reshade::api::device_api::d3d11 ||
           bytecode.size()!=19604u)
            return verdict::missing;
        auto *native=static_cast<ID3D11Device *>(host->get_native());
        if(native==nullptr) return verdict::missing;
        std::lock_guard<std::mutex> guard(lock_);
        if(owner_!=nullptr) return owner_==native && shader_!=nullptr ?
            verdict::ready : verdict::wrong_device;
        ID3D11PixelShader *new_shader=nullptr;
        const auto result=native->CreatePixelShader(
            bytecode.data(),bytecode.size(),nullptr,&new_shader);
        native_hr=static_cast<std::uint32_t>(result);
        if(FAILED(result) || new_shader==nullptr) {
            if(new_shader!=nullptr)new_shader->Release();
            return verdict::failed;
        }
        native->AddRef();
        owner_=native;
        shader_=new_shader;
        return verdict::ready;
    }
    void release_device(reshade::api::device *host) noexcept
    {
        if(host==nullptr ||
           host->get_api()!=reshade::api::device_api::d3d11)
            return;
        auto *native=static_cast<ID3D11Device *>(host->get_native());
        std::lock_guard<std::mutex> guard(lock_);
        if(native!=nullptr && native==owner_) release_unlocked();
    }
private:
    void release_unlocked() noexcept {
        if(shader_!=nullptr) {shader_->Release();shader_=nullptr;}
        if(owner_!=nullptr) {owner_->Release();owner_=nullptr;}
    }
    void reset_all() noexcept {
        std::lock_guard<std::mutex> guard(lock_);
        release_unlocked();
    }
    std::mutex lock_;
    ID3D11Device *owner_=nullptr;
    ID3D11PixelShader *shader_=nullptr;
};
} // namespace dsrrl::operators::env_spec::classic_rx34_gpu_v366
