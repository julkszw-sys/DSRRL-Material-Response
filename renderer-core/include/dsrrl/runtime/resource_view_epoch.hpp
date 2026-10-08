#pragma once
// Narrow synchronization primitive for the DSRRL companion-SRV TLS cache.
// A mutation only invalidates its resource-view hash bucket; colliding keys
// may be conservatively invalidated. Reset/device teardown invalidates all.
// This never changes resource identity or ownership and must be paired with
// the original authoritative g_cache lookup under g_mutex on a TLS miss.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime {

template<std::size_t Slots>
class resource_view_epoch final {
    static_assert(Slots > 0u && (Slots & (Slots - 1u)) == 0u,
                  "Epoch bucket count must be a power of two");

public:
    static constexpr std::size_t index(std::uint64_t key) noexcept {
        return static_cast<std::size_t>(
            ((key >> 4u) ^ (key >> 13u) ^ (key >> 23u)) &
            (Slots - 1u));
    }

    std::uint64_t current(std::uint64_t key) const noexcept {
        return versions_[index(key)].load(std::memory_order_acquire);
    }

    // Called under the authoritative g_mutex after a specific key changes.
    // Release ordering publishes prior map mutation to TLS readers.
    void invalidate(std::uint64_t key) noexcept {
        versions_[index(key)].fetch_add(1u, std::memory_order_release);
    }

    // Reset/device teardown: all TLS slots become invalid without mutating
    // any thread_local storage belonging to another thread.
    void invalidate_all() noexcept {
        for (auto &version : versions_)
            version.fetch_add(1u, std::memory_order_release);
    }

private:
    std::array<std::atomic<std::uint64_t>, Slots> versions_{};
};

} // namespace dsrrl::runtime
