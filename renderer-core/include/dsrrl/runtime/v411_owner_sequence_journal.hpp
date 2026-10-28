#pragma once
// V411 observational journal: no source gating, no binary patch, no guessing.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <memory>
#include <new>
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
    context_tuple_changed, capacity_miss, history_evicted, ambiguous_context_identity,
    context_unreadable, native_manager_disagreement
};
struct lookup_result {
    status state=status::no_record;
    witness prior{};
    std::uint64_t observed_version=0;
    bool had_pointer_reassignment=false;
    bool superseded_by_newer=false;
    bool history_has_evictions=false;
    std::uint8_t candidates=0;
    std::uint64_t matched_context_run=0;
    std::uint64_t latest_context_run=0;
};
class journal {
    static constexpr std::size_t shard_count=64, slots_per_shard=64;
    static constexpr std::size_t history_depth=8;
    struct entry {
        bool used=false;
        std::uintptr_t owner=0;
        std::array<witness,history_depth> history{};
        std::array<std::uint64_t,history_depth> context_runs{};
        std::uint64_t current_context_run=0;
        std::uint64_t version=0;
        bool had_pointer_reassignment=false;
        bool history_evicted=false;
    };
    // Bounded history needs several MiB; never allocate it on the Windows thread stack.
    // An allocation failure means missing diagnostic data, NOT a source-selector veto.
    std::unique_ptr<entry[]> data_;
    std::array<std::mutex,shard_count> locks_{};
    std::atomic<std::uint64_t> capacity_failures_{0};
    std::atomic<std::uint64_t> history_overwrites_{0};
    static std::uint64_t hash(std::uintptr_t p) noexcept {
        std::uint64_t h=static_cast<std::uint64_t>(p);
        h^=h>>33;h*=0xff51afd7ed558ccdULL;
        h^=h>>33;h*=0xc4ceb9fe1a85ec53ULL;
        return h^(h>>33);
    }
    static bool same_context_tuple(const witness &a,const witness &b) noexcept {
        return a.wrapper==b.wrapper && a.manager==b.manager &&
            a.context_vtable==b.context_vtable &&
            a.context_manager==b.context_manager;
    }
    static bool same_source_tuple(const witness &a,const witness &b) noexcept {
        return a.source_a==b.source_a && a.source_b==b.source_b &&
            a.selector_a==b.selector_a && a.selector_b==b.selector_b &&
            a.row_a==b.row_a && a.row_b==b.row_b &&
            a.beta_bits==b.beta_bits;
    }
    static bool same_witness_semantics(const witness &a,const witness &b) noexcept {
        return a.owner==b.owner && a.native_sequence==b.native_sequence &&
            a.selector_epoch==b.selector_epoch &&
            same_context_tuple(a,b) && same_source_tuple(a,b) &&
            a.vtable_readable==b.vtable_readable &&
            a.manager_field_readable==b.manager_field_readable &&
            a.context_manager_matches_wrapper==b.context_manager_matches_wrapper;
    }
    static lookup_result resolve(const entry &e,std::uint64_t seq,
        std::uint64_t epoch,std::uintptr_t wrapper,
        std::uintptr_t manager) noexcept {
        lookup_result r{};
        r.observed_version=e.version;
        r.latest_context_run=e.current_context_run;
        r.had_pointer_reassignment=e.had_pointer_reassignment;
        r.history_has_evictions=e.history_evicted;
        const auto length=static_cast<std::size_t>(
            e.version<history_depth ? e.version : history_depth);
        const witness *matching=nullptr;
        std::uint64_t matching_version=0;
        std::uint64_t matching_run=0;
        bool matching_seq=false;
        bool conflicting_tuple=false;
        for(std::size_t index=0;index<length;++index) {
            const auto version=e.version-index;
            const auto &w=e.history[(version-1)%history_depth];
            if(w.native_sequence!=seq)continue;
            matching_seq=true;
            if(w.selector_epoch!=epoch)continue;
            if(!matching) {
                matching=&w;matching_version=version;
                matching_run=e.context_runs[(version-1)%history_depth];
            } else if(!same_context_tuple(*matching,w) ||
                      !same_source_tuple(*matching,w)) {
                // A repeated native sequence+epoch cannot certify two
                // different endpoint payloads; fail observationally.
                conflicting_tuple=true;
            }
            if(r.candidates<255)++r.candidates;
        }
        if(seq==0 || epoch==0) {
            r.state=status::producer_changed;
        } else if(matching) {
            r.prior=*matching;
            r.matched_context_run=matching_run;
            r.superseded_by_newer=(matching_version!=e.version);
            if(conflicting_tuple ||
                matching_run!=e.current_context_run) {
                // A reused owner address is not a native actor lifetime ID.
                r.state=status::ambiguous_context_identity;
            } else if((wrapper && matching->wrapper!=wrapper) ||
                      (manager && matching->manager!=manager)) {
                r.state=status::context_tuple_changed;
            } else {
                r.state=status::matched_snapshot;
            }
        } else if(matching_seq) {
            r.state=status::epoch_changed;
        } else if(e.history_evicted) {
            r.state=status::history_evicted;
        } else {
            r.state=status::producer_changed;
        }
        if(!matching && e.version>0)
            r.prior=e.history[(e.version-1)%history_depth];
        return r;
    }
public:
    journal() noexcept : data_(new (std::nothrow) entry[shard_count*slots_per_shard]{}) {}
    journal(const journal&)=delete;
    journal& operator=(const journal&)=delete;
    bool observe(const witness &w) noexcept {
        if(!data_ || !w.owner || !w.wrapper || !w.manager ||
           !w.native_sequence || !w.selector_epoch) return false;
        const auto h=hash(w.owner);
        auto &mutex=locks_[(h>>32)&(shard_count-1)];
        auto *slots=data_.get()+((h>>32)&(shard_count-1))*slots_per_shard;
        std::lock_guard<std::mutex> lock(mutex);
        for(std::size_t n=0;n<slots_per_shard;++n) {
            auto &e=slots[(h+n)&(slots_per_shard-1)];
            if(!e.used) {
                e.used=true;e.owner=w.owner;e.version=1;
                e.current_context_run=1;
                e.history[0]=w;e.context_runs[0]=1;
                return true;
            }
            if(e.owner!=w.owner)continue;
            const auto &latest=e.history[(e.version-1)%history_depth];
            // Repeated identical native producer stamps do not evict real history.
            if(same_witness_semantics(latest,w))return true;
            if(!same_context_tuple(latest,w)) {
                e.had_pointer_reassignment=true;
                ++e.current_context_run;
            }
            if(e.version>=history_depth) {
                e.history_evicted=true;
                history_overwrites_.fetch_add(1,std::memory_order_relaxed);
            }
            ++e.version;
            e.history[(e.version-1)%history_depth]=w;
            e.context_runs[(e.version-1)%history_depth]=e.current_context_run;
            return true;
        }
        capacity_failures_.fetch_add(1,std::memory_order_relaxed);
        return false;
    }
    lookup_result lookup(std::uintptr_t owner,std::uint64_t native_seq,
        std::uint64_t selector_epoch,std::uintptr_t observed_wrapper=0,
        std::uintptr_t observed_manager=0) noexcept {
        if(!owner)return {};
        if(!data_)return {status::capacity_miss};
        const auto h=hash(owner);
        auto &mutex=locks_[(h>>32)&(shard_count-1)];
        auto *slots=data_.get()+((h>>32)&(shard_count-1))*slots_per_shard;
        std::lock_guard<std::mutex> lock(mutex);
        for(std::size_t n=0;n<slots_per_shard;++n) {
            auto &e=slots[(h+n)&(slots_per_shard-1)];
            if(!e.used)return {};
            if(e.owner==owner)
                return resolve(e,native_seq,selector_epoch,
                    observed_wrapper,observed_manager);
        }
        return {status::capacity_miss};
    }
    std::uint64_t capacity_failures() const noexcept {
        return capacity_failures_.load(std::memory_order_relaxed);
    }
    std::uint64_t history_overwrites() const noexcept {
        return history_overwrites_.load(std::memory_order_relaxed);
    }
};
inline journal &observations() noexcept {
    static journal instance;
    return instance;
}
}
