#pragma once
// SPC25: non-owning native SRV lifetime census; no texture mutation.
// Exact native (device, view, resource) generations are observed, never
// promoted to SpecRGB binding authority without a separate source/receiver
// proof. Bounded 16-probe table: capacity/collision => stock DSR fail-open.
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>

namespace dsrrl::runtime::native_ps_t1_lifetime {
struct observation {
    std::uint64_t epoch = 0;
    std::uintptr_t device = 0;
    explicit operator bool() const noexcept { return epoch != 0; }
};

template<std::size_t Capacity = 4096u> class registry final {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1u)) == 0,
                  "SPC25 epoch capacity must be power-of-two");
    static constexpr std::size_t probe_limit =
        Capacity < 16u ? Capacity : 16u;
    struct entry {
        std::uintptr_t device = 0, view = 0, resource = 0;
        std::uint64_t epoch = 0;
        bool active = false, ambiguous = false;
    };
    std::mutex mutex_;
    std::array<entry, Capacity> slots_{};
    std::uint64_t next_epoch_ = 0;

    static std::size_t bucket(std::uintptr_t view) noexcept {
        return static_cast<std::size_t>(
            ((view >> 4u) ^ (view >> 17u) ^ (view >> 29u)) &
            (Capacity - 1u));
    }

public:
    std::uint64_t init(std::uintptr_t device, std::uintptr_t view,
                       std::uintptr_t resource) noexcept {
        if (!device || !view || !resource)
            return 0;
        std::lock_guard<std::mutex> lock(mutex_);
        const auto start = bucket(view);
        std::size_t free_slot = Capacity;
        bool duplicate = false;
        // Scan the whole probe window BEFORE recycling a tombstone.
        // Otherwise a live duplicate later in the window could be missed.
        for (std::size_t i = 0; i < probe_limit; ++i) {
            const auto index = (start + i) & (Capacity - 1u);
            auto &slot = slots_[index];
            if (slot.active && slot.view == view) {
                slot.ambiguous = true; // Fail closed for both lifetimes.
                duplicate = true;
            } else if (!slot.active && free_slot == Capacity) {
                free_slot = index;
            }
        }
        if (duplicate || free_slot == Capacity ||
            next_epoch_ == std::numeric_limits<std::uint64_t>::max())
            return 0;
        slots_[free_slot] = entry{device, view, resource, ++next_epoch_,
                                  true, false};
        return next_epoch_;
    }

    void destroy(std::uintptr_t device, std::uintptr_t view) noexcept {
        if (!device || !view)
            return;
        std::lock_guard<std::mutex> lock(mutex_);
        const auto start = bucket(view);
        for (std::size_t i = 0; i < probe_limit; ++i) {
            auto &slot = slots_[(start + i) & (Capacity - 1u)];
            if (slot.active && slot.device == device && slot.view == view)
                slot.active = false;
        }
    }

    void destroy_device(std::uintptr_t device) noexcept {
        if (!device)
            return;
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &slot : slots_)
            if (slot.active && slot.device == device)
                slot.active = false;
    }

    observation current(std::uintptr_t view,
                        std::uintptr_t resource) noexcept {
        if (!view || !resource)
            return {};
        std::lock_guard<std::mutex> lock(mutex_);
        const auto start = bucket(view);
        observation found{};
        for (std::size_t i = 0; i < probe_limit; ++i) {
            const auto &slot = slots_[(start + i) & (Capacity - 1u)];
            if (!slot.active || slot.view != view)
                continue;
            if (slot.ambiguous || slot.resource != resource || found.epoch)
                return {};
            found = {slot.epoch, slot.device};
        }
        return found;
    }
};

// One inline function-local registry instance shared by both addon TUs.
inline registry<> &instance() noexcept {
    static registry<> epochs;
    return epochs;
}
} // namespace dsrrl::runtime::native_ps_t1_lifetime
