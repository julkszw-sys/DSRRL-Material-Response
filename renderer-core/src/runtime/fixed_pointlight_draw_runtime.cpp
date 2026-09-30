#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/fixed_pointlight_draw_runtime.hpp"
#include "dsrrl/runtime/pointlight_ptde_source_runtime.hpp"
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
    std::uint64_t serial=0u,epoch=0u;
    std::array<f4,8> raw_q{};
    std::uint8_t valid_mask=0u,captured_count=0u;
};
struct gpu_payload {
    ID3D11Device *device=nullptr;
    ID3D11Buffer *buffer=nullptr;
    ID3D11ShaderResourceView *srv=nullptr;
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

// No process-wide producer publication. Cross-thread joins fail open.
std::atomic<std::uint64_t> g_snapshot_epoch{1u};
thread_local snapshot g_producer_snapshot{},g_draw_snapshot{};
thread_local std::uint64_t g_local_serial=0u,g_consumed_serial=0u;
std::mutex g_gpu_mutex;
std::unordered_map<ID3D11DeviceContext *,gpu_payload> g_gpu_by_context;

std::atomic_bool g_enabled{false};
std::atomic_bool g_quarantined{false};
std::atomic_bool g_restore_failed{false};
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
    void *source) noexcept
{
    try {
        if(!g_enabled.load() || g_quarantined.load() ||
           owner==nullptr || raw==nullptr || slot>=4u){
            g_producer_snapshot={};
            telemetry::hot_count(g_rejects);
            return;
        }

        std::array<float,8> donor_raw{};
        std::memcpy(donor_raw.data(),raw-4,sizeof(donor_raw));
        const bool donor_ready=pointlight_ptde_source::capture(source,g_base,donor_raw);
        f4 value{};
        std::memcpy(&value,donor_raw.data()+4,sizeof(value));
        if(!std::isfinite(value.x) || !std::isfinite(value.y) ||
           !std::isfinite(value.z) || !std::isfinite(value.w)){
            g_producer_snapshot={};
            telemetry::hot_count(g_rejects);
            return;
        }

        const auto owner_key=reinterpret_cast<std::uintptr_t>(owner);
        const auto epoch=g_snapshot_epoch.load(std::memory_order_acquire);
        if(slot==0u){
            g_producer_snapshot={};
            g_producer_snapshot.owner=owner_key;
            g_producer_snapshot.serial=++g_local_serial;
            g_producer_snapshot.epoch=epoch;
            telemetry::hot_count(g_restarts);
        }
        auto &current=g_producer_snapshot;
        if(!donor_ready || current.epoch!=epoch || current.owner!=owner_key || current.captured_count!=slot){
            telemetry::hot_count(g_rejects);
            current={};
            return;
        }
        current.raw_q[slot]=value;
        current.raw_q[4u+slot]={0.0f,0.0f,0.0f,donor_raw[3]};
        current.valid_mask=static_cast<std::uint8_t>(current.valid_mask | (1u<<slot));
        current.captured_count=static_cast<std::uint8_t>(slot+1u);

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

        // Retail 0x1401C0C80 initializes EDI=0x60 and increments EDI
        // once per selected light (INC EDI at 0x1401C0DC3). The output
        // addresses multiply that ordinal by a 16-byte stride; EDI itself is
        // not a byte offset. Therefore the semantic fixed-light slot is
        // exactly EDI-0x60. Shifting by four collapsed 0x60..0x63 to slot 0
        // and made a 2/4-light snapshot impossible.
        // capture_callback(owner=RBP, slot=EDI-0x60,
        //                  raw=original_rsp+0x30, source=RBX)
        emit(code,{0x48,0x8B,0xCD});             // mov rcx,rbp
        emit(code,{0x8B,0xD7});                  // mov edx,edi
        emit(code,{0x83,0xEA,0x60});             // sub edx,60h => fixed slot 0..3
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

void release_gpu(gpu_payload &gpu) noexcept {
    if(gpu.srv) gpu.srv->Release();
    if(gpu.buffer) gpu.buffer->Release();
    if(gpu.device) gpu.device->Release();
    gpu={};
}
ID3D11ShaderResourceView *realize_t19(const snapshot &selected,ID3D11DeviceContext *context) noexcept {
    ID3D11Device *device=nullptr; context->GetDevice(&device);
    if(!device) return nullptr;
    std::lock_guard<std::mutex> lock(g_gpu_mutex);
    decltype(g_gpu_by_context)::iterator entry;
    try { entry=g_gpu_by_context.try_emplace(context).first; }
    catch(...) { device->Release(); return nullptr; }
    auto &gpu=entry->second;
    if(gpu.device && gpu.device!=device) release_gpu(gpu);
    if(!gpu.buffer){
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth=sizeof(selected.raw_q);
        desc.Usage=D3D11_USAGE_DYNAMIC;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride=sizeof(f4);
        if(FAILED(device->CreateBuffer(&desc,nullptr,&gpu.buffer))){device->Release();return nullptr;}
        D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
        view_desc.Format=DXGI_FORMAT_UNKNOWN;
        view_desc.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;
        view_desc.Buffer.NumElements=8u;
        if(FAILED(device->CreateShaderResourceView(gpu.buffer,&view_desc,&gpu.srv))){
            release_gpu(gpu);device->Release();return nullptr;
        }
        gpu.device=device; device->AddRef();
        telemetry::hot_count(g_t19_create);
    }else telemetry::hot_count(g_t19_hit);
    device->Release();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    // One resource per recording context prevents another context's Map from
    // replacing the payload between prepare and bind. DISCARD owns each draw.
    if(FAILED(context->Map(gpu.buffer,0u,D3D11_MAP_WRITE_DISCARD,0u,&mapped)))return nullptr;
    std::memcpy(mapped.pData,selected.raw_q.data(),sizeof(selected.raw_q));
    context->Unmap(gpu.buffer,0u);
    gpu.srv->AddRef();return gpu.srv;
}
void clear_state() noexcept {
    g_snapshot_epoch.fetch_add(1u,std::memory_order_acq_rel);
    g_producer_snapshot={};g_draw_snapshot={};
    std::lock_guard<std::mutex> lock(g_gpu_mutex);
    for(auto &entry:g_gpu_by_context) release_gpu(entry.second);
    g_gpu_by_context.clear();
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

void fixed_pointlight_draw_runtime::selector_event(void *owner) noexcept {
    telemetry::hot_count(g_selector_seen);
    g_draw_snapshot={};
    if(!g_enabled.load() || g_quarantined.load() || !owner) return;
    const auto &selected=g_producer_snapshot;
    if(selected.owner!=reinterpret_cast<std::uintptr_t>(owner)||
       selected.epoch!=g_snapshot_epoch.load(std::memory_order_acquire)||
       !selected.serial||selected.serial<=g_consumed_serial){
        telemetry::hot_count(g_selector_stale);return;
    }
    g_draw_snapshot=selected;
    telemetry::hot_count(g_selector_match);
}
bool fixed_pointlight_draw_runtime::prepare_t19(
    ID3D11DeviceContext *context,std::uint8_t expected_count,
    prepared_fixed_pointlight_draw &prepared) noexcept {
    prepared={};
    if(!g_enabled.load()||g_quarantined.load()||!context||
       (expected_count!=2u&&expected_count!=4u))return false;
    const auto &selected=g_draw_snapshot;
    if(!selected.owner||selected.epoch!=g_snapshot_epoch.load(std::memory_order_acquire)||
       selected.serial<=g_consumed_serial||selected.captured_count!=expected_count||
       selected.valid_mask!=static_cast<std::uint8_t>((1u<<expected_count)-1u))return false;
    auto *srv=realize_t19(selected,context);
    if(!srv)return false;
    g_consumed_serial=selected.serial;
    prepared.t19=srv;
    prepared.producer_serial=selected.serial;
    prepared.captured_light_count=selected.captured_count;
    prepared.owner_verified=true;prepared.producer_serial_fresh=true;prepared.ready=true;
    telemetry::hot_count(g_requests);return true;
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
    g_draw_snapshot={};
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

    std::lock_guard<std::mutex> lock(g_gpu_mutex);
    for(auto it=g_gpu_by_context.begin();it!=g_gpu_by_context.end();){
        if(it->second.device==native){release_gpu(it->second);it=g_gpu_by_context.erase(it);}
        else ++it;
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
