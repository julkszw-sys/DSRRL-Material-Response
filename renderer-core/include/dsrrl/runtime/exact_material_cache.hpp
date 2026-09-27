#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>

namespace dsrrl::runtime {

enum class exact_material_cache_writer_phase : std::uint8_t {
    erase_claimed = 0,
    publish_claimed
};

using exact_material_cache_writer_hook =
    void (*)(
        exact_material_cache_writer_phase phase,
        std::uintptr_t key,
        void *context) noexcept;

// Small bounded lock-free material->route cache used by the retail MTD
// carrier. Writers serialize a slot through k_busy before touching the route
// ordinal. Readers validate material both before and after reading the route,
// so a route value can never be consumed across an erase/replacement epoch.
//
// EMPTY = 0, BUSY = 1. Real engine material pointers are aligned and therefore
// cannot equal BUSY. A BUSY slot is deliberately invisible to readers.
template <std::size_t Sets,std::size_t Ways>
class exact_material_cache {
public:
    static_assert(Sets > 0u);
    static_assert(Ways > 0u);
    static_assert(
        (Sets & (Sets - 1u)) == 0u,
        "exact_material_cache Sets must be a power of two");

    static constexpr std::uintptr_t k_empty = 0u;
    static constexpr std::uintptr_t k_busy = 1u;

    struct slot {
        std::atomic<std::uintptr_t> material{k_empty};
        std::atomic<std::uint16_t> route_ordinal_plus_one{0u};
    };

    struct set_type {
        std::array<slot,Ways> slots{};
    };

    bool erase(
        const void *material,
        exact_material_cache_writer_hook hook = nullptr,
        void *hook_context = nullptr) noexcept
    {
        if (material == nullptr)
            return false;

        const auto key =
            reinterpret_cast<std::uintptr_t>(material);
        if (!valid_key(key))
            return false;

        auto &set = sets_[set_index(material)];
        bool erased = false;

        for (auto &entry : set.slots) {
            for (;;) {
                auto observed =
                    entry.material.load(
                        std::memory_order_acquire);

                if (observed == k_busy) {
                    std::this_thread::yield();
                    continue;
                }

                if (observed != key)
                    break;

                if (!entry.material.compare_exchange_weak(
                        observed,
                        k_busy,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire))
                    continue;

                if (hook != nullptr)
                    hook(
                        exact_material_cache_writer_phase::
                            erase_claimed,
                        key,
                        hook_context);

                // Route is changed only while material==BUSY. Publish EMPTY
                // last so a following writer cannot be clobbered by a delayed
                // route clear from this erase generation.
                entry.route_ordinal_plus_one.store(
                    0u,
                    std::memory_order_relaxed);
                entry.material.store(
                    k_empty,
                    std::memory_order_release);
                erased = true;
                break;
            }
        }

        return erased;
    }

