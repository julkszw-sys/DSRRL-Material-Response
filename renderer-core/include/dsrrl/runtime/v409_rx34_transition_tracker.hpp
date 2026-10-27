#pragma once
// V409: diagnostic-only, exact receiver-scoped GPU-issued transition tracker.
// No donor admission, shader, color, EXE or Source2 fallback modification.
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
namespace dsrrl::runtime::v409 {
struct identity {
    std::uintptr_t owner=0, shader=0;
    std::uint64_t semantic=0;
    std::uint32_t route=0, material_slot=0, receiver=0;
    std::array<std::uint8_t,32> flver{}, mtd{};
    bool operator==(const identity &b) const noexcept {
        return owner==b.owner && shader==b.shader &&
            semantic==b.semantic && route==b.route &&
            material_slot==b.material_slot && receiver==b.receiver &&
            flver==b.flver && mtd==b.mtd;
    }
};
struct source_snapshot {
    std::uint8_t kind=0;
    bool independent=false;
    std::uint64_t bank=0;
    std::uint32_t row=0;
    std::uint16_t probe=0;
    std::array<std::uint32_t,3> rgb_bits{};
    std::uint64_t serial=0, native_sequence=0;
    bool same_visual_source(const source_snapshot &b) const noexcept {
        // Native event identity and selector epoch are not image transitions.
        return kind==b.kind && independent==b.independent &&
            bank==b.bank && row==b.row && probe==b.probe &&
            rgb_bits==b.rgb_bits;
    }
};
enum class observation_kind : std::uint8_t { unchanged, first, transition, capacity_miss };
struct observation {
    observation_kind kind=observation_kind::unchanged;
    source_snapshot previous{};
    std::uint64_t sequence=0;
};
class source_transition_tracker {
    static constexpr std::size_t shards=64, per_shard=64;
    struct entry { bool occupied=false; identity key{}; source_snapshot source{}; };
    std::array<std::array<entry,per_shard>,shards> entries_{};
    std::array<std::mutex,shards> locks_{};
    std::atomic<std::uint64_t> events_{0}, misses_{0};
    static std::uint64_t hash_identity(const identity &v) noexcept {
        std::uint64_t h=14695981039346656037ULL;
        const auto add=[&h](const auto &x) noexcept {
            const auto *bytes=reinterpret_cast<const std::uint8_t *>(&x);
            for(std::size_t i=0;i<sizeof(x);++i)
                h=(h^bytes[i])*1099511628211ULL;
        };
        add(v.owner);add(v.shader);add(v.semantic);
        add(v.route);add(v.material_slot);add(v.receiver);
        for(auto byte:v.flver)h=(h^byte)*1099511628211ULL;
        for(auto byte:v.mtd)h=(h^byte)*1099511628211ULL;
        return h;
    }
public:
    observation observe(const identity &key,const source_snapshot &current) noexcept {
        const auto h=hash_identity(key);
        const auto shard=static_cast<std::size_t>((h>>32)&(shards-1));
        const auto start=static_cast<std::size_t>(h&(per_shard-1));
        std::lock_guard<std::mutex> guard(locks_[shard]);
        for(std::size_t i=0;i<per_shard;++i) {
            auto &e=entries_[shard][(start+i)&(per_shard-1)];
            if(!e.occupied) {
                e.key=key;e.source=current;e.occupied=true;
                return {observation_kind::first,{},
                    events_.fetch_add(1,std::memory_order_relaxed)+1};
            }
            if(e.key==key) {
                if(e.source.same_visual_source(current)) {
                    e.source=current;
                    return {};
                }
                const auto prior=e.source;
                e.source=current;
                return {observation_kind::transition,prior,
                    events_.fetch_add(1,std::memory_order_relaxed)+1};
            }
        }
        return {observation_kind::capacity_miss,{},
            misses_.fetch_add(1,std::memory_order_relaxed)+1};
    }
};
} // namespace dsrrl::runtime::v409
