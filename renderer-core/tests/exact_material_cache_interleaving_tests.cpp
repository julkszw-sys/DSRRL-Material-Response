#include "dsrrl/runtime/exact_material_cache.hpp"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>

using namespace dsrrl;

#define CHECK(x) do { if(!(x)){ std::cerr << "CHECK failed: " #x "\n"; return 1; } } while(false)

namespace {

struct writer_gate {
    std::uintptr_t old_key = 0u;
    std::uintptr_t new_key = 0u;
    std::atomic_bool erase_claimed{false};
    std::atomic_bool allow_erase_commit{false};
    std::atomic_bool publish_claimed{false};
    std::atomic_bool allow_publish_commit{false};
};

void writer_hook(
    runtime::exact_material_cache_writer_phase phase,
    std::uintptr_t key,
    void *context) noexcept
{
    auto &gate =
        *static_cast<writer_gate *>(context);

    if (phase ==
            runtime::exact_material_cache_writer_phase::
                erase_claimed &&
        key == gate.old_key) {
        gate.erase_claimed.store(
            true,
            std::memory_order_release);
        while (!gate.allow_erase_commit.load(
                    std::memory_order_acquire))
            std::this_thread::yield();
        return;
    }

    if (phase ==
            runtime::exact_material_cache_writer_phase::
                publish_claimed &&
        key == gate.new_key) {
        gate.publish_claimed.store(
            true,
            std::memory_order_release);
        while (!gate.allow_publish_commit.load(
                    std::memory_order_acquire))
            std::this_thread::yield();
    }
}

bool wait_until(
    const std::atomic_bool &flag) noexcept
{
    for (std::size_t i = 0u;
         i < 1000000u;
         ++i) {
        if (flag.load(
                std::memory_order_acquire))
            return true;
        std::this_thread::yield();
    }
    return false;
}

} // namespace

int main()
{
    // One set / one way forces erase(old) and publish(new) through the same
    // physical slot, making the writer interleaving deterministic.
    runtime::exact_material_cache<1u,1u> cache;

    constexpr std::uint16_t k_route_count = 16u;
    constexpr std::uint16_t k_old_route = 3u;
    constexpr std::uint16_t k_new_route = 7u;

    const auto *old_material =
        reinterpret_cast<const void *>(
            static_cast<std::uintptr_t>(
                0x1000u));
    const auto *new_material =
        reinterpret_cast<const void *>(
            static_cast<std::uintptr_t>(
                0x2000u));

    CHECK(cache.publish(
        old_material,
        k_old_route,
        k_route_count));

    std::uint16_t route = 0u;
    CHECK(cache.lookup(
        old_material,
        k_route_count,
        route));
    CHECK(route == k_old_route);

    writer_gate gate{};
    gate.old_key =
        reinterpret_cast<std::uintptr_t>(
            old_material);
    gate.new_key =
        reinterpret_cast<std::uintptr_t>(
            new_material);

    std::thread eraser([&] {
        (void)cache.erase(
            old_material,
            writer_hook,
            &gate);
    });

    CHECK(wait_until(gate.erase_claimed));

    // Erase owns the slot as BUSY. Readers must fail open rather than consume
    // either the old route or an in-flight route.
    route = 0xffffu;
    CHECK(!cache.lookup(
        old_material,
        k_route_count,
        route));

    std::thread publisher([&] {
        (void)cache.publish(
            new_material,
            k_new_route,
            k_route_count,
            writer_hook,
            &gate);
    });

    // Complete erase first. The publisher may claim the slot only after EMPTY
    // is published, so the eraser can no longer clear the new route.
    gate.allow_erase_commit.store(
        true,
        std::memory_order_release);
    eraser.join();

    CHECK(wait_until(gate.publish_claimed));

    // Publish now owns the same slot as BUSY; lookup(new) must still fail open
    // until route+key are committed as one writer generation.
    route = 0xffffu;
    CHECK(!cache.lookup(
        new_material,
        k_route_count,
        route));

    gate.allow_publish_commit.store(
        true,
        std::memory_order_release);
    publisher.join();

    route = 0u;
    CHECK(cache.lookup(
        new_material,
        k_route_count,
        route));
    CHECK(route == k_new_route);

    route = 0u;
    CHECK(!cache.lookup(
        old_material,
        k_route_count,
        route));

    // Pointer reuse regression: old and new MTD generations may occupy the
    // exact same engine material address. A delayed erase of the old
    // generation must not clear the route published for the new generation.
    cache.clear();

    const auto *reused_material =
        reinterpret_cast<const void *>(
            static_cast<std::uintptr_t>(
                0x3000u));

    CHECK(cache.publish(
        reused_material,
        k_old_route,
        k_route_count));

    writer_gate reuse_gate{};
    reuse_gate.old_key =
        reinterpret_cast<std::uintptr_t>(
            reused_material);
    reuse_gate.new_key =
        reuse_gate.old_key;

    std::thread reuse_eraser([&] {
        (void)cache.erase(
            reused_material,
            writer_hook,
            &reuse_gate);
    });

    CHECK(wait_until(
        reuse_gate.erase_claimed));

    std::thread reuse_publisher([&] {
        (void)cache.publish(
            reused_material,
            k_new_route,
            k_route_count,
            writer_hook,
            &reuse_gate);
    });

    // New publication cannot claim the same address while erase owns BUSY.
    CHECK(!reuse_gate.publish_claimed.load(
        std::memory_order_acquire));

    reuse_gate.allow_erase_commit.store(
        true,
        std::memory_order_release);
    reuse_eraser.join();

    CHECK(wait_until(
        reuse_gate.publish_claimed));

    route = 0xffffu;
    CHECK(!cache.lookup(
        reused_material,
        k_route_count,
        route));

    reuse_gate.allow_publish_commit.store(
        true,
        std::memory_order_release);
    reuse_publisher.join();

    route = 0u;
    CHECK(cache.lookup(
        reused_material,
        k_route_count,
        route));
    CHECK(route == k_new_route);

    std::cout
        << "exact material cache BUSY interleaving test passed\n";
    return 0;
}
