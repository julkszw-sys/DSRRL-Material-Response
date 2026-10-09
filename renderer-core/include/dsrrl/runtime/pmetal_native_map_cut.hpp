#pragma once
// PR292 single diagnostic: native D3D11 Map/Unmap -> exact PS b0 resource/context.
// Read-only observation. Nothing is written to mapped memory, shaders or D3D state.
// Vtable interception is confined to this opt-in diagnostic build; foreign hooks
// fail open. Native Map receipt does not imply GPU PS consumption.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d11.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace dsrrl::runtime::pmetal_native_map_cut {
using map_fn=HRESULT (STDMETHODCALLTYPE *)(ID3D11DeviceContext *,
    ID3D11Resource *,UINT,D3D11_MAP,UINT,D3D11_MAPPED_SUBRESOURCE *);
using unmap_fn=void (STDMETHODCALLTYPE *)(ID3D11DeviceContext *,
    ID3D11Resource *,UINT);
constexpr std::size_t map_slot=14u, unmap_slot=15u;

struct witness {
    std::uint64_t map_count=0,unmap_count=0,write_unmap_count=0;
    std::uintptr_t last_map_ctx=0,last_unmap_ctx=0;
    std::uint32_t last_map_type=0,last_map_tid=0,last_unmap_tid=0;
    bool source_context_match=false, complete_map_unmap=false;
    bool ever_watched=false,hook_active=false;
};

class observer {
    struct hook {
        std::atomic<void **> table{nullptr};
        map_fn original_map=nullptr;
        unmap_fn original_unmap=nullptr;
    };
    struct target {
        std::uintptr_t buffer=0,last_map_ctx=0,last_unmap_ctx=0;
        std::uint64_t map_count=0,unmap_count=0,write_unmap_count=0;
        std::uint32_t last_map_type=0,last_map_tid=0,last_unmap_tid=0;
        bool last_map_writable=false;
    };
    std::array<hook,16> hooks_{};
    std::array<target,128> targets_{};
    std::mutex lock_;
    std::atomic<bool> enabled_{false};
    std::atomic<std::uint64_t> hook_rejects_{0};
    std::atomic<std::uint32_t> installed_{0};

