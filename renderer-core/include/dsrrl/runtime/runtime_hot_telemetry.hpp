#pragma once

#include <atomic>
#include <cstdint>
#include <cstdlib>

namespace dsrrl::runtime::telemetry {

inline bool hot_enabled() noexcept
{
    static const bool enabled = []() noexcept {
        const char *value =
            std::getenv("DSRRL_RUNTIME_TELEMETRY");
        return value != nullptr &&
            value[0] == '1' &&
            value[1] == '\0';
    }();

    return enabled;
}

inline void hot_count(
    std::atomic<std::uint64_t> &counter) noexcept
{
    if (hot_enabled())
        counter.fetch_add(
            1u,
            std::memory_order_relaxed);
}

} // namespace dsrrl::runtime::telemetry
