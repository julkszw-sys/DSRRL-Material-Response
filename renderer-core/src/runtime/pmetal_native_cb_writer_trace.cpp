#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "dsrrl/runtime/pmetal_native_cb_writer_trace.hpp"

#include <Windows.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace dsrrl::runtime {
namespace {

struct watched_cb {
    std::uintptr_t buffer = 0u;
    std::uint32_t width = 0u;
    pmetal_native_cb_writer_stamp last{};
};
struct mapped_cb {
    std::uintptr_t buffer = 0u;
    const std::uint8_t *bytes = nullptr;
    std::uint64_t offset = 0u;
    std::uint64_t size = 0u;
};
std::mutex g_mutex{};
std::array<watched_cb,4096u> g_watches{};
std::atomic<std::uint64_t> g_next_epoch{0u};
thread_local std::array<mapped_cb,16u> g_mapped{};

std::size_t index_for(std::uintptr_t p) noexcept {
    const std::uint64_t v = static_cast<std::uint64_t>(p);
    return static_cast<std::size_t>(
        ((v >> 4u) ^ (v >> 21u) ^ (v >> 37u)) &
        (g_watches.size()-1u));
}
watched_cb *find_locked(std::uintptr_t p,bool insert) noexcept {
    const auto index=index_for(p);
    for (std::size_t i=0;i<16u;++i) {
        auto &slot=g_watches[(index+i)&(g_watches.size()-1u)];
        if(slot.buffer==p)
            return &slot;
        if(slot.buffer==0u) {
            if(!insert)
                return nullptr;
            slot.buffer=p;
            return &slot;
        }
    }
    return nullptr; // collision: fail open; never guess another CB
}
std::uint64_t hash_bytes(
    const std::uint8_t *data,std::size_t size) noexcept {
    std::uint64_t h=14695981039346656037ULL;
    for(std::size_t i=0u;i<size;++i) {
        h^=data[i];
        h*=1099511628211ULL;
    }
    return h;
}
void publish_write(
    std::uintptr_t key,const void *data,
    std::uint64_t offset,std::uint64_t size,
    std::uint8_t method) noexcept {
    if(key==0u || data==nullptr || offset!=0u)
        return;
    std::uint32_t width=0u;
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        const auto *slot=find_locked(key,false);
        if(slot==nullptr || slot->width==0u)
            return;
        width=slot->width;
    }
    // Only native exact b0=2064 or b1=48. Partial writes are recorded as
    // partial, but never misrepresented as an exact full-buffer snapshot.
    const std::uint32_t count=static_cast<std::uint32_t>(
        size>=width?width:size);
    if(count==0u)
        return;
    const auto hash=hash_bytes(
        static_cast<const std::uint8_t *>(data),count);
    pmetal_native_cb_writer_stamp next{};
    next.epoch=g_next_epoch.fetch_add(1u,std::memory_order_relaxed)+1u;
    next.hash=hash;
    next.timestamp_ms=GetTickCount64();
    next.writer_tid=GetCurrentThreadId();
    next.byte_count=count;
    next.method=method;
    next.watched=true;
    next.complete=count==width;
#if defined(DSRRL_PMETAL_ASYLUM_CB_REGISTER_TRACE)
    // Hash only fully captured CPU-side float4-sized registers.
    // The b0/b1 ABI and per-register semantic names remain UNVERIFIED.
    if (next.complete && (width % 16u) == 0u) {
        next.register_count=static_cast<std::uint16_t>(width/16u);
        const auto *bytes=static_cast<const std::uint8_t *>(data);
        for (std::uint32_t i=0u;i<next.register_count;++i)
            next.register_hashes[i]=hash_bytes(bytes+i*16u,16u);
    }
#endif
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        auto *slot=find_locked(key,false);
        if(slot!=nullptr && slot->width==width)
            slot->last=next;
    }
}
} // namespace

void pmetal_native_cb_writer_watch(
    std::uintptr_t p,std::uint32_t width) noexcept {
    if(p==0u || (width!=2064u && width!=48u))
        return;
    std::lock_guard<std::mutex> guard(g_mutex);
    auto *slot=find_locked(p,true);
    if(slot==nullptr)
        return;
    if(slot->width!=width) {
        slot->width=width;
        slot->last={};
        slot->last.watched=true;
    }
}
pmetal_native_cb_writer_stamp pmetal_native_cb_writer_lookup(
    std::uintptr_t p) noexcept {
    std::lock_guard<std::mutex> guard(g_mutex);
    const auto *slot=find_locked(p,false);
    return slot==nullptr?pmetal_native_cb_writer_stamp{}:slot->last;
}
void pmetal_native_cb_writer_after_map(
    std::uintptr_t p,void *ptr,
    std::uint64_t offset,std::uint64_t size,
    bool writable) noexcept {
    if(!writable || p==0u || ptr==nullptr)
        return;
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        if(find_locked(p,false)==nullptr)
            return;
    }
    for(auto &item:g_mapped) {
        if(item.buffer==p || item.buffer==0u) {
            item={p,static_cast<const std::uint8_t *>(ptr),offset,size};
            return;
        }
    }
}
void pmetal_native_cb_writer_before_unmap(
    std::uintptr_t p) noexcept {
    for(auto &item:g_mapped) {
        if(item.buffer!=p || p==0u)
            continue;
        // Map data remains valid during the ReShade before-Unmap event.
        // No GPU readback or additional D3D call occurs here.
        publish_write(p,item.bytes,item.offset,item.size,1u);
        item={};
        return;
    }
}
void pmetal_native_cb_writer_before_update(
    std::uintptr_t p,const void *src,
    std::uint64_t offset,std::uint64_t size) noexcept {
    publish_write(p,src,offset,size,2u);
}
void pmetal_native_cb_writer_destroy(
    std::uintptr_t p) noexcept {
    if(p==0u)return;
    std::lock_guard<std::mutex> guard(g_mutex);
    auto *slot=find_locked(p,false);
    if(slot!=nullptr) {
        // Keep the probe-chain slot occupied, but revoke its payload.
        // A future COM object reusing the address will be watched anew.
        slot->width=0u;
        slot->last={};
    }
}
void pmetal_native_cb_writer_reset() noexcept {
    std::lock_guard<std::mutex> guard(g_mutex);
    g_watches={};
    g_next_epoch.store(0u,std::memory_order_relaxed);
    g_mapped={};
}
} // namespace dsrrl::runtime
