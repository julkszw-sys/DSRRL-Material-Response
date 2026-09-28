#pragma once

#include <atomic>
#include <cstdint>
#include <cstdlib>

namespace dsrrl::runtime::telemetry {

inline bool environment_flag(
    const char *name) noexcept
{
#if defined(_MSC_VER)
    char *value = nullptr;
    std::size_t length = 0u;
    if (_dupenv_s(
            &value,
            &length,
            name) != 0 ||
        value == nullptr)
        return false;

    const bool enabled =
        length == 2u &&
        value[0] == '1' &&
        value[1] == '\0';
    std::free(value);
    return enabled;
#else
    const char *value =
        std::getenv(name);
    return value != nullptr &&
        value[0] == '1' &&
        value[1] == '\0';
#endif
}

inline bool hot_enabled() noexcept
{
#if defined(DSRRL_RUNTIME_TELEMETRY_DEFAULT_ON)
    return true;
#else
    static const bool enabled =
        environment_flag(
            "DSRRL_RUNTIME_TELEMETRY");
    return enabled;
#endif
}

inline bool effect_enabled() noexcept
{
#if defined(DSRRL_EFFECT_TELEMETRY_DEFAULT_ON)
    return true;
#else
    static const bool enabled =
        environment_flag(
            "DSRRL_EFFECT_TELEMETRY");
    return enabled;
#endif
}

inline void hot_count(
    std::atomic<std::uint64_t> &counter) noexcept
{
    if (hot_enabled())
        counter.fetch_add(
            1u,
            std::memory_order_relaxed);
}

inline bool native_state_verification_enabled() noexcept
{
#if defined(DSRRL_RUNTIME_VERIFY_STATE_DEFAULT_ON)
    return true;
#else
    static const bool enabled =
        environment_flag(
            "DSRRL_RUNTIME_VERIFY_STATE");
    return enabled;
#endif
}

} // namespace dsrrl::runtime::telemetry
