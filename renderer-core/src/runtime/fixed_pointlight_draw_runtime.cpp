#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/fixed_pointlight_draw_runtime.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include "dsrrl/runtime/flver_identity_transport.hpp"

#include <Windows.h>
#include <d3d11.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace dsrrl::runtime {
namespace {

struct f4 {
    float x=0.0f;
    float y=0.0f;
    float z=0.0f;
    float w=0.0f;
};

struct snapshot {
    std::uintptr_t owner=0u;
    std::uint64_t serial=0u;
    std::array<f4,4> raw_q{};
    std::uint8_t valid_mask=0u;
    std::uint8_t captured_count=0u;
    mutable std::atomic_bool consumed{false};
    std::shared_ptr<std::atomic<std::uint64_t>> owner_consumed_serial{};

    mutable std::mutex gpu_mutex;
    mutable ID3D11Device *device=nullptr;
    mutable ID3D11Buffer *buffer=nullptr;
    mutable ID3D11ShaderResourceView *srv=nullptr;

    ~snapshot()
    {
        if(srv!=nullptr) srv->Release();
        if(buffer!=nullptr) buffer->Release();
        if(device!=nullptr) device->Release();
    }
};

struct capture_hook {
    void *target=nullptr;
    void *stub=nullptr;
    std::array<std::uint8_t,18> original{};
    bool patched=false;
};

constexpr std::uintptr_t k_capture_rva=0x1C0D05u;
constexpr std::array<std::uint8_t,18> k_capture_bytes={{
    0xF3,0x0F,0x10,0x64,0x24,0x38,
    0xF3,0x0F,0x10,0x4C,0x24,0x30,
    0xF3,0x0F,0x10,0x5C,0x24,0x34
}};

std::uintptr_t g_base=0u;
fixed_pointlight_draw_runtime *g_runtime=nullptr;
capture_hook g_hook{};

std::mutex g_mutex;
std::unordered_map<std::uintptr_t,std::shared_ptr<snapshot>> g_snapshots;
std::unordered_map<
    std::uintptr_t,
    std::shared_ptr<std::atomic<std::uint64_t>>>
    g_owner_consumed_serial;
std::atomic_bool g_have_snapshots{false};
std::atomic<std::uint64_t> g_snapshot_epoch{1u};

struct selector_snapshot_tls {
    std::uintptr_t owner=0u;
    std::uint64_t epoch=0u;
    std::shared_ptr<const snapshot> selected{};
    bool present=false;
};

thread_local std::shared_ptr<snapshot> g_producer_snapshot{};
thread_local std::shared_ptr<const snapshot> g_draw_snapshot{};
thread_local selector_snapshot_tls g_selector_cache{};

std::atomic_bool g_enabled{false};
std::atomic_bool g_quarantined{false};
std::atomic_bool g_restore_failed{false};
std::atomic<std::uint64_t> g_serial{0u};
std::atomic<std::uint64_t> g_captures{0u};
std::atomic<std::uint64_t> g_restarts{0u};
std::atomic<std::uint64_t> g_rejects{0u};
std::atomic<std::uint64_t> g_selector_seen{0u};
std::atomic<std::uint64_t> g_selector_match{0u};
std::atomic<std::uint64_t> g_selector_stale{0u};
std::atomic<std::uint64_t> g_t19_create{0u};
std::atomic<std::uint64_t> g_t19_hit{0u};
std::atomic<std::uint64_t> g_requests{0u};

bool readable_range(const void *ptr,std::size_t size) noexcept
{
    if(ptr==nullptr) return false;
    if(size==0u) return true;
    auto cursor=reinterpret_cast<std::uintptr_t>(ptr);
    const auto end=cursor+size;
    if(end<cursor) return false;
    while(cursor<end){
        MEMORY_BASIC_INFORMATION mbi{};
        if(VirtualQuery(reinterpret_cast<const void *>(cursor),&mbi,sizeof(mbi))!=sizeof(mbi))
            return false;
        if(mbi.State!=MEM_COMMIT || (mbi.Protect&PAGE_GUARD)!=0u)
            return false;
        const DWORD access=mbi.Protect&0xffu;
        const bool ok=
            access==PAGE_READONLY || access==PAGE_READWRITE ||
            access==PAGE_WRITECOPY || access==PAGE_EXECUTE_READ ||
            access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
        if(!ok) return false;
        const auto begin=reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const auto region_end=begin+mbi.RegionSize;
        if(region_end<=cursor || region_end<begin) return false;
        cursor=std::min(region_end,end);
    }
    return true;
}

bool write_bytes(void *dst,const void *src,std::size_t size) noexcept
{
    DWORD old=0u;
    if(!VirtualProtect(dst,size,PAGE_EXECUTE_READWRITE,&old))
        return false;
    std::memcpy(dst,src,size);
    const bool flushed=FlushInstructionCache(GetCurrentProcess(),dst,size)!=FALSE;
    DWORD ignored=0u;
    (void)VirtualProtect(dst,size,old,&ignored);
    return flushed;
}

void emit_u64(std::vector<std::uint8_t> &out,std::uint64_t value)
{
    for(unsigned i=0;i<8u;++i)
        out.push_back(static_cast<std::uint8_t>(value>>(i*8u)));
}

void emit(std::vector<std::uint8_t> &out,std::initializer_list<std::uint8_t> bytes)
{
    out.insert(out.end(),bytes.begin(),bytes.end());
}

void emit_store_xmm(std::vector<std::uint8_t> &out,std::uint8_t xmm,std::uint32_t disp)
{
    // movdqu [rsp+disp32], xmmN
    emit(out,{0xF3,0x0F,0x7F,
              static_cast<std::uint8_t>(0x84u | ((xmm&7u)<<3u)),
              0x24});
    for(unsigned i=0;i<4u;++i)
        out.push_back(static_cast<std::uint8_t>(disp>>(i*8u)));
}

void emit_load_xmm(std::vector<std::uint8_t> &out,std::uint8_t xmm,std::uint32_t disp)
{
    // movdqu xmmN, [rsp+disp32]
    emit(out,{0xF3,0x0F,0x6F,
              static_cast<std::uint8_t>(0x84u | ((xmm&7u)<<3u)),
              0x24});
    for(unsigned i=0;i<4u;++i)
        out.push_back(static_cast<std::uint8_t>(disp>>(i*8u)));
}

void __fastcall capture_callback(
    void *owner,
    std::uint32_t slot,
    const float *raw,
    void *) noexcept
{
    try {
        if(!g_enabled.load() || g_quarantined.load() ||
           owner==nullptr || raw==nullptr || slot>=4u){
            telemetry::hot_count(g_rejects);
            return;
        }

        f4 value{};
        std::memcpy(&value,raw,sizeof(value));
        if(!std::isfinite(value.x) || !std::isfinite(value.y) ||
           !std::isfinite(value.z) || !std::isfinite(value.w)){
            telemetry::hot_count(g_rejects);
            return;
        }

        const auto owner_key=reinterpret_cast<std::uintptr_t>(owner);

        if(slot==0u){
            auto next=std::make_shared<snapshot>();
            next->owner=owner_key;
            next->serial=g_serial.fetch_add(1u,std::memory_order_relaxed)+1u;
            g_producer_snapshot=std::move(next);
            telemetry::hot_count(g_restarts);
        }

        auto current=g_producer_snapshot;
        if(!current || current->owner!=owner_key ||
           current->captured_count!=slot){
            telemetry::hot_count(g_rejects);
            g_producer_snapshot.reset();
            return;
        }

        current->raw_q[slot]=value;
        current->valid_mask=static_cast<std::uint8_t>(
            current->valid_mask | (1u<<slot));
        current->captured_count=static_cast<std::uint8_t>(slot+1u);

        {
            std::lock_guard<std::mutex> lock(g_mutex);

            if(!current->owner_consumed_serial){
                auto &owner_serial=
                    g_owner_consumed_serial[owner_key];
                if(!owner_serial)
                    owner_serial=
                        std::make_shared<
                            std::atomic<std::uint64_t>>(0u);
                current->owner_consumed_serial=
                    owner_serial;
            }

            g_snapshots[owner_key]=current;
            g_snapshot_epoch.fetch_add(
                1u,
                std::memory_order_release);
            g_have_snapshots.store(
                true,
                std::memory_order_release);
        }

        telemetry::hot_count(g_captures);
    } catch (...) {
        telemetry::hot_count(g_rejects);
        g_quarantined.store(true);
    }
}

bool build_capture_stub(void *target,void *&stub_out) noexcept
{
    stub_out=nullptr;
    try{
        std::vector<std::uint8_t> code;
        code.reserve(320u);

        // Preserve flags too: the callback is arbitrary C++ and may clobber
        // condition codes consumed by the host after the stolen MOVSS block.
        // Site RSP is 16-byte aligned; pushfq + sub 108h restores 16-byte
        // call alignment while leaving 100h scratch + shadow space.
        emit(code,{0x9C}); // pushfq
        emit(code,{0x48,0x81,0xEC,0x08,0x01,0x00,0x00}); // sub rsp,108h

        // volatile GPR saves at +20..+50. Preserve RFLAGS separately at
        // +58 without changing call-site alignment.
        emit(code,{0x48,0x89,0x44,0x24,0x20}); // rax
        emit(code,{0x9C});                     // pushfq
        emit(code,{0x58});                     // pop rax
        emit(code,{0x48,0x89,0x84,0x24,0x58,0x00,0x00,0x00}); // [rsp+58]=flags
        emit(code,{0x48,0x89,0x4C,0x24,0x28}); // rcx
        emit(code,{0x48,0x89,0x54,0x24,0x30}); // rdx
        emit(code,{0x4C,0x89,0x44,0x24,0x38}); // r8
        emit(code,{0x4C,0x89,0x4C,0x24,0x40}); // r9
        emit(code,{0x4C,0x89,0x54,0x24,0x48}); // r10
        emit(code,{0x4C,0x89,0x5C,0x24,0x50}); // r11

        for(std::uint8_t i=0u;i<6u;++i)
            emit_store_xmm(code,i,0x60u+0x10u*i);

        // capture_callback(owner=RBP, slot=EDI-0x60,
        //                  raw=original_rsp+0x30, source=RBX)
        emit(code,{0x48,0x8B,0xCD});             // mov rcx,rbp
        emit(code,{0x8B,0xD7});                  // mov edx,edi
        emit(code,{0x83,0xEA,0x60});             // sub edx,60h
        emit(code,{0xC1,0xEA,0x04});             // shr edx,4 => fixed slot 0..3
        emit(code,{0x4C,0x8D,0x84,0x24,0x40,0x01,0x00,0x00}); // lea r8,[rsp+140h]
        emit(code,{0x4C,0x8B,0xCB});             // mov r9,rbx
        emit(code,{0x48,0xB8});
        emit_u64(code,reinterpret_cast<std::uint64_t>(&capture_callback));
        emit(code,{0xFF,0xD0});                  // call rax

        for(std::uint8_t i=0u;i<6u;++i)
            emit_load_xmm(code,i,0x60u+0x10u*i);

        emit(code,{0x4C,0x8B,0x5C,0x24,0x50}); // r11
        emit(code,{0x4C,0x8B,0x54,0x24,0x48}); // r10
        emit(code,{0x4C,0x8B,0x4C,0x24,0x40}); // r9
        emit(code,{0x4C,0x8B,0x44,0x24,0x38}); // r8
        emit(code,{0x48,0x8B,0x54,0x24,0x30}); // rdx
        emit(code,{0x48,0x8B,0x4C,0x24,0x28}); // rcx
        emit(code,{0x48,0x8B,0x84,0x24,0x58,0x00,0x00,0x00}); // rax=flags
        emit(code,{0x50});                     // push rax
        emit(code,{0x9D});                     // popfq
        emit(code,{0x48,0x8B,0x44,0x24,0x20}); // rax
        emit(code,{0x48,0x81,0xC4,0x08,0x01,0x00,0x00}); // add rsp,108h
        emit(code,{0x9D}); // popfq

        // Stolen instructions, byte exact.
        code.insert(code.end(),k_capture_bytes.begin(),k_capture_bytes.end());

        // Absolute indirect JMP target+18.
        emit(code,{0xFF,0x25,0x00,0x00,0x00,0x00});
        emit_u64(code,reinterpret_cast<std::uint64_t>(
            static_cast<std::uint8_t *>(target)+k_capture_bytes.size()));

        void *stub=VirtualAlloc(
            nullptr,
            code.size(),
            MEM_COMMIT|MEM_RESERVE,
            PAGE_EXECUTE_READWRITE);
        if(stub==nullptr) return false;
        std::memcpy(stub,code.data(),code.size());
        if(FlushInstructionCache(GetCurrentProcess(),stub,code.size())==FALSE){
            VirtualFree(stub,0,MEM_RELEASE);
            return false;
        }
        stub_out=stub;
        return true;
    }catch(...){
        return false;
    }
}

bool install_capture_hook() noexcept
{
    auto *target=reinterpret_cast<std::uint8_t *>(g_base+k_capture_rva);
    if(!readable_range(target,k_capture_bytes.size()) ||
       std::memcmp(target,k_capture_bytes.data(),k_capture_bytes.size())!=0)
        return false;

    void *stub=nullptr;
    if(!build_capture_stub(target,stub))
        return false;

    std::array<std::uint8_t,18> patch{};
    patch.fill(0x90u);
    patch[0]=0xFFu; patch[1]=0x25u;
    const auto address=reinterpret_cast<std::uint64_t>(stub);
    std::memcpy(patch.data()+6u,&address,sizeof(address));

    if(!write_bytes(target,patch.data(),patch.size())){
        VirtualFree(stub,0,MEM_RELEASE);
        return false;
    }

    g_hook.target=target;
    g_hook.stub=stub;
    g_hook.original=k_capture_bytes;
    g_hook.patched=true;
    return true;
}

bool restore_capture_hook() noexcept
{
    bool ok=true;
    if(g_hook.patched && g_hook.target!=nullptr){
        ok=write_bytes(
            g_hook.target,
            g_hook.original.data(),
            g_hook.original.size());
    }
    if(ok && g_hook.stub!=nullptr)
        VirtualFree(g_hook.stub,0,MEM_RELEASE);
    if(ok)
        g_hook={};
    return ok;
}

ID3D11ShaderResourceView *realize_t19(
    const std::shared_ptr<const snapshot> &selected,
    ID3D11Device *device) noexcept
{
    if(!selected || device==nullptr)
        return nullptr;

    std::lock_guard<std::mutex> lock(selected->gpu_mutex);
    if(selected->device!=nullptr && selected->device!=device)
        return nullptr;

    if(selected->srv!=nullptr){
        selected->srv->AddRef();
        telemetry::hot_count(g_t19_hit);
        return selected->srv;
    }

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth=static_cast<UINT>(sizeof(selected->raw_q));
    desc.Usage=D3D11_USAGE_IMMUTABLE;
    desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride=sizeof(f4);

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem=selected->raw_q.data();

    ID3D11Buffer *buffer=nullptr;
    if(FAILED(device->CreateBuffer(&desc,&init,&buffer)) || buffer==nullptr)
        return nullptr;

    D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
    view_desc.Format=DXGI_FORMAT_UNKNOWN;
    view_desc.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;
    view_desc.Buffer.FirstElement=0u;
    view_desc.Buffer.NumElements=4u;

    ID3D11ShaderResourceView *srv=nullptr;
    if(FAILED(device->CreateShaderResourceView(buffer,&view_desc,&srv)) || srv==nullptr){
        buffer->Release();
        return nullptr;
    }

    if(selected->device==nullptr){
        selected->device=device;
        device->AddRef();
    }
    selected->buffer=buffer;
    selected->srv=srv;
    srv->AddRef();
    telemetry::hot_count(g_t19_create);
    return srv;
}

void clear_state() noexcept
{
    g_producer_snapshot.reset();
    g_draw_snapshot.reset();
    std::lock_guard<std::mutex> lock(g_mutex);
    g_snapshots.clear();
    g_owner_consumed_serial.clear();
    g_snapshot_epoch.fetch_add(
        1u,
        std::memory_order_release);
    g_selector_cache={};
    g_have_snapshots.store(
        false,
        std::memory_order_release);
}

} // namespace

