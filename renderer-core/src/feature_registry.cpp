#include "dsrrl/core/feature_registry.hpp"

namespace dsrrl::core {

bool feature_registry::set(operator_id op, bool enabled) noexcept
{
    const auto i = index(op);
    if (i >= operator_count)
        return false;

    std::lock_guard lock(mutex_);
    enabled_[i] = enabled;
    return true;
}

bool feature_registry::enabled(operator_id op) const noexcept
{
    const auto i = index(op);
    if (i >= operator_count)
        return false;

    std::lock_guard lock(mutex_);
    return enabled_[i];
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
