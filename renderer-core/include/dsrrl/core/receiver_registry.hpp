#pragma once

#include "types.hpp"

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace dsrrl::core {

using sha256_digest = std::array<std::uint8_t, 32>;

struct receiver_descriptor {
    std::uint32_t receiver_id = 0;
    std::uint64_t fast_hash = 0;
    sha256_digest exact_sha256{};
    std::uint64_t consumer_family_hash = 0;
    operator_mask capabilities = 0;
};

class receiver_registry {
public:
    bool register_receiver(const receiver_descriptor &descriptor);
    std::optional<receiver_descriptor> resolve(
        std::uint64_t fast_hash,
        const sha256_digest &exact_sha256) const;
    std::size_t size() const noexcept;

private:
    mutable std::mutex mutex_;
    std::vector<receiver_descriptor> receivers_;
};

} // namespace dsrrl::core