    hook *find_hook(ID3D11DeviceContext *context) noexcept {
        if (!context) return nullptr;
        auto **table=*reinterpret_cast<void ***>(context);
        for (auto &h:hooks_)
            if(h.table.load(std::memory_order_acquire)==table)
                return &h;
        return nullptr;
    }
    static bool swap_slot(void **slot,void *expected,void *replacement) noexcept {
        if (!slot || *slot!=expected) return false;
        DWORD old=0;
        if(!VirtualProtect(slot,sizeof(void *),PAGE_READWRITE,&old))
            return false;
        const auto prior=InterlockedCompareExchangePointer(
            reinterpret_cast<PVOID volatile *>(slot),replacement,expected);
        DWORD ignored=0;
        (void)VirtualProtect(slot,sizeof(void *),old,&ignored);
        return prior==expected && *slot==replacement;
    }
    static bool write_access(D3D11_MAP kind) noexcept {
        return kind==D3D11_MAP_WRITE || kind==D3D11_MAP_READ_WRITE ||
               kind==D3D11_MAP_WRITE_DISCARD || kind==D3D11_MAP_WRITE_NO_OVERWRITE;
    }
    void map_event(std::uintptr_t ctx,std::uintptr_t res,D3D11_MAP kind) noexcept {
        std::lock_guard<std::mutex> guard(lock_);
        for(auto &t:targets_) if(t.buffer==res && res) {
            ++t.map_count;
            t.last_map_ctx=ctx;
            t.last_map_type=static_cast<std::uint32_t>(kind);
            t.last_map_tid=GetCurrentThreadId();
            t.last_map_writable=write_access(kind);
            return;
        }
    }
    void unmap_event(std::uintptr_t ctx,std::uintptr_t res) noexcept {
        std::lock_guard<std::mutex> guard(lock_);
        for(auto &t:targets_) if(t.buffer==res && res) {
            ++t.unmap_count;
            t.last_unmap_ctx=ctx;
            t.last_unmap_tid=GetCurrentThreadId();
            if (t.last_map_ctx==ctx && t.last_map_writable)
                ++t.write_unmap_count;
            return;
        }
    }
    static HRESULT STDMETHODCALLTYPE mapped(ID3D11DeviceContext *ctx,
        ID3D11Resource *res,UINT sub,D3D11_MAP type,UINT flags,
        D3D11_MAPPED_SUBRESOURCE *out) noexcept {
        auto &self=global();
        auto *h=self.find_hook(ctx);
        if(!h || !h->original_map) return E_FAIL; // impossible under retained vtable
        const HRESULT hr=h->original_map(ctx,res,sub,type,flags,out);
        if(SUCCEEDED(hr) && sub==0 && res)
            self.map_event(reinterpret_cast<std::uintptr_t>(ctx),
                reinterpret_cast<std::uintptr_t>(res),type);
        return hr;
    }
    static void STDMETHODCALLTYPE unmapped(ID3D11DeviceContext *ctx,
        ID3D11Resource *res,UINT sub) noexcept {
        auto &self=global();
        auto *h=self.find_hook(ctx);
        if(sub==0 && res)
            self.unmap_event(reinterpret_cast<std::uintptr_t>(ctx),
                reinterpret_cast<std::uintptr_t>(res));
        if(h && h->original_unmap)
            h->original_unmap(ctx,res,sub);
    }
public:
    static observer &global() noexcept {
        static observer value;
        return value;
    }
    bool install(ID3D11DeviceContext *ctx) noexcept {
        if (!ctx) return false;
        auto **table=*reinterpret_cast<void ***>(ctx);
        if (!table) return false;
        std::lock_guard<std::mutex> guard(lock_);
        if(find_hook(ctx)) return true;
        hook *empty=nullptr;
        for(auto &h:hooks_) if(!h.table.load(std::memory_order_acquire)) {
            empty=&h; break;
        }
        if (!empty) { ++hook_rejects_;return false;}
        auto m=reinterpret_cast<map_fn>(table[map_slot]);
        auto u=reinterpret_cast<unmap_fn>(table[unmap_slot]);
        if(!m || !u || m==&mapped || u==&unmapped) {
            ++hook_rejects_;return false;
        }
        empty->original_map=m;
        empty->original_unmap=u;
        empty->table.store(table,std::memory_order_release);
        if(!swap_slot(&table[map_slot],reinterpret_cast<void *>(m),
                     reinterpret_cast<void *>(&mapped))) {
            empty->table.store(nullptr,std::memory_order_release);
            ++hook_rejects_;
            return false;
        }
        if(!swap_slot(&table[unmap_slot],reinterpret_cast<void *>(u),
                      reinterpret_cast<void *>(&unmapped))) {
            const bool restored=swap_slot(&table[map_slot],
                reinterpret_cast<void *>(&mapped),reinterpret_cast<void *>(m));
            if(restored)
                empty->table.store(nullptr,std::memory_order_release);
            // If restoration failed retain dispatch table/original in memory.
            ++hook_rejects_;
            return false;
        }
        enabled_.store(true,std::memory_order_release);
        ++installed_;
        return true;
    }
    witness observe(std::uintptr_t ctx,std::uintptr_t buf) noexcept {
        witness w{};
        if(!ctx || !buf) return w;
        std::lock_guard<std::mutex> guard(lock_);
        for(auto &t:targets_) {
            if(t.buffer==buf) {
                w.ever_watched=true;
                w.map_count=t.map_count;
                w.unmap_count=t.unmap_count;
                w.write_unmap_count=t.write_unmap_count;
                w.last_map_ctx=t.last_map_ctx;
                w.last_unmap_ctx=t.last_unmap_ctx;
                w.last_map_type=t.last_map_type;
                w.last_map_tid=t.last_map_tid;
                w.last_unmap_tid=t.last_unmap_tid;
                w.source_context_match=t.last_unmap_ctx==ctx && t.last_map_ctx==ctx;
                w.complete_map_unmap=t.last_map_ctx!=0 &&
                                      t.last_map_ctx==t.last_unmap_ctx &&
                                      t.write_unmap_count!=0;
                w.hook_active=enabled_.load(std::memory_order_acquire);
                return w;
            }
        }
        for(auto &t:targets_) if(!t.buffer) {
            t.buffer=buf;
            w.ever_watched=true;
            w.hook_active=enabled_.load(std::memory_order_acquire);
            return w;
        }
        return {}; // bounded: fail-open, no fabricated association
    }
    void forget(std::uintptr_t res) noexcept {
        if(!res) return;
        std::lock_guard<std::mutex> guard(lock_);
        for(auto &t:targets_) if(t.buffer==res) t={};
    }
    void clear_targets() noexcept {
        std::lock_guard<std::mutex> guard(lock_);
        for(auto &t:targets_) t={};
    }
    bool detach() noexcept {
        enabled_.store(false,std::memory_order_release);
        std::lock_guard<std::mutex> guard(lock_);
        bool good=true;
        for(auto &h:hooks_) {
            auto **table=h.table.load(std::memory_order_acquire);
            if(!table) continue;
            const auto expected_u=reinterpret_cast<void *>(&unmapped);
            const auto expected_m=reinterpret_cast<void *>(&mapped);
            const auto orig_u=reinterpret_cast<void *>(h.original_unmap);
            const auto orig_m=reinterpret_cast<void *>(h.original_map);
            if(table[unmap_slot]==expected_u)
                good &= swap_slot(&table[unmap_slot],expected_u,orig_u);
            else good &= table[unmap_slot]==orig_u;
            if(table[map_slot]==expected_m)
                good &= swap_slot(&table[map_slot],expected_m,orig_m);
            else good &= table[map_slot]==orig_m;
            if(table[unmap_slot]==orig_u && table[map_slot]==orig_m)
                h.table.store(nullptr,std::memory_order_release);
        }
        for(auto &t:targets_) t={};
        return good;
    }
    std::uint32_t hook_count() const noexcept {return installed_.load();}
    std::uint64_t rejects() const noexcept {return hook_rejects_.load();}
};
inline observer &global_observer() noexcept {return observer::global();}
} // namespace dsrrl::runtime::pmetal_native_map_cut
