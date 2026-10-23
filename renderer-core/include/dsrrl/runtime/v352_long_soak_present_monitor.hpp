#pragma once
// V352 long soak -- Present cadence only. QPC once per Present, no GPU
// readback, heap allocation, file I/O, or per-draw clock collection.
// The durations below are PRESENT INTERVALS (not GPU frame time, nor
// addon CPU execution time). Keep these domains distinct in any report.
#include <reshade.hpp>
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>

namespace dsrrl::runtime::v352 {

class long_soak_present_monitor {
public:
    void reset() noexcept {
        first_tick_ = 0;
        last_tick_ = 0;
        window_tick_ = 0;
        window_frames_ = 0;
        window_intervals_ = 0;
        sum_us_ = 0;
        min_us_ = UINT64_MAX;
        max_us_ = 0;
        hitch_33_ = 0;
        hitch_50_ = 0;
        hitch_100_ = 0;
        intervals_ = {};
        windows_ = 0;
        frequency_ = 0;
        emitted_start_ = false;
    }

    // Returns true only once per elapsed 30s reporting window.
    bool present() noexcept {
        if (frequency_ == 0) {
            LARGE_INTEGER f{};
            if (!QueryPerformanceFrequency(&f) || f.QuadPart <= 0)
                return false;
            frequency_ = static_cast<std::uint64_t>(f.QuadPart);
        }
        LARGE_INTEGER now{};
        if (!QueryPerformanceCounter(&now))
            return false;
        const auto tick = static_cast<std::uint64_t>(now.QuadPart);
        if (!emitted_start_) {
            first_tick_ = window_tick_ = last_tick_ = tick;
            emitted_start_ = true;
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL V352 SOAK] start=1"
                " telemetry=RUNTIME+EFFECT+PRESENT_QPC"
                " window_s=30 cadence=PRESENT_INTERVAL_CPU_WALLCLOCK"
                " gpu_time=NOT_MEASURED addon_cpu_time=NOT_MEASURED"
                " native_state_verify=OFF disk_exe_unchanged=1");
            return false;
        }
        if (tick <= last_tick_) {
            last_tick_ = tick;
            return false;
        }
        const auto dt_us = (tick - last_tick_) * 1000000ull / frequency_;
        last_tick_ = tick;
        ++window_intervals_;
        ++window_frames_;
        sum_us_ += dt_us;
        if (dt_us < min_us_) min_us_ = dt_us;
        if (dt_us > max_us_) max_us_ = dt_us;
        if (dt_us >= 33000ull) ++hitch_33_;
        if (dt_us >= 50000ull) ++hitch_50_;
        if (dt_us >= 100000ull) ++hitch_100_;
        const auto ms_bucket = dt_us >= 200000ull
            ? 200u : static_cast<std::size_t>((dt_us + 999ull) / 1000ull);
        ++intervals_[ms_bucket];

        const auto elapsed_tick = tick - window_tick_;
        if (elapsed_tick < frequency_ * 30ull)
            return false;

        const double elapsed_s =
            static_cast<double>(tick - first_tick_) /
            static_cast<double>(frequency_);
        const double window_s =
            static_cast<double>(elapsed_tick) /
            static_cast<double>(frequency_);
        const double fps =
            static_cast<double>(window_frames_) / window_s;
        const double avg_ms =
            window_intervals_ != 0
                ? static_cast<double>(sum_us_) / (window_intervals_ * 1000.0)
                : 0.;
        const auto p95 = percentile_ceiling_ms(95u);
        const auto p99 = percentile_ceiling_ms(99u);
        char line[620]{};
        std::snprintf(
            line,sizeof(line),
            "[DSRRL V352 PERF] window=%llu elapsed_s=%.2f window_s=%.2f"
            " presents=%llu avg_fps=%.3f avg_interval_ms=%.3f"
            " min_interval_ms=%.3f p95_interval_ms_le=%u"
            " p99_interval_ms_le=%u max_interval_ms=%.3f"
            " interval_ge_33ms=%llu interval_ge_50ms=%llu interval_ge_100ms=%llu"
            " histogram_bucket_ms=1 histogram_cap_ms=200"
            " measurement=PRESENT_INTERVAL_NOT_GPU",
            static_cast<unsigned long long>(++windows_),
            elapsed_s,window_s,
            static_cast<unsigned long long>(window_frames_),
            fps,avg_ms,
            static_cast<double>(min_us_) / 1000.0,
            p95,p99,
            static_cast<double>(max_us_) / 1000.0,
            static_cast<unsigned long long>(hitch_33_),
            static_cast<unsigned long long>(hitch_50_),
            static_cast<unsigned long long>(hitch_100_));
        reshade::log::message(reshade::log::level::info,line);
        window_tick_ = tick;
        window_frames_ = 0;
        window_intervals_ = 0;
        sum_us_ = 0;
        min_us_ = UINT64_MAX;
        max_us_ = 0;
        hitch_33_ = hitch_50_ = hitch_100_ = 0;
        intervals_ = {};
        return true;
    }

private:
    std::uint32_t percentile_ceiling_ms(unsigned percentile) const noexcept {
        if (window_intervals_ == 0) return 0;
        const auto target =
            (window_intervals_ * percentile + 99ull) / 100ull;
        std::uint64_t sum = 0;
        for (std::uint32_t i = 0; i < intervals_.size(); ++i) {
            sum += intervals_[i];
            if (sum >= target) return i;
        }
        return 200u;
    }

    std::uint64_t first_tick_ = 0;
    std::uint64_t last_tick_ = 0;
    std::uint64_t window_tick_ = 0;
    std::uint64_t frequency_ = 0;
    std::uint64_t window_frames_ = 0;
    std::uint64_t window_intervals_ = 0;
    std::uint64_t sum_us_ = 0;
    std::uint64_t min_us_ = UINT64_MAX;
    std::uint64_t max_us_ = 0;
    std::uint64_t hitch_33_ = 0;
    std::uint64_t hitch_50_ = 0;
    std::uint64_t hitch_100_ = 0;
    std::uint64_t windows_ = 0;
    std::array<std::uint64_t,201> intervals_{};
    bool emitted_start_ = false;
};

} // namespace dsrrl::runtime::v352
