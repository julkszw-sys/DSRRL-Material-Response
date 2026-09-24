#pragma once

#include "types.hpp"

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace dsrrl::core {

constexpr std::size_t max_snapshot_lanes = 16;

struct semantic_payload {
    std::array<float4, max_snapshot_lanes> lanes{};
    std::uint32_t lane_count = 0;
};

struct semantic_snapshot {
    semantic_key key{};
    std::uint64_t sequence = 0;
    std::uint64_t producer_epoch = 0;
    semantic_payload payload{};
};

class snapshot_bus {
public:
    bool publish(const semantic_key &key, std::uint64_t producer_epoch, const semantic_payload &payload);
    std::optional<semantic_snapshot> latest(const semantic_key &key) const;
    void clear_owner(std::uint64_t owner);
    void clear_all();
    std::size_t size() const noexcept;

private:
    mutable std::mutex mutex_;
    std::uint64_t sequence_ = 0;
    std::unordered_map<semantic_key, semantic_snapshot, semantic_key_hash> snapshots_;
};

} // namespace dsrrl::core
