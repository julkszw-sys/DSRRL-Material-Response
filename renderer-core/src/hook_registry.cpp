#include "dsrrl/core/hook_registry.hpp"

namespace dsrrl::core {

bool hook_registry::claim(hook_claim claim) noexcept
{
    if (claim.site == 0 || claim.semantic == hook_semantic::unknown)
        return false;

    std::lock_guard lock(mutex_);
    const auto it = claims_.find(claim.site);
    if (it == claims_.end()) {
        claims_.emplace(claim.site, claim);
        return true;
    }

    return it->second.owner == claim.owner &&
           it->second.semantic == claim.semantic;
}

bool hook_registry::release(std::uint64_t site, operator_id owner) noexcept
{
    std::lock_guard lock(mutex_);
    const auto it = claims_.find(site);
    if (it == claims_.end() || it->second.owner != owner)
        return false;

    claims_.erase(it);
    return true;
}

std::optional<hook_claim> hook_registry::resolve(std::uint64_t site) const noexcept
{
    std::lock_guard lock(mutex_);
    const auto it = claims_.find(site);
    if (it == claims_.end())
        return std::nullopt;
    return it->second;
}

bool hook_registry::empty() const noexcept
{
    std::lock_guard lock(mutex_);
    return claims_.empty();
}

std::size_t hook_registry::size() const noexcept
{
    std::lock_guard lock(mutex_);
    return claims_.size();
}

} // namespace dsrrl::core
