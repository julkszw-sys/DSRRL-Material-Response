#pragma once
#include <cstdint>

namespace dsrrl::runtime {

// Diagnostic-only CPU writer provenance. This is neither native pass
// equivalence nor evidence that a captured CPU upload reached the GPU.
struct pmetal_native_cb_writer_stamp {
    std::uint64_t epoch = 0u;
    std::uint64_t hash = 0u;
    std::uint64_t timestamp_ms = 0u;
    std::uint32_t writer_tid = 0u;
    std::uint32_t byte_count = 0u;
    std::uint8_t method = 0u; // 1=Map/Unmap, 2=UpdateSubresource
    bool watched = false;
    bool complete = false;
};

void pmetal_native_cb_writer_watch(
    std::uintptr_t native_buffer,
    std::uint32_t exact_byte_width) noexcept;
pmetal_native_cb_writer_stamp pmetal_native_cb_writer_lookup(
    std::uintptr_t native_buffer) noexcept;
void pmetal_native_cb_writer_after_map(
    std::uintptr_t native_buffer, void *mapped,
    std::uint64_t offset, std::uint64_t size,
    bool writable) noexcept;
void pmetal_native_cb_writer_before_unmap(
    std::uintptr_t native_buffer) noexcept;
void pmetal_native_cb_writer_before_update(
    std::uintptr_t native_buffer, const void *src,
    std::uint64_t offset, std::uint64_t size) noexcept;
void pmetal_native_cb_writer_destroy(
    std::uintptr_t native_buffer) noexcept;
void pmetal_native_cb_writer_reset() noexcept;
} // namespace dsrrl::runtime
