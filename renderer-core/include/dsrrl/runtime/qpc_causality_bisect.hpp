#pragma once
// V350 same-binary QPC/atomic causality test. DIAGNOSTIC ONLY.
// A single byte '0'..'3' in DSRRL_QPC_MODE.txt next to DarkSoulsRemastered.exe,
// read once by AddonInit before any renderer/engine hooks are installed.
// 0: instrumentation bypass; 1: QPC reads + TLS sink ONLY;
// 2: globally shared locked atomics WITHOUT QPC;
// 3: QPC reads plus the same shared counters (legacy-style).
// Unlike historical DSRRL_STUTTER_PROFILE, only time perturbation differs;
// no per-draw disk I/O, D3D state changes, logging, sleeps, affinity or priority.
#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <cstring>
#include <cstddef>

namespace dsrrl::runtime::qpc_bisect {

enum class stage : std::size_t {
    flver_parse, flver_digest, flver_registry, mtd_observe,
    selector, pmetal_selector, resource_view_init, dds_load,
    create_texture, present, count
};
constexpr std::size_t stage_count=static_cast<std::size_t>(stage::count);
struct counter {
    std::atomic<std::uint64_t> calls{0u};
    std::atomic<std::uint64_t> ticks{0u};
    std::atomic<std::uint64_t> max_ticks{0u};
};
inline std::array<counter,stage_count> g_counters{};
inline std::atomic<std::uint32_t> g_mode{0u};
inline thread_local volatile std::uint64_t g_tls_qpc_sink=0u;

inline void init_from_game_directory() noexcept {
    // Deterministic load-once switch: no mode change mid-scene, data races,
    // or cross-mode ring buffers; every A/B uses the SAME DLL and instruction layout.
    wchar_t path[MAX_PATH]{};
    const DWORD len=GetModuleFileNameW(nullptr,path,MAX_PATH);
    char token='0';
    if(len!=0u && len<MAX_PATH) {
        wchar_t *last=std::wcsrchr(path,L'\\');
        if(last!=nullptr) {
            *(last+1)=L'\0';
            constexpr wchar_t tail[]=L"DSRRL_QPC_MODE.txt";
            const std::size_t used=static_cast<std::size_t>(last+1-path);
            if(used+std::size(tail)<MAX_PATH) {
                std::memcpy(last+1,tail,sizeof(tail));
                HANDLE file=CreateFileW(path,GENERIC_READ,
                    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                    nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
                if(file!=INVALID_HANDLE_VALUE) {
                    DWORD n=0u;
                    if(!ReadFile(file,&token,1u,&n,nullptr)||n!=1u)token='0';
                    CloseHandle(file);
                }
            }
        }
    }
    const std::uint32_t mode=token>='0'&&token<='3'
        ? static_cast<std::uint32_t>(token-'0') : 0u;
    g_mode.store(mode,std::memory_order_release);
    const char *description[]={
        "OFF","QPC_READS_TLS_ONLY","ATOMIC_RMW_ONLY","QPC_PLUS_ATOMIC_RMW"};
    char msg[256]{};
    std::snprintf(msg,sizeof(msg),
        "[DSRRL QPC CAUSALITY] mode=%u %s same_binary=YES instrumentation_only=YES",
        static_cast<unsigned>(mode),description[mode]);
    OutputDebugStringA(msg);
}

inline void update_max(std::atomic<std::uint64_t> &maximum,
                       std::uint64_t ticks) noexcept {
    auto value=maximum.load(std::memory_order_relaxed);
    while(value<ticks && !maximum.compare_exchange_weak(
        value,ticks,std::memory_order_relaxed,
        std::memory_order_relaxed)) {}
}

class scope {
public:
    explicit scope(stage s) noexcept
        : index_(static_cast<std::size_t>(s)),
          mode_(g_mode.load(std::memory_order_relaxed)) {
        if((mode_&1u)!=0u) {
            LARGE_INTEGER ticks{};
            (void)QueryPerformanceCounter(&ticks);
            begin_=static_cast<std::uint64_t>(ticks.QuadPart);
        }
    }
    ~scope() noexcept {
        if(mode_==0u)return;
        std::uint64_t delta=1u;
        if((mode_&1u)!=0u) {
            LARGE_INTEGER ticks{};
            (void)QueryPerformanceCounter(&ticks);
            const auto end=static_cast<std::uint64_t>(ticks.QuadPart);
            delta=end>=begin_?end-begin_:0u;
            // Thread-local volatile sink isolates timestamp reads from all
            // cross-thread atomic operations in mode 1.
            g_tls_qpc_sink = g_tls_qpc_sink ^ (delta+index_);
        }
        if((mode_&2u)!=0u) {
            auto &c=g_counters[index_];
            c.calls.fetch_add(1u,std::memory_order_relaxed);
            c.ticks.fetch_add(delta,std::memory_order_relaxed);
            update_max(c.max_ticks,delta);
        }
    }
    scope(const scope &)=delete;
    scope &operator=(const scope &)=delete;
private:
    std::size_t index_;
    std::uint32_t mode_;
    std::uint64_t begin_=0u;
};

inline void report() noexcept {
    const auto mode=g_mode.load(std::memory_order_relaxed);
    std::uint64_t calls=0u;
    for(const auto &s:g_counters)
        calls+=s.calls.load(std::memory_order_relaxed);
    char msg[256]{};
    std::snprintf(msg,sizeof(msg),
        "[DSRRL QPC CAUSALITY] unload_mode=%u shared_observed_scopes=%llu qpc_only_tls_not_counted=YES",
        static_cast<unsigned>(mode),
        static_cast<unsigned long long>(calls));
    OutputDebugStringA(msg);
}
} // namespace dsrrl::runtime::qpc_bisect
