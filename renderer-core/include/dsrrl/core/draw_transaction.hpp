#pragma once

#include "types.hpp"

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace dsrrl::core {

struct island_patch {
    operator_id op = operator_id::material_response;
    std::uint32_t carrier_write_mask = 0;
    bool shader_replacement = false;
    bool resource_replacement = false;
};

struct render_patch_plan {
    std::array<island_patch, operator_count> patches{};
    std::uint32_t patch_count = 0;
    std::uint32_t carrier_write_mask = 0;

    bool empty() const noexcept { return patch_count == 0; }
};

struct transaction_state {
    std::uint64_t command = 0;
    std::uint64_t draw_serial = 0;
    context_kind context = context_kind::unknown;
    render_patch_plan plan{};
};

class draw_transaction_manager {
public:
    bool begin(
        std::uint64_t command,
        std::uint64_t draw_serial,
        context_kind context,
        const render_patch_plan &plan);
    bool restore(std::uint64_t command);
    std::optional<transaction_state> active(std::uint64_t command) const;
    bool empty() const noexcept;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, transaction_state> active_;
};

} // namespace dsrrl::core