    bool publish(
        const void *material,
        std::uint16_t route_ordinal,
        std::uint16_t route_count,
        exact_material_cache_writer_hook hook = nullptr,
        void *hook_context = nullptr) noexcept
    {
        if (material == nullptr ||
            route_ordinal >= route_count ||
            route_ordinal == 0xffffu)
            return false;

        const auto key =
            reinterpret_cast<std::uintptr_t>(material);
        if (!valid_key(key))
            return false;

        const auto encoded =
            static_cast<std::uint16_t>(
                route_ordinal + 1u);
        auto &set = sets_[set_index(material)];

        for (;;) {
            bool busy_seen = false;

            // Prefer updating an existing stable key. Any BUSY member means a
            // writer is already mutating this set, so do not allocate another
            // slot until that writer publishes a stable state.
            for (auto &entry : set.slots) {
                auto observed =
                    entry.material.load(
                        std::memory_order_acquire);

                if (observed == k_busy) {
                    busy_seen = true;
                    continue;
                }

                if (observed != key)
                    continue;

                if (!entry.material.compare_exchange_weak(
                        observed,
                        k_busy,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    busy_seen = true;
                    continue;
                }

                publish_claimed(
                    entry,
                    key,
                    encoded,
                    hook,
                    hook_context);
                return true;
            }

            if (busy_seen) {
                std::this_thread::yield();
                continue;
            }

            // Claim an empty slot as BUSY first. Route publication happens
            // only after exclusive writer ownership is established.
            for (auto &entry : set.slots) {
                auto expected = k_empty;
                if (!entry.material.compare_exchange_strong(
                        expected,
                        k_busy,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire))
                    continue;

                publish_claimed(
                    entry,
                    key,
                    encoded,
                    hook,
                    hook_context);
                return true;
            }

            // Bounded deterministic replacement. Claim slot 0 from its stable
            // key to BUSY before changing the route. If another writer owns
            // it, retry the set instead of publishing through that generation.
            auto &entry = set.slots[0];
            auto observed =
                entry.material.load(
                    std::memory_order_acquire);

            if (observed == k_busy) {
                std::this_thread::yield();
                continue;
            }

            if (!entry.material.compare_exchange_weak(
                    observed,
                    k_busy,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
                continue;

            publish_claimed(
                entry,
                key,
                encoded,
                hook,
                hook_context);
            return true;
        }
    }

    bool lookup(
        const void *material,
        std::uint16_t route_count,
        std::uint16_t &route_ordinal) const noexcept
    {
        route_ordinal = 0u;
        if (material == nullptr)
            return false;

        const auto key =
            reinterpret_cast<std::uintptr_t>(material);
        if (!valid_key(key))
            return false;

        const auto &set = sets_[set_index(material)];

        for (const auto &entry : set.slots) {
            const auto before =
                entry.material.load(
                    std::memory_order_acquire);
            if (before != key)
                continue;

            const auto encoded =
                entry.route_ordinal_plus_one.load(
                    std::memory_order_acquire);

            // Writer protocol guarantees every route mutation occurs while
            // material==BUSY. Re-read the key after route load to reject an
            // erase/replacement that began after the first key observation.
            const auto after =
                entry.material.load(
                    std::memory_order_acquire);
            if (after != key)
                continue;

            if (encoded == 0u)
                return false;

            const auto ordinal =
                static_cast<std::size_t>(
                    encoded - 1u);
            if (ordinal >= route_count)
                return false;

            route_ordinal =
                static_cast<std::uint16_t>(
                    ordinal);
            return true;
        }

        return false;
    }

    void clear() noexcept
    {
        // Called after runtime hooks have been restored, so there are no live
        // producers/readers. Keep it simple while preserving stable EMPTY
        // publication order.
        for (auto &set : sets_)
            for (auto &entry : set.slots) {
                entry.route_ordinal_plus_one.store(
                    0u,
                    std::memory_order_relaxed);
                entry.material.store(
                    k_empty,
                    std::memory_order_release);
            }
    }

private:
    static bool valid_key(
        std::uintptr_t key) noexcept
    {
        return key > k_busy;
    }

    static std::size_t set_index(
        const void *material) noexcept
    {
        const auto p =
            reinterpret_cast<std::uintptr_t>(
                material);
        return static_cast<std::size_t>(
            ((p >> 4u) ^
             (p >> 13u) ^
             (p >> 23u)) &
            (Sets - 1u));
    }

    static void publish_claimed(
        slot &entry,
        std::uintptr_t key,
        std::uint16_t encoded,
        exact_material_cache_writer_hook hook,
        void *hook_context) noexcept
    {
        if (hook != nullptr)
            hook(
                exact_material_cache_writer_phase::
                    publish_claimed,
                key,
                hook_context);

        entry.route_ordinal_plus_one.store(
            encoded,
            std::memory_order_relaxed);
        entry.material.store(
            key,
            std::memory_order_release);
    }

    std::array<set_type,Sets> sets_{};
};

} // namespace dsrrl::runtime
