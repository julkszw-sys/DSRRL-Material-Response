#pragma once
#include <cstdint>

namespace dsrrl::runtime::engine {

using selector_callback = void(*)(void *container, void *owner, void *ret, void *r14, void *r15, std::int32_t material_index) noexcept;
using mtd_callback = void(*)(void *material, const void *raw, std::uint32_t len) noexcept;

bool verify_provenance() noexcept;
bool install(selector_callback selector, mtd_callback mtd) noexcept;
void uninstall() noexcept;
std::uintptr_t image_base() noexcept;
bool safe_read_bytes(const void *src, void *dst, std::size_t size) noexcept;

} // namespace dsrrl::runtime::engine
