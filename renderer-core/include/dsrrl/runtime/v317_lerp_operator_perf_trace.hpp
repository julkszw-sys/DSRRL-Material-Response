#pragma once
// V317 opt-in instrumentation ONLY. Does not change shaders, CBs, SRVs,
// material authority, any draw state, or engine hooks. All samples are bounded.
// "issued" means dispatch completed, NOT PTDE pixel equivalence.
#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include <reshade.hpp>
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>

namespace dsrrl::runtime::v317_lerp_diagnostic {
inline std::atomic<std::uint32_t> operator_lines{0};
inline std::array<std::atomic<std::uint64_t>,4096> operator_keys{};
inline std::atomic<std::uint64_t> operator_throttled{0};
inline std::atomic<std::uint64_t> lerp_issue{0}, lerp_fallback{0};
inline std::atomic<std::uint64_t> stable_issue{0}, stable_fallback{0};
inline std::atomic<std::uint64_t> lerp_t10_request{0}, lerp_t10_absent{0};
inline std::atomic<std::uint64_t> lerp_probe_pair_ready{0}, lerp_probe_pair_bad{0};

inline std::uint64_t mix(std::uint64_t h, std::uint8_t b) noexcept {
    return (h ^ b) * 1099511628211ull;
}
inline void hash_word(std::uint64_t &h, std::uint32_t x) noexcept {
    for (unsigned i=0;i<4u;++i)
        h=mix(h,static_cast<std::uint8_t>(x>>(i*8u)));
}

// Sampling first 16 per thread then 1/16 of subsequent issued draws
// bounds the hashing cost separately from the bounded 256 log lines.
inline bool should_sample() noexcept {
    static thread_local std::uint32_t seq=0u;
    ++seq;
    return seq<=16u || (seq & 15u)==0u;
}
inline void operator_event(
    const char *stage,
    bool lerp,
    const operators::material_response::material_identity &m,
    const operators::material_response::decision &d,
    const prepared_pmetal_envspec_draw &p,
    bool issued) noexcept
{
    if (!should_sample()) return;
    if (lerp) {
        if (issued) lerp_issue.fetch_add(1u,std::memory_order_relaxed);
        else lerp_fallback.fetch_add(1u,std::memory_order_relaxed);
        if (p.material_resources.spec_rgb)
            lerp_t10_request.fetch_add(1u,std::memory_order_relaxed);
        else lerp_t10_absent.fetch_add(1u,std::memory_order_relaxed);
    } else {
        if (issued) stable_issue.fetch_add(1u,std::memory_order_relaxed);
        else stable_fallback.fetch_add(1u,std::memory_order_relaxed);
    }
    const bool resources =
        p.env_resources.ptde_a && p.env_resources.ptde_b &&
        p.envdiffuse_resources.ptde_a && p.envdiffuse_resources.ptde_b &&
        p.env_resources.sampler && p.envdiffuse_resources.sampler && p.b12;
    if (lerp) {
        if (resources) lerp_probe_pair_ready.fetch_add(1u,std::memory_order_relaxed);
        else lerp_probe_pair_bad.fetch_add(1u,std::memory_order_relaxed);
    }
    std::uint64_t h=14695981039346656037ull;
    for (auto b:m.flver_sha256) h=mix(h,b);
    for (auto b:m.raw_mtd_sha256) h=mix(h,b);
    hash_word(h,d.receiver_id); hash_word(h,m.route_index);
    hash_word(h,m.material_slot); hash_word(h,lerp?1u:0u);
    hash_word(h,issued?1u:0u);
    hash_word(h,p.env_resources.probe_a);
    hash_word(h,p.env_resources.probe_b);
#if defined(DSRRL_V291_LERP_CUBEMAP_CENSUS)
    if(lerp) {
        const float beta=p.diagnostic_beta;
        const std::uint32_t band=
            beta<=0.0f?0u:beta<0.1f?1u:beta<0.4f?2u:
            beta<0.7f?3u:beta<1.0f?4u:5u;
        hash_word(h,band);
    }
#endif
    if(!h)h=1u;
    bool distinct=false;
    for (unsigned i=0u;i<12u;++i) {
        auto &entry=operator_keys[(h+i*53u)&4095u];
        const auto old=entry.load(std::memory_order_relaxed);
        if(old==h) return;
        if(old==0u) {
            std::uint64_t zero=0u;
            if(entry.compare_exchange_strong(zero,h,std::memory_order_relaxed)) {
                distinct=true;break;
            }
            if(zero==h)return;
        }
    }
    if(!distinct) return;
    const auto log_index=operator_lines.fetch_add(1u,std::memory_order_relaxed);
    if(log_index>=256u) {operator_throttled.fetch_add(1u,std::memory_order_relaxed);return;}
    char line[1100]{};
#if defined(DSRRL_V291_LERP_CUBEMAP_CENSUS)
    const double beta=static_cast<double>(p.diagnostic_beta);
    const unsigned spec_a=p.diagnostic_probe_a, spec_b=p.diagnostic_probe_b;
    const unsigned dif_a=p.diagnostic_diff_a, dif_b=p.diagnostic_diff_b;
#else
    const double beta=-1.0;
    const unsigned spec_a=p.env_resources.probe_a, spec_b=p.env_resources.probe_b;
    const unsigned dif_a=p.envdiffuse_resources.probe_a, dif_b=p.envdiffuse_resources.probe_b;
#endif
    std::snprintf(line,sizeof(line),
      "[DSRRL V317 LERP SPEC_PROBE] stage=%s phase=%s issued=%u rx=%u route=%u slot=%u "
      "spec_t10_request=%u spec_req_count=%u spec_material_ready=%u c101=%.9g c100=%.9g,%.9g,%.9g spec_power=%.9g spec_power_verified=%u env_slot=%u "
      "spec_a=%u spec_b=%u diff_a=%u diff_b=%u beta=%.9g "
      "t12=%p t14=%p t11=%p t13=%p s_spec=%u s_diff=%u b12=%u ps=%p "
      "mtd=%02x%02x%02x%02x%02x%02x%02x%02x flver=%02x%02x%02x%02x%02x%02x%02x%02x "
      "pixel=UNVERIFIED",
      stage,lerp?"LERP":"STABLE",issued?1u:0u,
      d.receiver_id,m.route_index,m.material_slot,
      p.material_resources.spec_rgb?1u:0u,p.material_resources.request_count,
      p.material_resources.retained_count!=0u?1u:0u,
      static_cast<double>(d.c101),static_cast<double>(d.c100[0]),
      static_cast<double>(d.c100[1]),static_cast<double>(d.c100[2]),
      static_cast<double>(d.ptde_specular_power),
      d.ptde_specular_power_verified?1u:0u,
      static_cast<unsigned>(p.env_resources.slot),
      spec_a,spec_b,dif_a,dif_b,beta,
      static_cast<void*>(p.env_resources.ptde_a),
      static_cast<void*>(p.env_resources.ptde_b),
      static_cast<void*>(p.envdiffuse_resources.ptde_a),
      static_cast<void*>(p.envdiffuse_resources.ptde_b),
      p.env_resources.sampler?1u:0u,p.envdiffuse_resources.sampler?1u:0u,
      p.b12?1u:0u,static_cast<void*>(p.shader),
      m.raw_mtd_sha256[0],m.raw_mtd_sha256[1],
      m.raw_mtd_sha256[2],m.raw_mtd_sha256[3],
      m.raw_mtd_sha256[4],m.raw_mtd_sha256[5],
      m.raw_mtd_sha256[6],m.raw_mtd_sha256[7],
      m.flver_sha256[0],m.flver_sha256[1],
      m.flver_sha256[2],m.flver_sha256[3],
      m.flver_sha256[4],m.flver_sha256[5],
      m.flver_sha256[6],m.flver_sha256[7]);
    reshade::log::message(reshade::log::level::info,line);
}
inline std::atomic<std::uint64_t> sampled[2]{};
inline std::atomic<std::uint64_t> identity_ticks[2]{};
inline std::atomic<std::uint64_t> prepare_ticks[2]{};
inline std::atomic<std::uint64_t> dispatch_ticks[2]{};
inline std::atomic<std::uint64_t> callback_ticks[2]{};
inline std::atomic<std::uint64_t> callback_peak[2]{};
inline std::atomic<std::uint64_t> present_ticks{0};
inline std::atomic<std::uint64_t> present_count{0};
inline std::atomic<std::uint64_t> present_peak{0};
inline std::atomic<std::uint64_t> last_present{0};
inline LARGE_INTEGER qpc() noexcept { LARGE_INTEGER v{};QueryPerformanceCounter(&v);return v; }
inline std::uint64_t frequency() noexcept {
    static const std::uint64_t f=[]() noexcept {
        LARGE_INTEGER v{};QueryPerformanceFrequency(&v);
        return v.QuadPart>0?static_cast<std::uint64_t>(v.QuadPart):1ull;
    }();
    return f;
}
inline void peak(std::atomic<std::uint64_t> &p,std::uint64_t v) noexcept {
    auto old=p.load(std::memory_order_relaxed);
    while(v>old && !p.compare_exchange_weak(old,v,std::memory_order_relaxed)) {}
}
struct draw_profile {
    bool active=false; unsigned bucket=0u;
    std::uint64_t start=0u,identity=0u,prepared=0u,dispatch=0u;
    draw_profile(bool candidate,bool lerp) noexcept : bucket(lerp?1u:0u) {
        static thread_local std::uint32_t draw_seq[2]{};
        if(candidate && ((++draw_seq[bucket] & 63u)==0u)) {
            active=true;
            start=static_cast<std::uint64_t>(qpc().QuadPart);
        }
    }
    void identity_done() noexcept {
        if(active)identity=static_cast<std::uint64_t>(qpc().QuadPart);
    }
    void prepare_done() noexcept {
        if(active)prepared=static_cast<std::uint64_t>(qpc().QuadPart);
    }
    void dispatch_done() noexcept {
        if(active)dispatch=static_cast<std::uint64_t>(qpc().QuadPart);
    }
    ~draw_profile() noexcept {
        if(!active)return;
        const auto end=static_cast<std::uint64_t>(qpc().QuadPart);
        sampled[bucket].fetch_add(1u,std::memory_order_relaxed);
        identity_ticks[bucket].fetch_add((identity?identity:end)-start,std::memory_order_relaxed);
        if(identity)prepare_ticks[bucket].fetch_add((prepared?prepared:end)-identity,std::memory_order_relaxed);
        if(prepared)dispatch_ticks[bucket].fetch_add((dispatch?dispatch:end)-prepared,std::memory_order_relaxed);
        callback_ticks[bucket].fetch_add(end-start,std::memory_order_relaxed);
        peak(callback_peak[bucket],end-start);
    }
};
inline void on_present(std::uint64_t frame) noexcept {
    const auto tick=static_cast<std::uint64_t>(qpc().QuadPart);
    const auto prev=last_present.exchange(tick,std::memory_order_relaxed);
    if(prev && tick>prev) {
        const auto dt=tick-prev;
        present_ticks.fetch_add(dt,std::memory_order_relaxed);
        present_count.fetch_add(1u,std::memory_order_relaxed);
        peak(present_peak,dt);
    }
    if(frame%240u!=0u)return;
    const double micros_per_tick=1.0e6/static_cast<double>(frequency());
    const auto pc=present_count.exchange(0u,std::memory_order_relaxed);
    const auto pt=present_ticks.exchange(0u,std::memory_order_relaxed);
    const auto px=present_peak.exchange(0u,std::memory_order_relaxed);
    char line[520]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V317 PERF] frames=%llu present_avg_ms=%.4f present_max_ms=%.4f "
        "present_type=CPU_PRESENT_INTERVAL_NOT_GPU_TIME sample_stride_draw=64 "
        "spec_issued_sample=%llu spec_no_t10_sample=%llu pair_ready_sample=%llu pair_bad_sample=%llu "
        "unique_operator_lines=%u log_overflow=%llu",
        static_cast<unsigned long long>(pc),
        pc?static_cast<double>(pt)*micros_per_tick/(1000.0*pc):0.0,
        static_cast<double>(px)*micros_per_tick/1000.0,
        static_cast<unsigned long long>(lerp_issue.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(lerp_t10_absent.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(lerp_probe_pair_ready.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(lerp_probe_pair_bad.load(std::memory_order_relaxed)),
        operator_lines.load(std::memory_order_relaxed),
        static_cast<unsigned long long>(operator_throttled.load(std::memory_order_relaxed)));
    reshade::log::message(reshade::log::level::info,line);
    for(unsigned b=0;b<2u;++b) {
        const auto n=sampled[b].exchange(0u,std::memory_order_relaxed);
        const auto id=identity_ticks[b].exchange(0u,std::memory_order_relaxed);
        const auto pr=prepare_ticks[b].exchange(0u,std::memory_order_relaxed);
        const auto di=dispatch_ticks[b].exchange(0u,std::memory_order_relaxed);
        const auto total=callback_ticks[b].exchange(0u,std::memory_order_relaxed);
        const auto mx=callback_peak[b].exchange(0u,std::memory_order_relaxed);
        char stage[460]{};
        std::snprintf(stage,sizeof(stage),
            "[DSRRL V317 PERF_STAGE] phase=%s samples=%llu identity_avg_us=%.4f "
            "prepare_avg_us=%.4f dispatch_restore_avg_us=%.4f "
            "callback_avg_us=%.4f callback_max_us=%.4f QPC_SAMPLED=1 "
            "excludes_gpu_time_and_external_hooks=1",
            b?"LERP":"STABLE",static_cast<unsigned long long>(n),
            n?static_cast<double>(id)*micros_per_tick/n:0.0,
            n?static_cast<double>(pr)*micros_per_tick/n:0.0,
            n?static_cast<double>(di)*micros_per_tick/n:0.0,
            n?static_cast<double>(total)*micros_per_tick/n:0.0,
            static_cast<double>(mx)*micros_per_tick);
        reshade::log::message(reshade::log::level::info,stage);
    }
}
} // namespace dsrrl::runtime::v317_lerp_diagnostic
