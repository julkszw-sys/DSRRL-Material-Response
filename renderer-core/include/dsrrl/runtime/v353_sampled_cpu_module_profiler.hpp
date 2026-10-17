#pragma once
// V353 CPU module profiler: 1/128 deterministic calls per stage/thread.
// QPC only on sampled entry/exit, bounded atomics per completed sample.
// Inclusive timings can overlap between stages: never sum them as exclusive CPU.
// No GPU timing/readback; keep state and renderer source untouched.
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>

namespace dsrrl::runtime::v353 {

enum class stage : std::uint8_t {
    draw_regular,
    draw_indexed,
    receiver_material_batch,
    flver_owner_route,
    material_decision,
    material_cb_shader_prepare,
    lightbank_source,
    envspec_full_prepare,
    cubemap_resource,
    material_texture_sidecars,
    draw_transaction,
    count
};
inline constexpr std::size_t k_stages = static_cast<std::size_t>(stage::count);
inline constexpr std::uint32_t k_stride = 128u;
inline constexpr std::array<const char *,k_stages> k_stage_names {{
    "draw_regular_inclusive",
    "draw_indexed_inclusive",
    "receiver_material_batch_inclusive",
    "flver_owner_route",
    "material_decision",
    "material_cb_shader_prepare",
    "lightbank_source",
    "envspec_full_prepare",
    "cubemap_resource",
    "material_texture_sidecars",
    "draw_transaction_cpu_submit"
}};
struct samples {
    std::atomic<std::uint64_t> n{0};
    std::atomic<std::uint64_t> sum_ticks{0};
    std::atomic<std::uint64_t> max_ticks{0};
    std::atomic<std::uint64_t> skipped_clock{0};
};
inline std::array<samples,k_stages> g_samples{};
inline thread_local std::array<std::uint32_t,k_stages> g_thread_sequence{};

inline std::uint64_t qpc_frequency() noexcept {
    static const std::uint64_t frequency=[]() noexcept {
        LARGE_INTEGER f{};
        return QueryPerformanceFrequency(&f) && f.QuadPart > 0
            ? static_cast<std::uint64_t>(f.QuadPart) : 0u;
    }();
    return frequency;
}
class scoped_sample {
public:
    explicit scoped_sample(stage p) noexcept :
        stage_(static_cast<std::size_t>(p)) {
        // No atomic RMW and NO QPC on 127 of 128 calls.
        const auto n = ++g_thread_sequence[stage_];
        if ((n & (k_stride - 1u)) != 0u || qpc_frequency()==0u)
            return;
        LARGE_INTEGER v{};
        if (!QueryPerformanceCounter(&v)) {
            g_samples[stage_].skipped_clock.fetch_add(1,std::memory_order_relaxed);
            return;
        }
        start_ = static_cast<std::uint64_t>(v.QuadPart);
        active_=true;
    }
    scoped_sample(const scoped_sample &) = delete;
    scoped_sample &operator=(const scoped_sample &) = delete;
    ~scoped_sample() noexcept {
        if (!active_) return;
        LARGE_INTEGER v{};
        if (!QueryPerformanceCounter(&v) ||
            static_cast<std::uint64_t>(v.QuadPart)<start_) {
            g_samples[stage_].skipped_clock.fetch_add(1,std::memory_order_relaxed);
            return;
        }
        const auto ticks=static_cast<std::uint64_t>(v.QuadPart)-start_;
        auto &a=g_samples[stage_];
        a.sum_ticks.fetch_add(ticks,std::memory_order_relaxed);
        a.n.fetch_add(1,std::memory_order_relaxed);
        auto observed=a.max_ticks.load(std::memory_order_relaxed);
        while (ticks>observed &&
               !a.max_ticks.compare_exchange_weak(
                    observed,ticks,std::memory_order_relaxed))
            {}
    }
private:
    std::size_t stage_;
    std::uint64_t start_=0;
    bool active_=false;
};

template <typename Sink>
inline void report_window(double window_s, std::uint64_t window_index, Sink &&emit) noexcept {
    const auto frequency=qpc_frequency();
    if (frequency==0u || window_s<=0.) return;
    for (std::size_t i=0;i<k_stages;++i) {
        auto &a=g_samples[i];
        const auto n=a.n.exchange(0,std::memory_order_relaxed);
        const auto ticks=a.sum_ticks.exchange(0,std::memory_order_relaxed);
        const auto max=a.max_ticks.exchange(0,std::memory_order_relaxed);
        const auto failed=a.skipped_clock.exchange(0,std::memory_order_relaxed);
        const double mean_us=n? (static_cast<double>(ticks)*1000000.)/
            (static_cast<double>(frequency)*static_cast<double>(n)):0.;
        const double max_us=(static_cast<double>(max)*1000000.)/
            static_cast<double>(frequency);
        const double est_ms_per_second =
            (static_cast<double>(ticks)*1000.*static_cast<double>(k_stride))/
            (static_cast<double>(frequency)*window_s);
        const auto approx_invocations=n*static_cast<std::uint64_t>(k_stride);
        char line[512]{};
        std::snprintf(line,sizeof(line),
            "[DSRRL V353 CPU_STAGE] window=%llu stage=%s samples=%llu"
            " sample_stride=%u estimated_invocations=%llu"
            " mean_sample_us=%.4f max_sample_us=%.4f"
            " projected_cpu_ms_per_s=%.4f clock_fail=%llu"
            " timing=CPU_INCLUSIVE_ESTIMATE gpu_time=NOT_MEASURED"
            " caution=OVERLAPPING_STAGES_DO_NOT_SUM",
            static_cast<unsigned long long>(window_index),
            k_stage_names[i],
            static_cast<unsigned long long>(n),k_stride,
            static_cast<unsigned long long>(approx_invocations),
            mean_us,max_us,est_ms_per_second,
            static_cast<unsigned long long>(failed));
        emit(line);
    }
}
inline void reset() noexcept {
    for (auto &a:g_samples) {
        a.n.store(0,std::memory_order_relaxed);
        a.sum_ticks.store(0,std::memory_order_relaxed);
        a.max_ticks.store(0,std::memory_order_relaxed);
        a.skipped_clock.store(0,std::memory_order_relaxed);
    }
    g_thread_sequence={};
}

} // namespace dsrrl::runtime::v353
