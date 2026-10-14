#pragma once
// V274B DIAGNOSTIC ONLY. Native D3D11 CPU writer shadow for actual deferred
// contexts. No modification of CPU data, CB, shader, SRV or drawing commands.
// This header lives in tools/ and is placed in include/ ONLY transiently while
// building one addon, then deleted from the build workspace.
#include <reshade.hpp>
#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <cstdio>

namespace dsrrl::runtime::v274b {
constexpr std::size_t k_prefix_bytes=103u*16u;
constexpr std::size_t k_max_entries=512u;
struct pending_map {
    const void *data=nullptr;
    DWORD tid=0u;
};
struct snapshot {
    std::array<std::uint8_t,k_prefix_bytes> data{};
    DWORD writer_tid=0u;
    std::uint64_t epoch=0;
    std::uint32_t source=0; // 1=Unmap; 2=immediate Update; 3=deferred Update
};
inline std::mutex g_mutex;
inline std::unordered_map<std::uint64_t,pending_map> g_pending;
inline std::unordered_map<std::uint64_t,snapshot> g_shadows;
inline std::atomic<std::uint64_t> g_epoch{0};
inline std::atomic<std::uint64_t> g_map_calls{0};
inline std::atomic<std::uint64_t> g_map_cbs{0};
inline std::atomic<std::uint64_t> g_unmap_cbs{0};
inline std::atomic<std::uint64_t> g_update_cbs{0};

inline bool owner_cb(const reshade::api::resource res,
                    D3D11_BUFFER_DESC &desc) noexcept {
    if(res.handle==0u) return false;
    // ReShade's documented D3D11 native resource handle is an ID3D11Buffer *.
    const auto *ptr=reinterpret_cast<ID3D11Buffer *>(
        static_cast<std::uintptr_t>(res.handle));
    if(ptr==nullptr) return false;
    // Avoid querying non-buffers: mapped events are buffer-only and our
    // Update callbacks verify the D3D11 dimension first.
    auto *resource=const_cast<ID3D11Buffer *>(ptr);
    D3D11_RESOURCE_DIMENSION type=D3D11_RESOURCE_DIMENSION_UNKNOWN;
    resource->GetType(&type);
    if(type!=D3D11_RESOURCE_DIMENSION_BUFFER) return false;
    resource->GetDesc(&desc);
    return (desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER)!=0u &&
           desc.ByteWidth >= k_prefix_bytes &&
           desc.ByteWidth <= 65536u;
}
inline void map_buffer(
    reshade::api::device *,reshade::api::resource resource,
    std::uint64_t offset,std::uint64_t,
    reshade::api::map_access access,void **data) noexcept {
    g_map_calls.fetch_add(1,std::memory_order_relaxed);
    if(offset != 0u || data==nullptr || *data==nullptr ||
       access==reshade::api::map_access::read_only) return;
    D3D11_BUFFER_DESC desc{};
    if(!owner_cb(resource,desc)) return;
    g_map_cbs.fetch_add(1,std::memory_order_relaxed);
    std::lock_guard<std::mutex> guard(g_mutex);
    if(g_pending.size()>=k_max_entries) g_pending.clear();
    g_pending[resource.handle]={*data,GetCurrentThreadId()};
}
inline void unmap_buffer(
    reshade::api::device *,reshade::api::resource resource) noexcept {
    std::lock_guard<std::mutex> guard(g_mutex);
    auto it=g_pending.find(resource.handle);
    if(it==g_pending.end()) return;
    const pending_map p=it->second;
    g_pending.erase(it);
    if(p.data==nullptr) return;
    snapshot s{};
    // ReShade dispatches before the original ID3D11DeviceContext::Unmap.
    std::memcpy(s.data.data(),p.data,k_prefix_bytes);
    s.writer_tid=p.tid;
    s.source=1u;
    s.epoch=g_epoch.fetch_add(1,std::memory_order_relaxed)+1u;
    if(g_shadows.size()>=k_max_entries) g_shadows.clear();
    g_shadows[resource.handle]=s;
    g_unmap_cbs.fetch_add(1,std::memory_order_relaxed);
}
inline void inspect_update(const void *data,reshade::api::resource res,
                           std::uint64_t offset,std::uint64_t size,
                           std::uint32_t source) noexcept {
    if(data==nullptr || offset!=0u) return;
    D3D11_BUFFER_DESC desc{};
    if(!owner_cb(res,desc)) return;
    if(size!=UINT64_MAX && size < k_prefix_bytes) return;
    snapshot s{};
    std::memcpy(s.data.data(),data,k_prefix_bytes);
    s.writer_tid=GetCurrentThreadId();
    s.source=source;
    s.epoch=g_epoch.fetch_add(1,std::memory_order_relaxed)+1u;
    std::lock_guard<std::mutex> guard(g_mutex);
    if(g_shadows.size()>=k_max_entries) g_shadows.clear();
    g_shadows[res.handle]=s;
    g_update_cbs.fetch_add(1,std::memory_order_relaxed);
}
inline bool update_immediate(reshade::api::device *,const void *data,
                             reshade::api::resource res,
                             std::uint64_t off,std::uint64_t size) noexcept {
    inspect_update(data,res,off,size,2u);
    return false; // never block/modify stock D3D11 update
}
inline bool update_deferred(reshade::api::command_list *,const void *data,
                            reshade::api::resource res,
                            std::uint64_t off,std::uint64_t size) noexcept {
    inspect_update(data,res,off,size,3u);
    return false; // never block/modify stock D3D11 update
}
inline void destroy_resource(reshade::api::device *,
                             reshade::api::resource res) noexcept {
    std::lock_guard<std::mutex> guard(g_mutex);
    g_pending.erase(res.handle);
    g_shadows.erase(res.handle);
}
struct observation {
    bool b0_bound=false;
    bool has_shadow=false;
    bool same_thread=false;
    bool context1=false;
    UINT first=0;
    UINT count=0;
    std::uint32_t writer_source=0;
    DWORD writer_tid=0;
    std::uint64_t epoch=0;
    float cb85=0,cb100w=0,cb102=0;
    float cb10[3]{};
    // V274C: full Fog c12 and scattering c13..20 CPU-producer bytes.
    std::array<float,36u> atmosphere{};
    const char *status="B0_ABSENT";
};
inline observation observe(ID3D11DeviceContext *context) noexcept {
    observation out{};
    if(context==nullptr) return out;
    ID3D11Buffer *cb0=nullptr;
    ID3D11DeviceContext1 *ctx1=nullptr;
    if(SUCCEEDED(context->QueryInterface(
            __uuidof(ID3D11DeviceContext1),
            reinterpret_cast<void **>(&ctx1))) && ctx1!=nullptr) {
        out.context1=true;
        ctx1->PSGetConstantBuffers1(
            0u,1u,&cb0,&out.first,&out.count);
        ctx1->Release();
    } else {
        context->PSGetConstantBuffers(0u,1u,&cb0);
    }
    if(cb0==nullptr) return out;
    out.b0_bound=true;
    if(out.first!=0u || (out.context1 &&
        out.count!=0u && out.count<103u)) {
        out.status="B0_OFFSET_OR_WINDOW_UNKNOWN";
        cb0->Release();
        return out;
    }
    const auto key=static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(cb0));
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        const auto it=g_shadows.find(key);
        if(it!=g_shadows.end()) {
            auto &s=it->second;
            out.has_shadow=true;
            out.epoch=s.epoch;
            out.writer_tid=s.writer_tid;
            out.writer_source=s.source;
            out.same_thread=s.writer_tid==GetCurrentThreadId();
            auto scalar=[&](std::size_t reg,std::size_t lane) noexcept {
                float f=0.0f;
                std::memcpy(&f,s.data.data()+reg*16u+lane*4u,4u);
                return f;
            };
            out.cb85=scalar(85,0);
            out.cb100w=scalar(100,3);
            out.cb102=scalar(102,0);
            for(std::size_t i=0;i<3u;++i) out.cb10[i]=scalar(10,i);
            for(std::size_t reg=12u; reg<=20u; ++reg)
                for(std::size_t lane=0u; lane<4u; ++lane)
                    out.atmosphere[(reg-12u)*4u+lane]=scalar(reg,lane);
            // Same-thread is only a conservative writer-order witness.
            // This is NOT a GPU readback, exact command-list replay proof,
            // or pixel match. Cross-thread is especially non-authoritative.
            out.status=out.same_thread
                ? "CPU_WRITER_SAME_THREAD_ORDER_ONLY"
                : "CPU_WRITER_CROSS_THREAD_UNVERIFIED";
        } else {
            out.status="CB0_NO_CPU_WRITER_SHADOW";
        }
    }
    cb0->Release();
    return out;
}
inline void log_prepared(ID3D11DeviceContext *context,
                        std::uint32_t rx,
                        std::uint32_t route,
                        std::uint64_t mtd_hash,
                        std::uint64_t flver_hash,
                        std::uint32_t slot,float beta,
                        float c101) noexcept {
    if(context==nullptr || rx<33u || rx>35u ||
       !(beta>0.0f && beta<1.0f)) return;
    static std::array<std::atomic<std::uint64_t>,96> seen{};
    const std::uint32_t bucket=
        static_cast<std::uint32_t>(beta*4.0f);
    std::uint64_t key=mtd_hash ^
        (flver_hash * 0x9e3779b97f4a7c15ull) ^
        (static_cast<std::uint64_t>(route+1u)<<33u) ^
        (static_cast<std::uint64_t>(slot+1u)<<12u) ^
        (static_cast<std::uint64_t>(rx)<<6u) ^ bucket;
    if(key==0u) key=1u;
    bool fresh=false;
    for(auto &v:seen) {
        auto old=v.load(std::memory_order_relaxed);
        if(old==key) return;
        if(old==0u && v.compare_exchange_strong(
            old,key,std::memory_order_relaxed)) {
            fresh=true;break;
        }
    }
    if(!fresh) return;
    const auto o=observe(context);
    char line[1350]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V274B DEFERRED CB0 CPU SHADOW] "
        "rx=%u route=%u mtd=%016llx flver=%016llx "
        "slot=%u beta=%.8g c101=%.8g "
        "cb0_bound=%u shadow=%u authority=%s "
        "ctx1=%u first=%u count=%u "
        "writer_source=%u writer_tid=%lu reader_tid=%lu same_thread=%u "
        "epoch=%llu cb85=%.9g cb10=%.9g,%.9g,%.9g "
        "cb100w=%.9g cb102=%.9g "
        "map_all=%llu map_cb=%llu unmap_cb=%llu update_cb=%llu "
        "stage=PREPARED_ONLY GPU_READ=NO PIXEL=OPEN",
        static_cast<unsigned>(rx),
        static_cast<unsigned>(route),
        static_cast<unsigned long long>(mtd_hash),
        static_cast<unsigned long long>(flver_hash),
        static_cast<unsigned>(slot),
        static_cast<double>(beta),
        static_cast<double>(c101),
        o.b0_bound ? 1u:0u, o.has_shadow?1u:0u,
        o.status,
        o.context1?1u:0u,
        static_cast<unsigned>(o.first),
        static_cast<unsigned>(o.count),
        static_cast<unsigned>(o.writer_source),
        static_cast<unsigned long>(o.writer_tid),
        static_cast<unsigned long>(GetCurrentThreadId()),
        o.same_thread?1u:0u,
        static_cast<unsigned long long>(o.epoch),
        static_cast<double>(o.cb85),
        static_cast<double>(o.cb10[0]),
        static_cast<double>(o.cb10[1]),
        static_cast<double>(o.cb10[2]),
        static_cast<double>(o.cb100w),
        static_cast<double>(o.cb102),
        static_cast<unsigned long long>(
            g_map_calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_map_cbs.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_unmap_cbs.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_update_cbs.load(std::memory_order_relaxed)));
    reshade::log::message(reshade::log::level::info,line);
    // V274C: second bounded log line from ALREADY captured same CPU shadow.
    // These are material-local Fog + LS *producer values*, never a pixel
    // result or proof that deferred command-list execution retained them.
    char atmosphere_line[1600]{};
    int written=std::snprintf(atmosphere_line,sizeof(atmosphere_line),
        "[DSRRL V274C ATMOS CB0] rx=%u route=%u mtd=%016llx "
        "flver=%016llx slot=%u beta=%.9g shadow=%u same_thread=%u "
        "epoch=%llu producer=%u authority=CPU_WRITER_ONLY "
        "gpu_cb=UNOBSERVED pixel=OPEN",
        static_cast<unsigned>(rx),
        static_cast<unsigned>(route),
        static_cast<unsigned long long>(mtd_hash),
        static_cast<unsigned long long>(flver_hash),
        static_cast<unsigned>(slot),
        static_cast<double>(beta),
        o.has_shadow ? 1u : 0u,
        o.same_thread ? 1u : 0u,
        static_cast<unsigned long long>(o.epoch),
        static_cast<unsigned>(o.writer_source));
    if(written>0 && static_cast<std::size_t>(written)<sizeof(atmosphere_line)) {
        std::size_t at=static_cast<std::size_t>(written);
        for(std::uint32_t reg=12u; reg<=20u; ++reg) {
            const std::size_t row=(reg-12u)*4u;
            const int n=std::snprintf(atmosphere_line+at,
                sizeof(atmosphere_line)-at,
                " cb%u=%+.9g,%+.9g,%+.9g,%+.9g",
                static_cast<unsigned>(reg),
                static_cast<double>(o.atmosphere[row+0u]),
                static_cast<double>(o.atmosphere[row+1u]),
                static_cast<double>(o.atmosphere[row+2u]),
                static_cast<double>(o.atmosphere[row+3u]));
            if(n<=0 || static_cast<std::size_t>(n)>=sizeof(atmosphere_line)-at) {
                at=0u;
                break;
            }
            at+=static_cast<std::size_t>(n);
        }
        if(at!=0u)
            reshade::log::message(reshade::log::level::info,atmosphere_line);
    }
}
} // namespace dsrrl::runtime::v274b
