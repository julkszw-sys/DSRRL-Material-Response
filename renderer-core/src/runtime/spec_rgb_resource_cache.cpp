#include "dsrrl/runtime/spec_rgb_resource_cache.hpp"

namespace dsrrl::runtime {
namespace {

spec_rgb_bind_plan fail_open(
    const operators::resource_bridges::spec_rgb_decision &route,
    spec_rgb_resource_handle stock_t1) noexcept
{
    spec_rgb_bind_plan out;
    out.route = route;
    out.preserved_t1_srv = stock_t1;
    return out;
}

} // namespace

spec_rgb_resource_result spec_rgb_resource_cache::register_exact(
    const std::u16string &logical_name,
    spec_rgb_resource_handle stock_t1_srv,
    spec_rgb_resource_handle ptde_t10_srv)
{
    if (logical_name.empty())
        return spec_rgb_resource_result::fail_open_invalid_identity;
    if (stock_t1_srv == 0 || ptde_t10_srv == 0)
        return spec_rgb_resource_result::fail_open_invalid_resource;

    std::lock_guard<std::mutex> lock(mutex_);

    const auto stock_owner = name_by_stock_.find(stock_t1_srv);
    if (stock_owner != name_by_stock_.end() &&
        stock_owner->second != logical_name) {
        // A stock SRV observed under two logical identities is no longer an
        // exact ownership carrier. Quarantine the previously accepted owner,
        // rather than leaving a stale route that could still activate later.
        const auto previous = by_name_.find(stock_owner->second);
        if (previous != by_name_.end()) {
            previous->second.stock_t1_srv = 0;
            previous->second.ptde_t10_srv = 0;
            previous->second.ambiguous = true;
        }
        name_by_stock_.erase(stock_owner);
        return spec_rgb_resource_result::fail_open_conflicting_stock_binding;
    }

    const auto found = by_name_.find(logical_name);
    if (found == by_name_.end()) {
        by_name_.emplace(
            logical_name,
            entry{logical_name, stock_t1_srv, ptde_t10_srv, false});
        name_by_stock_[stock_t1_srv] = logical_name;
        return spec_rgb_resource_result::registered;
    }

    entry &existing = found->second;
    if (existing.ambiguous)
        return spec_rgb_resource_result::fail_open_ambiguous_identity;

    if (existing.stock_t1_srv == stock_t1_srv &&
        existing.ptde_t10_srv == ptde_t10_srv)
        return spec_rgb_resource_result::already_registered;

    // Exact logical identity must never silently select between two resources.
    // Quarantine the name and remove its reverse association so future draws
    // preserve the host state until a complete identity can be re-established.
    name_by_stock_.erase(existing.stock_t1_srv);
    existing.stock_t1_srv = 0;
    existing.ptde_t10_srv = 0;
    existing.ambiguous = true;
    return spec_rgb_resource_result::fail_open_conflicting_companion;
}

spec_rgb_bind_plan spec_rgb_resource_cache::plan_bind(
    const spec_rgb_bind_request &request) const
{
    auto route = operators::resource_bridges::evaluate_spec_rgb_route(request.route);
    auto out = fail_open(route, request.currently_bound_t1);

    if (route.action !=
        operators::resource_bridges::spec_rgb_action::bind_ptde_t10_rgb)
        return out;

    if (request.logical_name.empty() || request.currently_bound_t1 == 0)
        return out;

    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = by_name_.find(request.logical_name);
    if (found == by_name_.end() || found->second.ambiguous)
        return out;

    const entry &record = found->second;
    if (record.stock_t1_srv == 0 || record.ptde_t10_srv == 0 ||
        record.stock_t1_srv != request.currently_bound_t1)
        return out;

    out.activate = true;
    out.ptde_t10_srv = record.ptde_t10_srv;
    return out;
}

void spec_rgb_resource_cache::erase_stock(
    spec_rgb_resource_handle stock_t1_srv)
{
    if (stock_t1_srv == 0)
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    const auto owner = name_by_stock_.find(stock_t1_srv);
    if (owner == name_by_stock_.end())
        return;

    const auto found = by_name_.find(owner->second);
    if (found != by_name_.end() &&
        found->second.stock_t1_srv == stock_t1_srv)
        by_name_.erase(found);

    name_by_stock_.erase(owner);
}

void spec_rgb_resource_cache::clear() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    by_name_.clear();
    name_by_stock_.clear();
}

std::size_t spec_rgb_resource_cache::size() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return by_name_.size();
}

} // namespace dsrrl::runtime
