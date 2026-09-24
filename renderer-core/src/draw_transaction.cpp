#include "dsrrl/core/draw_transaction.hpp"

namespace dsrrl::core {

bool draw_transaction_manager::begin(
    std::uint64_t command,
    std::uint64_t draw_serial,
    context_kind context,
    const render_patch_plan &plan)
{
    if (command == 0 || plan.empty())
        return false;

    std::uint32_t seen_mask = 0;
    for (std::uint32_t i = 0; i < plan.patch_count; ++i) {
        const std::uint32_t mask = plan.patches[i].carrier_write_mask;
        if ((seen_mask & mask) != 0)
            return false;
        seen_mask |= mask;
    }

    if (seen_mask != plan.carrier_write_mask)
        return false;

    std::lock_guard lock(mutex_);
    if (active_.find(command) != active_.end())
        return false;

    active_.emplace(command, transaction_state{command, draw_serial, context, plan});
    return true;
}

bool draw_transaction_manager::restore(std::uint64_t command)
{
    std::lock_guard lock(mutex_);
    const auto it = active_.find(command);
    if (it == active_.end())
        return false;
    active_.erase(it);
    return true;
}

std::optional<transaction_state> draw_transaction_manager::active(std::uint64_t command) const
{
    std::lock_guard lock(mutex_);
    const auto it = active_.find(command);
    if (it == active_.end())
        return std::nullopt;
    return it->second;
}

bool draw_transaction_manager::empty() const noexcept
{
    std::lock_guard lock(mutex_);
    return active_.empty();
}

} // namespace dsrrl::core