void fixed_pointlight_selector_event_bridge(void *owner) noexcept
{
    if(g_runtime!=nullptr)
        g_runtime->selector_event(owner);
}

bool fixed_pointlight_draw_runtime::install() noexcept
{
    if(g_enabled.load())
        return g_runtime==this;
    if(g_runtime!=nullptr && g_runtime!=this)
        return false;

    const auto selector=flver_identity_transport::status();
    if(!selector.provenance_ok || !selector.selector_armed)
        return false;

    g_base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(g_base==0u)
        return false;

    g_runtime=this;
    if(!install_capture_hook()){
        g_runtime=nullptr;
        g_base=0u;
        return false;
    }

    g_quarantined.store(false);
    g_restore_failed.store(false);
    g_enabled.store(true);
    return true;
}

void fixed_pointlight_draw_runtime::uninstall() noexcept
{
    g_enabled.store(false);
    consume_draw_selection();
    clear_state();

    if(!restore_capture_hook()){
        g_restore_failed.store(true);
        g_quarantined.store(true);
        return;
    }

    g_runtime=nullptr;
    g_base=0u;
}

void fixed_pointlight_draw_runtime::selector_event(void *owner) noexcept
{
    telemetry::hot_count(g_selector_seen);
    g_draw_snapshot.reset();

    if(!g_enabled.load() || g_quarantined.load() || owner==nullptr)
        return;

    // The selector bridge is shared with Upper/Lower, so this callback can be
    // invoked at very high frequency even when the fixed PointLight producer
    // has never published a snapshot. Avoid a guaranteed mutex/map miss in
    // that state. Publication sets this flag under the same map lock before
    // unlock; clearing resets it under the lock after erasing the map.
    if(!g_have_snapshots.load(
            std::memory_order_acquire)){
        telemetry::hot_count(g_selector_stale);
        return;
    }

    const auto key=reinterpret_cast<std::uintptr_t>(owner);
    const auto epoch=
        g_snapshot_epoch.load(
            std::memory_order_acquire);
    std::shared_ptr<const snapshot> selected{};

    if(g_selector_cache.owner==key &&
       g_selector_cache.epoch==epoch){
        if(!g_selector_cache.present){
            telemetry::hot_count(g_selector_stale);
            return;
        }
        selected=g_selector_cache.selected;
    }else{
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto it=g_snapshots.find(key);
        if(it!=g_snapshots.end())
            selected=it->second;

        g_selector_cache={
            key,
            g_snapshot_epoch.load(
                std::memory_order_relaxed),
            selected,
            selected!=nullptr
        };
    }

    if(!selected){
        telemetry::hot_count(g_selector_stale);
        return;
    }

    g_draw_snapshot=std::move(selected);
    telemetry::hot_count(g_selector_match);
}

