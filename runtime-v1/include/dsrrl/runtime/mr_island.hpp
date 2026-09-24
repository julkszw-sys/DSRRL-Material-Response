#pragma once
#include <cstdint>

namespace dsrrl::core { class renderer_core; }

namespace dsrrl::runtime::mr {

bool register_runtime(core::renderer_core &core) noexcept;
void unregister_runtime() noexcept;

void selector_event(void *container, void *owner, void *ret, void *r14, void *r15,
                    std::int32_t material_index) noexcept;
void mtd_event(void *material, const void *raw, std::uint32_t len) noexcept;

} // namespace dsrrl::runtime::mr
