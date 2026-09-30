// Exercise the real callback/selector/carrier without installing a game hook.
#include "../src/runtime/fixed_pointlight_draw_runtime.cpp"
#include <cstdlib>
#include <thread>
#include <iostream>
namespace dsrrl::runtime::flver_identity_transport {
hook_status status() noexcept { return {}; }
}
using namespace dsrrl::runtime;
static void check(bool value) { if(!value) std::abort(); }
template<class T> void put(std::uint8_t *p,std::size_t offset,T value) { std::memcpy(p+offset,&value,sizeof(value)); }
int main() {
    ID3D11Device *device=nullptr; ID3D11DeviceContext *context=nullptr,*deferred=nullptr;
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    check(SUCCEEDED(device->CreateDeferredContext(0,&deferred)));
    std::array<std::uint8_t,0x730> param{};
    put<std::uint16_t>(param.data(),0xa,64);
    for(std::uint32_t i=0;i<64;++i) { put(param.data(),0x30+12*i,i);put(param.data(),0x34+12*i,0x330+16*i); }
    std::memcpy(param.data()+0x330,pointlight_donors::banks[0].dsr.data(),1024);
    std::array<std::uint8_t,0x20> bank{};put(bank.data(),0x18,reinterpret_cast<std::uintptr_t>(param.data()));
    std::array<std::uintptr_t,13> vt{};
    g_base=0x140000000u;vt[12]=g_base+0x55bc00u;
    std::array<std::uint8_t,0x60> node{};
    put(node.data(),0,reinterpret_cast<std::uintptr_t>(vt.data()));
    put(node.data(),0x50,reinterpret_cast<std::uintptr_t>(bank.data()));put<std::int32_t>(node.data(),0x58,8);
    const std::array<float,8> host={1,2,3,1,1,1,1,5};
    void *owner=reinterpret_cast<void*>(0x12340u);
    g_enabled.store(true); fixed_pointlight_draw_runtime runtime;
    auto publish=[&] { for(unsigned i=0;i<2;++i)capture_callback(owner,i,host.data()+4,node.data());runtime.selector_event(owner); };
    prepared_fixed_pointlight_draw prepared;
    publish();check(runtime.prepare_t19(context,2,prepared));auto *first=prepared.t19;
    runtime.release_prepared_draw(prepared);
    runtime.selector_event(owner);check(!runtime.prepare_t19(context,2,prepared));
    publish();check(runtime.prepare_t19(context,2,prepared));check(prepared.t19==first);runtime.release_prepared_draw(prepared);
    publish();check(runtime.prepare_t19(deferred,2,prepared));check(prepared.t19!=first);runtime.release_prepared_draw(prepared);
    check(g_gpu_by_context.size()==2);
    // Another thread cannot borrow this thread's source, even with same owner.
    std::thread other([&]{runtime.selector_event(owner);check(!g_draw_snapshot.owner);});other.join();
    publish();clear_state();check(!runtime.prepare_t19(context,2,prepared));
    // An unknown bank invalidates the entire selected set.
    param[0x330+13*16+14]^=1;publish();check(!runtime.prepare_t19(context,2,prepared));
    clear_state();g_enabled.store(false);deferred->Release();context->Release();device->Release();
    std::cout<<"FIXED_POINTLIGHT_TRANSPORT_PASS: same-thread, stale rejection, unknown-bank fail-open, per-context GPU reuse\n";
}
