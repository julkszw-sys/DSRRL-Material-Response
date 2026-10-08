#include "dsrrl/runtime/resource_view_epoch.hpp"

#include <atomic>
#include <cstdint>
#include <thread>

// Exact lifecycle contract for companion TLS snapshots. No D3D requirement.
int main() {
    dsrrl::runtime::resource_view_epoch<256u> generations{};
    constexpr std::uint64_t a = 0x100000u;
    std::uint64_t b = a + 0x10u;
    for (; dsrrl::runtime::resource_view_epoch<256u>::index(a) ==
               dsrrl::runtime::resource_view_epoch<256u>::index(b); b += 0x10u) {}

    const auto av = generations.current(a);
    const auto bv = generations.current(b);
    generations.invalidate(a);
    if (generations.current(a) != av + 1u) return 1;
    if (generations.current(b) != bv) return 2; // Unrelated TLS remains valid.

    // A freed and re-created SRV reusing the same numeric pointer must not
    // reuse a cached binding from the prior lifetime (ABA prevention).
    const auto before_recreate = generations.current(a);
    generations.invalidate(a);
    if (generations.current(a) == before_recreate) return 3;

    // A bucket collision may conservatively invalidate another key; this
    // cannot preserve stale identity incorrectly.
    auto collision = a + 0x10u;
    for (; dsrrl::runtime::resource_view_epoch<256u>::index(a) !=
               dsrrl::runtime::resource_view_epoch<256u>::index(collision);
         collision += 0x10u) {}
    const auto collision_before = generations.current(collision);
    generations.invalidate(a);
    if (generations.current(collision) != collision_before + 1u) return 4;

    const auto a_before_reset = generations.current(a);
    const auto b_before_reset = generations.current(b);
    generations.invalidate_all();
    if (generations.current(a) != a_before_reset + 1u ||
        generations.current(b) != b_before_reset + 1u) return 5;

    // Concurrent invalidation must publish a monotonically advancing
    // version that all threads can observe using acquire loads.
    std::atomic<bool> ready{false};
    std::thread producer([&] {
        for (int i = 0; i != 10000; ++i)
            generations.invalidate(a);
        ready.store(true, std::memory_order_release);
    });
    while (!ready.load(std::memory_order_acquire))
        (void)generations.current(a);
    producer.join();
    if (generations.current(a) != a_before_reset + 1u + 10000u) return 6;
    if (generations.current(b) != b_before_reset + 1u) return 7;
    return 0;
}
