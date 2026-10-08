#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime {

// Pure TLS slot-choice policy. No shared state, COM operations or locks.
// Caller must still check the key's current authoritative resource epoch.
template<std::size_t Slots, std::size_t Ways>
struct companion_tls_way_selector {
    static_assert(Slots != 0 && Ways != 0 && Slots % Ways == 0);
    static_assert((Slots & (Slots - 1u)) == 0u);
    static_assert((Ways & (Ways - 1u)) == 0u);
    static constexpr std::size_t sets = Slots / Ways;

    static constexpr std::size_t set(std::uint64_t key) noexcept {
        return static_cast<std::size_t>(
            ((key >> 4u) ^ (key >> 13u) ^ (key >> 23u)) & (sets - 1u));
    }

    template<class Entry>
    static std::size_t choose(
        const std::array<Entry, Slots> &entries,
        std::array<std::uint8_t, sets> &victims,
        std::uint64_t key) noexcept
    {
        const auto index = set(key);
        const auto base = index * Ways;
        // Reuse same key first even if its epoch is stale; caller will
        // revalidate against authoritative g_cache before publication.
        for (std::size_t way = 0; way < Ways; ++way)
            if (entries[base + way].key == key)
                return base + way;
        for (std::size_t way = 0; way < Ways; ++way)
            if (entries[base + way].key == 0u)
                return base + way;
        const auto way = static_cast<std::size_t>(
            victims[index]++ & static_cast<std::uint8_t>(Ways - 1u));
        return base + way;
    }
};

} // namespace dsrrl::runtime
