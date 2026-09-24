#pragma once
#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime::engine {

using selector_callback = void(*)(void *container, void *owner, void *ret, void *r14, void *r15, std::int32_t material_index) noexcept;
using mtd_callback = void(*)(void *material, const void *raw, std::uint32_t len) noexcept;

struct upper_lower_callbacks {
    void (*wrapper_enter)(void *owner, void *assignment) noexcept = nullptr;
    void (*wrapper_exit)() noexcept = nullptr;
    void (*steady_cache)(void *source, std::int32_t selector) noexcept = nullptr;
    void (*true_blend)(const void *a, const void *b, float beta, std::uintptr_t return_rva) noexcept = nullptr;
};

bool verify_provenance() noexcept;

// Base EngineBridge: one shared selector hook + MTD parse hook.
bool install(selector_callback selector, mtd_callback mtd) noexcept;

// Optional U/L producer hooks owned by EngineBridge. Failure does not remove the
// already-installed selector/MTD base hooks; caller may fail-open only U/L.
bool install_upper_lower(const upper_lower_callbacks &callbacks) noexcept;
void uninstall_upper_lower() noexcept;

void uninstall() noexcept;
std::uintptr_t image_base() noexcept;
bool safe_read_bytes(const void *src, void *dst, std::size_t size) noexcept;

} // namespace dsrrl::runtime::engine
