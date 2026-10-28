#pragma once
// V411 observational journal: no source gating, no binary patch, no guessing.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
namespace dsrrl::runtime::v411 {
struct witness {
    std::uintptr_t owner=0, wrapper=0, manager=0;
    std::uintptr_t source_a=0, source_b=0;
    std::uint64_t native_sequence=0, selector_epoch=0;
    std::uint32_t row_a=0, row_b=0, beta_bits=0;
    std::int16_t selector_a=-1, selector_b=-1;
    std::uint32_t thread_id=0;
    std::uintptr_t context_vtable=0, context_manager=0;
    bool vtable_readable=false, manager_field_readable=false;
    bool context_manager_matches_wrapper=false;
};
enum class status:std::uint8_t {
    no_record, matched_snapshot, producer_changed, epoch_changed,
    context_tuple_changed, capacity_miss
};
struct lookup_result {
    status state=status::no_record;
    witness prior{};
    std::uint64_t observed_version=0;
    bool had_pointer_reassignment=false;
};
class journal {
    static constexpr std::size_t shard_count=64, slots_per_shard=64;
    struct entry {
        bool used=false;
        witness value{};
        std::uint64_t version=0;
        bool had_pointer_reassignment=false;
    };
    std::array<std::array<entry,slots_per_shard>,shard_count> data_{};
    std::array<std::mutex,shard_count> locks_{};
    std::atomic<std::uint64_t> capacity_failures_{0};
    static std::uint64_t hash(std::uintptr_t p) noexcept {
        std::uint64_t h=static_cast<std::uint64_t>(p);
        h^=h>>33;h*=0xff51afd7ed558ccdULL;
        h^=h>>33;h*=0xc4ceb9fe1a85ec53ULL;
        return h^(h>>33);
    }
public:
    bool observe(const witness &w) noexcept {
        if(w.owner==0 || w.wrapper==0 || w.manager==0 ||
           w.native_sequence==0 || w.selector_epoch==0) return false;
        const auto h=hash(w.owner);
        auto &mutex=locks_[(h>>32)&(shard_count-1)];
        auto &slots=data_[(h>>32)&(shard_count-1)];
        std::lock_guard<std::mutex> lock(mutex);
        for(std::size_t n=0;n<slots_per_shard;++n) {
            auto &s=slots[(h+n)&(slots_per_shard-1)];
            if(!s.used) {
                s.used=true;s.value=w;s.version=1;
                return true;
            }
            if(s.value.owner==w.owner) {
                if(s.value.wrapper!=w.wrapper || s.value.manager!=w.manager)
                    s.had_pointer_reassignment=true;
                s.value=w;++s.version;return true;
            }
        }
        capacity_failures_.fetch_add(1,std::memory_order_relaxed);
        return false;
    }
    lookup_result lookup(std::uintptr_t owner,std::uint64_t native_seq,
                         std::uint64_t selector_epoch,
                         std::uintptr_t observed_wrapper=0,
                         std::uintptr_t observed_manager=0) noexcept {
        if(owner==0)return {};
        const auto h=hash(owner);
        auto &mutex=locks_[(h>>32)&(shard_count-1)];
        auto &slots=data_[(h>>32)&(shard_count-1)];
        std::lock_guard<std::mutex> lock(mutex);
        for(std::size_t n=0;n<slots_per_shard;++n) {
            auto &s=slots[(h+n)&(slots_per_shard-1)];
            if(!s.used)return {};
            if(s.value.owner!=owner)continue;
            lookup_result out{};
            out.prior=s.value;out.observed_version=s.version;
            out.had_pointer_reassignment=s.had_pointer_reassignment;
            if(native_seq==0 || s.value.native_sequence!=native_seq)
                out.state=status::producer_changed;
            else if(selector_epoch==0 || s.value.selector_epoch!=selector_epoch)
                out.state=status::epoch_changed;
            else if((observed_wrapper && s.value.wrapper!=observed_wrapper) ||
                    (observed_manager && s.value.manager!=observed_manager))
                out.state=status::context_tuple_changed;
            else out.state=status::matched_snapshot;
            return out;
        }
        return {status::capacity_miss};
    }
    std::uint64_t capacity_failures() const noexcept {
        return capacity_failures_.load(std::memory_order_relaxed);
    }
};
inline journal &observations() noexcept {
    static journal instance;
    return instance;
}
}
