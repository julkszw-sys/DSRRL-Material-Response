#pragma once
// V350 RC3 diagnostics: bounded counters + 1/64 sampled CPU QPC.
// No disk writes, no native state mutations, no per-draw log messages.
// This module is OBSERVATION ONLY: counters never participate in shader,
// receiver, material, resource or fail-open authorization decisions.
#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>

namespace dsrrl::runtime::rc3_telemetry {

enum class reason : unsigned {
    draw_identity_unresolved,
    pmetal_prepare_rejected,
    pmetal_shader_batch_rejected,
    pmetal_resource_batch_rejected,
    lerp_material_not_prepared,
    lerp_shader_batch_rejected,
    lerp_resource_batch_rejected,
    lerp_spec_anti_hybrid_guard,
    stable_material_not_prepared,
    stable_shader_batch_rejected,
    stable_resource_batch_rejected,
    draw_transaction_not_issued,
    draw_transaction_restore_failed,
    count
};
enum class stage : unsigned {
    create_pipeline,
    bind_pipeline,
    draw_stable,
    draw_hemenvlerp,
    draw_a1_or_other,
    dds_load,
    early_gpu_preload,
    count
};
inline constexpr const char *reason_names[] = {
    "identity_or_receiver_unresolved",
    "pmetal_prepare_reject",
    "pmetal_shader_batch_reject",
    "pmetal_resource_batch_reject",
    "lerp_mr_carrier_not_ready",
    "lerp_shader_batch_reject",
    "lerp_resource_batch_reject",
    "lerp_spec_antihybrid_guard_partial_only",
    "stable_mr_carrier_not_ready",
    "stable_shader_batch_reject",
    "stable_resource_batch_reject",
    "draw_tx_not_issued",
    "draw_tx_restore_failed"
};
inline constexpr const char *stage_names[] = {
    "create_pipeline_cpu", "bind_pipeline_cpu", "draw_stable_postroute_cpu",
    "draw_hemenvlerp_postroute_cpu", "draw_a1_other_postroute_cpu",
    "dds_load_cpu", "prewarm_cpu"
};
inline constexpr unsigned reasons = static_cast<unsigned>(reason::count);
inline constexpr unsigned stages = static_cast<unsigned>(stage::count);
static_assert(sizeof(reason_names)/sizeof(reason_names[0]) == reasons);
static_assert(sizeof(stage_names)/sizeof(stage_names[0]) == stages);

#if defined(DSRRL_V350_RC3_RUNTIME_TELEMETRY)
struct fail_count {
    std::atomic<std::uint64_t> hits{0};
    std::atomic<std::uint64_t> first_frame{0};
    std::atomic<std::uint64_t> last_frame{0};
    std::atomic<std::uint32_t> last_receiver{0};
    std::atomic<std::uint32_t> last_route{0};
};
struct cpu_count {
    std::atomic<std::uint64_t> samples{0};
    std::atomic<std::uint64_t> ticks{0};
    std::atomic<std::uint64_t> max_ticks{0};
};
inline std::array<fail_count,reasons> failures{};
inline std::array<cpu_count,stages> cpu{};
inline std::atomic<std::uint64_t> frames{0};
inline std::atomic<std::uint64_t> frequency{0};
inline std::atomic<std::uint64_t> last_present_ticks{0};
inline std::atomic<std::uint64_t> present_gaps{0};
inline std::atomic<std::uint64_t> present_gap_ticks{0};
inline std::atomic<std::uint64_t> max_present_gap_ticks{0};
inline std::atomic<std::uint64_t> frame_start{1};

inline void maximize(std::atomic<std::uint64_t> &target,std::uint64_t value) noexcept {
    auto seen=target.load(std::memory_order_relaxed);
    while(seen<value && !target.compare_exchange_weak(
        seen,value,std::memory_order_relaxed,std::memory_order_relaxed)) {}
}
inline void initialize() noexcept {
    LARGE_INTEGER f{};
    if (QueryPerformanceFrequency(&f) && f.QuadPart>0)
        frequency.store(static_cast<std::uint64_t>(f.QuadPart),
                        std::memory_order_release);
}
inline void mark(reason r,std::uint32_t receiver=0,std::uint32_t route=0) noexcept {
    auto &f=failures[static_cast<unsigned>(r)];
    auto frame=frames.load(std::memory_order_relaxed);
    if(frame==0) frame=1;
    auto zero=std::uint64_t{0};
    (void)f.first_frame.compare_exchange_strong(
        zero,frame,std::memory_order_relaxed);
    f.last_frame.store(frame,std::memory_order_relaxed);
    f.last_receiver.store(receiver,std::memory_order_relaxed);
    f.last_route.store(route,std::memory_order_relaxed);
    f.hits.fetch_add(1,std::memory_order_relaxed);
}
struct sampled_scope {
    stage name;
    LARGE_INTEGER start{};
    bool active=false;
    explicit sampled_scope(stage s) noexcept : name(s) {
        if(frequency.load(std::memory_order_relaxed)==0) return;
        // Independent sampler per site/thread: sampling does not alter draw state.
        thread_local std::array<unsigned,stages> serial{};
        const auto i=static_cast<unsigned>(s);
        if((++serial[i] & 63u)!=1u) return;
        active=QueryPerformanceCounter(&start)!=0;
    }
    ~sampled_scope() noexcept {
        if(!active) return;
        LARGE_INTEGER finish{};
        if(!QueryPerformanceCounter(&finish) || finish.QuadPart<start.QuadPart) return;
        auto &p=cpu[static_cast<unsigned>(name)];
        const auto delta=static_cast<std::uint64_t>(finish.QuadPart-start.QuadPart);
        p.samples.fetch_add(1,std::memory_order_relaxed);
        p.ticks.fetch_add(delta,std::memory_order_relaxed);
        maximize(p.max_ticks,delta);
    }
    sampled_scope(const sampled_scope&)=delete;
    sampled_scope& operator=(const sampled_scope&)=delete;
};
inline void present(std::uint64_t frame) noexcept {
    frames.store(frame,std::memory_order_relaxed);
    if(frequency.load(std::memory_order_relaxed)==0) return;
    LARGE_INTEGER now{};
    if(!QueryPerformanceCounter(&now)) return;
    const auto t=static_cast<std::uint64_t>(now.QuadPart);
    const auto prev=last_present_ticks.exchange(t,std::memory_order_relaxed);
    if(!prev || t<=prev) return;
    const auto delta=t-prev;
    present_gaps.fetch_add(1,std::memory_order_relaxed);
    present_gap_ticks.fetch_add(delta,std::memory_order_relaxed);
    maximize(max_present_gap_ticks,delta);
}
template<typename Emit>
inline void report(std::uint64_t frame,Emit emit) noexcept {
    const auto hz=frequency.load(std::memory_order_relaxed);
    const auto first=frame_start.exchange(frame+1,std::memory_order_relaxed);
    char line[640]{};
    std::snprintf(line,sizeof(line),
       "[DSRRL RC3 TELEMETRY] window=%llu-%llu pointlight_bridge=OFF "
       "stock_dsr_pointlight=ON cpu_sample=1/64 qpc_hz=%llu pixel=UNVERIFIED",
       static_cast<unsigned long long>(first),
       static_cast<unsigned long long>(frame),
       static_cast<unsigned long long>(hz));
    emit(line);
    for(unsigned i=0;i<reasons;++i) {
        auto &f=failures[i];
        const auto hits=f.hits.exchange(0,std::memory_order_relaxed);
        if(hits==0) continue;
        const auto first_at=f.first_frame.exchange(0,std::memory_order_relaxed);
        const auto last_at=f.last_frame.exchange(0,std::memory_order_relaxed);
        const auto rx=f.last_receiver.load(std::memory_order_relaxed);
        const auto route=f.last_route.load(std::memory_order_relaxed);
        std::snprintf(line,sizeof(line),
            "[DSRRL RC3 FAILOPEN] reason=%s count=%llu first_frame=%llu last_frame=%llu "
            "last_rx=%u last_route=%u classification=%s",
            reason_names[i],static_cast<unsigned long long>(hits),
            static_cast<unsigned long long>(first_at),
            static_cast<unsigned long long>(last_at),rx,route,
            i==static_cast<unsigned>(reason::lerp_spec_anti_hybrid_guard)
              ?"partial_guard_not_full_stock_fallback":"operator_local_fallback");
        emit(line);
    }
    for(unsigned i=0;i<stages;++i) {
        auto &p=cpu[i];
        const auto n=p.samples.exchange(0,std::memory_order_relaxed);
        const auto ticks=p.ticks.exchange(0,std::memory_order_relaxed);
        const auto maximum=p.max_ticks.exchange(0,std::memory_order_relaxed);
        if(n==0 || hz==0) continue;
        const double avg=1.0e6*static_cast<double>(ticks)/
             (static_cast<double>(hz)*static_cast<double>(n));
        const double peak=1.0e6*static_cast<double>(maximum)/static_cast<double>(hz);
        std::snprintf(line,sizeof(line),
           "[DSRRL RC3 PERF] stage=%s samples=%llu mean_cpu_us=%.2f max_sampled_cpu_us=%.2f "
           "gpu_time=NOT_MEASURED",
           stage_names[i],static_cast<unsigned long long>(n),avg,peak);
        emit(line);
    }
    const auto n=present_gaps.exchange(0,std::memory_order_relaxed);
    const auto ticks=present_gap_ticks.exchange(0,std::memory_order_relaxed);
    const auto maximum=max_present_gap_ticks.exchange(0,std::memory_order_relaxed);
    if(n && hz) {
        std::snprintf(line,sizeof(line),
          "[DSRRL RC3 PERF] stage=present_spacing samples=%llu mean_ms=%.3f max_ms=%.3f "
          "includes_vsync_wait=YES gpu_time=NOT_MEASURED",
          static_cast<unsigned long long>(n),
          1.0e3*static_cast<double>(ticks)/(static_cast<double>(hz)*static_cast<double>(n)),
          1.0e3*static_cast<double>(maximum)/static_cast<double>(hz));
        emit(line);
    }
}
#else
inline void initialize() noexcept {}
inline void mark(reason,std::uint32_t=0,std::uint32_t=0) noexcept {}
struct sampled_scope {
    explicit sampled_scope(stage) noexcept {}
};
inline void present(std::uint64_t) noexcept {}
template<typename Emit> inline void report(std::uint64_t,Emit) noexcept {}
#endif
} // namespace dsrrl::runtime::rc3_telemetry
