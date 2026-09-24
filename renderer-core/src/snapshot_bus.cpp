#include "dsrrl/core/snapshot_bus.hpp"

namespace dsrrl::core {

bool snapshot_bus::publish(
    const semantic_key &key,
    std::uint64_t producer_epoch,
    const semantic_payload &payload)
{
    if (!valid_operator_id(key.op) ||
        key.owner == 0 ||
        payload.lane_count > max_snapshot_lanes)
        return false;

    std::lock_guard lock(mutex_);
    semantic_snapshot snapshot;
    snapshot.key = key;
    snapshot.sequence = ++sequence_;
    snapshot.producer_epoch = producer_epoch;
    snapshot.payload = payload;
    snapshots_[key] = snapshot;
    return true;
}

std::optional<semantic_snapshot> snapshot_bus::latest(const semantic_key &key) const
{
    std::lock_guard lock(mutex_);
    const auto it = snapshots_.find(key);
    if (it == snapshots_.end())
        return std::nullopt;
    return it->second;
}

void snapshot_bus::clear_owner(std::uint64_t owner)
{
    std::lock_guard lock(mutex_);
    for (auto it = snapshots_.begin(); it != snapshots_.end();) {
        if (it->first.owner == owner)
            it = snapshots_.erase(it);
        else
            ++it;
    }
}

void snapshot_bus::clear_all()
{
    std::lock_guard lock(mutex_);
    snapshots_.clear();
}

std::size_t snapshot_bus::size() const noexcept
{
    std::lock_guard lock(mutex_);
    return snapshots_.size();
}

} // namespace dsrrl::core