bool fixed_pointlight_draw_runtime::prepare_t19(
    ID3D11DeviceContext *context,
    std::uint8_t expected_count,
    prepared_fixed_pointlight_draw &prepared) noexcept
{
    prepared={};
    if(!g_enabled.load() || g_quarantined.load() ||
       context==nullptr || !g_draw_snapshot ||
       (expected_count!=2u && expected_count!=4u))
        return false;

    const auto selected=g_draw_snapshot;
    const auto expected_mask=static_cast<std::uint8_t>((1u<<expected_count)-1u);
    if(selected->captured_count!=expected_count ||
       selected->valid_mask!=expected_mask ||
       !selected->owner_consumed_serial)
        return false;

    const auto last_consumed=
        selected->owner_consumed_serial->load(
            std::memory_order_acquire);
    if(last_consumed>=selected->serial){
        telemetry::hot_count(g_selector_stale);
        return false;
    }

    bool expected_consumed=false;
    if(!selected->consumed.compare_exchange_strong(
            expected_consumed,
            true,
            std::memory_order_acq_rel,
            std::memory_order_acquire)){
        telemetry::hot_count(g_selector_stale);
        return false;
    }

    ID3D11Device *device=nullptr;
    context->GetDevice(&device);
    if(device==nullptr){
        selected->consumed.store(
            false,
            std::memory_order_release);
        return false;
    }

    auto *srv=realize_t19(selected,device);
    device->Release();
    if(srv==nullptr){
        selected->consumed.store(
            false,
            std::memory_order_release);
        return false;
    }

    auto observed=
        selected->owner_consumed_serial->load(
            std::memory_order_acquire);
    for(;;){
        if(observed>=selected->serial){
            srv->Release();
            telemetry::hot_count(g_selector_stale);
            return false;
        }

        if(selected->owner_consumed_serial->
                compare_exchange_weak(
                    observed,
                    selected->serial,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
            break;
    }

    prepared.t19=srv;
    prepared.producer_serial=selected->serial;
    prepared.captured_light_count=selected->captured_count;
    prepared.owner_verified=true;
    prepared.producer_serial_fresh=true;
    prepared.ready=true;
    telemetry::hot_count(g_requests);
    return true;
}

void fixed_pointlight_draw_runtime::release_prepared_draw(
    prepared_fixed_pointlight_draw &prepared) noexcept
{
    if(prepared.t19!=nullptr)
        prepared.t19->Release();
    prepared={};
}

void fixed_pointlight_draw_runtime::consume_draw_selection() noexcept
{
    g_draw_snapshot.reset();
}

void fixed_pointlight_draw_runtime::on_destroy_device(
    reshade::api::device *device) noexcept
{
    if(device==nullptr ||
       device->get_api()!=reshade::api::device_api::d3d11)
        return;

    auto *native=reinterpret_cast<ID3D11Device *>(device->get_native());
    if(native==nullptr)
        return;

    std::lock_guard<std::mutex> lock(g_mutex);
    for(auto &entry:g_snapshots){
        auto &s=entry.second;
        if(!s) continue;
        std::lock_guard<std::mutex> gpu(s->gpu_mutex);
        if(s->device!=native)
            continue;
        if(s->srv!=nullptr){s->srv->Release();s->srv=nullptr;}
        if(s->buffer!=nullptr){s->buffer->Release();s->buffer=nullptr;}
        if(s->device!=nullptr){s->device->Release();s->device=nullptr;}
    }
}

fixed_pointlight_telemetry fixed_pointlight_draw_runtime::telemetry() const noexcept
{
    return {
        g_captures.load(),
        g_restarts.load(),
        g_rejects.load(),
        g_selector_seen.load(),
        g_selector_match.load(),
        g_selector_stale.load(),
        g_t19_create.load(),
        g_t19_hit.load(),
        g_requests.load(),
        g_hook.patched,
        g_quarantined.load(),
        g_restore_failed.load()
    };
}

void fixed_pointlight_draw_runtime::reset() noexcept
{
    clear_state();
    g_selector_cache={};
    g_serial.store(0u);
    g_captures.store(0u);
    g_restarts.store(0u);
    g_rejects.store(0u);
    g_selector_seen.store(0u);
    g_selector_match.store(0u);
    g_selector_stale.store(0u);
    g_t19_create.store(0u);
    g_t19_hit.store(0u);
    g_requests.store(0u);
}

} // namespace dsrrl::runtime
