#pragma once

#include "types.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace dsrrl::core {

enum class hook_semantic : std::uint8_t {
    unknown = 0,
    lightbank_single_packer,
    lightbank_true_blend,
    point_light_source,
    material_route,
    count
};

struct hook_claim {
    std::uint64_t site = 0;
    operator_id owner = operator_id::material_response;
    hook_semantic semantic = hook_semantic::unknown;
};

class hook_registry {
public:
    bool claim(hook_claim claim) noexcept;
    bool release(std::uint64_t site, operator_id owner) noexcept;
    std::optional<hook_claim> resolve(std::uint64_t site) const noexcept;
    bool empty() const noexcept;
    std::size_t size() const noexcept;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, hook_claim> claims_;
};

} // namespace dsrrl::core
