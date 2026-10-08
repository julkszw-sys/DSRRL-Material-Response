#pragma once
// R43 v2.0.0 streaming-stutter diagnostics.
// Instrumentation only: no feature gates, renderer state, routing or asset identity changes.
// The normal release compiles to no-ops. No I/O or allocation occurs in timed hooks.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#if defined(DSRRL_STUTTER_PROFILE) && defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace dsrrl::runtime::stutter_profile {

enum class stage : std::size_t {
    flver_parse = 0,
    flver_digest,
    flver_registry,
    flver_lookup_fallback,
    flver_destroy,
    mtd_observe,
    selector,
    pmetal_selector,
    resource_view_init,
    sidecar_load,
    d3d_texture_create,
    sidecar_pool_hit,
    sidecar_pool_miss,
    count
};

inline constexpr std::size_t stage_count =
    static_cast<std::size_t>(stage::count);

inline constexpr std::array<const char *,stage_count> stage_names{{
    "FLVER_PARSE", "FLVER_SHA256", "FLVER_REGISTRY",
    "FLVER_LOOKUP_FALLBACK", "FLVER_DESTROY",
    "MTD_OBSERVE", "SELECTOR", "PMETAL_SELECTOR",
    "RESOURCE_VIEW_INIT", "DDS_LOAD", "CREATE_TEXTURE2D",
    "DDS_POOL_HIT", "DDS_POOL_MISS"
}};

struct snapshot {
    std::uint64_t calls = 0;
    std::uint64_t total_ticks = 0;
    std::uint64_t max_ticks = 0;
    std::uint64_t over_100ms = 0;
};

struct frame_snapshot {
    std::uint64_t frames = 0;
    std::uint64_t max_ticks = 0;
    std::uint64_t over_100ms = 0;
};

#if defined(DSRRL_STUTTER_PROFILE) && defined(_WIN32)
struct counter {
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> total_ticks{0};
    std::atomic<std::uint64_t> max_ticks{0};
    std::atomic<std::uint64_t> over_100ms{0};
};

inline std::array<counter,stage_count> counters{};
inline std::atomic<std::uint64_t> present_previous{0};
inline std::atomic<std::uint64_t> present_frames{0};
inline std::atomic<std::uint64_t> present_max{0};
inline std::atomic<std::uint64_t> present_over_100ms{0};
inline std::atomic<std::uint64_t> report_deadline{0};

inline std::uint64_t ticks() noexcept {
    LARGE_INTEGER value{};
    (void)QueryPerformanceCounter(&value);
    return static_cast<std::uint64_t>(value.QuadPart);
}

inline std::uint64_t frequency() noexcept {
    static const std::uint64_t value = []() noexcept {
        LARGE_INTEGER freq{};
        if (!QueryPerformanceFrequency(&freq) || freq.QuadPart <= 0)
            return std::uint64_t{1};
        return static_cast<std::uint64_t>(freq.QuadPart);
    }();
    return value;
}

inline void update_max(std::atomic<std::uint64_t> &target,
                       std::uint64_t measured) noexcept {
    auto current = target.load(std::memory_order_relaxed);
    while (current < measured &&
           !target.compare_exchange_weak(
               current, measured,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {}
}

inline void record(stage id, std::uint64_t elapsed_ticks) noexcept {
    auto &c = counters[static_cast<std::size_t>(id)];
    c.calls.fetch_add(1u, std::memory_order_relaxed);
    c.total_ticks.fetch_add(elapsed_ticks, std::memory_order_relaxed);
    update_max(c.max_ticks, elapsed_ticks);
    if (elapsed_ticks >= frequency() / 10u)
        c.over_100ms.fetch_add(1u, std::memory_order_relaxed);
}

struct scope {
    stage id;
    std::uint64_t begin;
    explicit scope(stage value) noexcept : id(value), begin(ticks()) {}
    ~scope() noexcept { record(id, ticks() - begin); }
    scope(const scope &) = delete;
    scope &operator=(const scope &) = delete;
};

inline bool on_present_due() noexcept {
    const auto now = ticks();
    const auto previous =
        present_previous.exchange(now, std::memory_order_relaxed);
    if (previous != 0 && now >= previous) {
        const auto elapsed = now - previous;
        present_frames.fetch_add(1u, std::memory_order_relaxed);
        update_max(present_max, elapsed);
        if (elapsed >= frequency() / 10u)
            present_over_100ms.fetch_add(1u, std::memory_order_relaxed);
    }

    auto deadline = report_deadline.load(std::memory_order_relaxed);
    if (deadline == 0u) {
        const auto initial = now + 5u * frequency();
        (void)report_deadline.compare_exchange_strong(
            deadline, initial, std::memory_order_relaxed);
        return false;
    }
    if (now < deadline)
        return false;

    return report_deadline.compare_exchange_strong(
        deadline, now + 5u * frequency(),
        std::memory_order_relaxed);
}

inline frame_snapshot take_frame_snapshot() noexcept {
    return {
        present_frames.exchange(0u, std::memory_order_relaxed),
        present_max.exchange(0u, std::memory_order_relaxed),
        present_over_100ms.exchange(0u, std::memory_order_relaxed)
    };
}
inline snapshot take_stage_snapshot(stage id) noexcept {
    auto &c = counters[static_cast<std::size_t>(id)];
    return {
        c.calls.exchange(0u, std::memory_order_relaxed),
        c.total_ticks.exchange(0u, std::memory_order_relaxed),
        c.max_ticks.exchange(0u, std::memory_order_relaxed),
        c.over_100ms.exchange(0u, std::memory_order_relaxed)
    };
}
#else
struct scope {
    explicit scope(stage) noexcept {}
};
#endif

} // namespace dsrrl::runtime::stutter_profile
