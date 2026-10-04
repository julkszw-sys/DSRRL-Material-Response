#include "dsrrl/core/feature_registry.hpp"

namespace dsrrl::core {

bool feature_registry::set(operator_id op, bool enabled) noexcept
{
    const auto i = index(op);
    if (i >= operator_count)
        return false;

    std::lock_guard lock(mutex_);
    enabled_[i] = enabled;

    const auto bit = operator_bit(op);
    auto mask =
        enabled_mask_.load(
            std::memory_order_relaxed);
    if (enabled)
        mask |= bit;
    else
        mask &= ~bit;
    enabled_mask_.store(
        mask,
        std::memory_order_release);
    return true;
}

bool feature_registry::enabled(operator_id op) const noexcept
{
    const auto i = index(op);
    if (i >= operator_count)
        return false;

    return
        (enabled_mask_.load(
             std::memory_order_acquire) &
         operator_bit(op)) != 0u;
}

bool feature_registry::all_disabled() const noexcept
{
    std::lock_guard lock(mutex_);
    for (const bool value : enabled_)
        if (value)
            return false;
    return true;
}

std::array<bool, operator_count> feature_registry::snapshot() const noexcept
{
    std::lock_guard lock(mutex_);
    return enabled_;
}

} // namespace dsrrl::core
