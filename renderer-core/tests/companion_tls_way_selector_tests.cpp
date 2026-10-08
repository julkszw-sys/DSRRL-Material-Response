#include "dsrrl/runtime/companion_tls_way_selector.hpp"
#include "dsrrl/runtime/resource_view_epoch.hpp"
#include <array>
#include <cstdint>
#include <thread>

struct Entry {
    std::uint64_t key = 0u;
    std::uint64_t epoch = 0u;
};

int main()
{
    using Ways = dsrrl::runtime::companion_tls_way_selector<256u,4u>;
    static_assert(Ways::sets == 64u);
    std::array<Entry,256u> entries{};
    std::array<std::uint8_t,Ways::sets> victims{};
    dsrrl::runtime::resource_view_epoch<256u> epochs{};
    std::array<std::uint64_t,5u> keys{};
    keys[0] = 0x1000u;
    std::size_t found = 1u;
    for (std::uint64_t key = 0x1010u; found < keys.size() && key < 0x100000u; key += 16u)
        if (Ways::set(key) == Ways::set(keys[0]))
            keys[found++] = key;
    if (found != keys.size()) return 1;

    std::array<std::size_t,4u> original{};
    for (std::size_t i=0u; i<4u; ++i) {
        original[i] = Ways::choose(entries,victims,keys[i]);
        if (entries[original[i]].key != 0u) return 2;
        entries[original[i]] = {keys[i], epochs.current(keys[i])};
    }
    for (std::size_t i=0u; i<4u; ++i) {
        if (Ways::choose(entries,victims,keys[i]) != original[i]) return 3;
        if (entries[original[i]].key != keys[i]) return 4;
    }

    epochs.invalidate(keys[0]);
    if (Ways::choose(entries,victims,keys[0]) != original[0]) return 5;
    if (entries[original[0]].epoch == epochs.current(keys[0])) return 6;
    entries[original[0]].epoch = epochs.current(keys[0]);

    const auto evicted = Ways::choose(entries,victims,keys[4]);
    bool found_old=false;
    for (auto slot:original) if (slot==evicted) found_old=true;
    if (!found_old) return 7;
    entries[evicted] = {keys[4],epochs.current(keys[4])};
    if (Ways::choose(entries,victims,keys[4]) != evicted) return 8;

    // The policy is strictly per-thread: no mutable cache-line hand-off
    // between parsing/Present threads.
    std::array<int,4u> failures{};
    std::array<std::thread,4u> workers;
    for (std::size_t tid=0; tid<workers.size(); ++tid)
        workers[tid] = std::thread([tid,&failures]() {
            std::array<Entry,256u> local{};
            std::array<std::uint8_t,Ways::sets> local_victims{};
            for (std::uint64_t i=1u; i<=20000u; ++i) {
                const auto key = (i % 400u + 1u)*16u;
                const auto slot = Ways::choose(local,local_victims,key);
                local[slot].key=key;
                if (Ways::choose(local,local_victims,key) != slot) {
                    failures[tid] = 9;
                    return;
                }
            }
        });
    for (auto &worker:workers) worker.join();
    for (const auto failure:failures) if (failure != 0) return failure;
    return 0;
}
