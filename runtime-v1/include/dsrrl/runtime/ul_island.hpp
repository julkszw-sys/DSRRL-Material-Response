#pragma once

#include "dsrrl/core/types.hpp"
#include "dsrrl/runtime/engine_hooks.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace dsrrl::core { class renderer_core; }

namespace dsrrl::runtime::ul {

struct draw_snapshot {
    std::uintptr_t owner = 0;
    std::uint16_t area_a = 0;
    std::uint16_t area_b = 0;
    std::uint32_t beta_bits = 0;
    alignas(16) std::array<core::float4,8> carrier{};
};

bool register_runtime(core::renderer_core &core) noexcept;
void unregister_runtime() noexcept;

engine::upper_lower_callbacks callbacks() noexcept;

void selector_event(
    void *owner,void *ret,void *r14,void *r15) noexcept;

std::shared_ptr<const draw_snapshot> consume_draw_snapshot() noexcept;
void clear_draw_snapshot() noexcept;

bool native_draw_matches(
    const draw_snapshot &snapshot,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance,
    std::uint32_t &native_instances) noexcept;

} // namespace dsrrl::runtime::ul
