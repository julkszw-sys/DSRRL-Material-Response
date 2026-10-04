#pragma once

#include "types.hpp"

#include <array>
#include <atomic>
#include <mutex>

namespace dsrrl::core {

class feature_registry {
public:
    feature_registry() = default;

    bool set(operator_id op, bool enabled) noexcept;
    bool enabled(operator_id op) const noexcept;
    bool all_disabled() const noexcept;
    std::array<bool, operator_count> snapshot() const noexcept;

private:
    static constexpr std::size_t index(operator_id op) noexcept
    {
        return static_cast<std::size_t>(op);
    }

    mutable std::mutex mutex_;
    std::array<bool, operator_count> enabled_{};
    // Hot-path readers need only the enabled bit. Writers remain serialized
    // with the canonical bool array so snapshot() retains its coherent view.
    std::atomic<operator_mask> enabled_mask_{0u};
};

} // namespace dsrrl::core
