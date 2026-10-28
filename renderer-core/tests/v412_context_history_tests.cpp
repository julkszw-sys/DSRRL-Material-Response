#include "dsrrl/runtime/v411_owner_sequence_journal.hpp"
#include <cassert>
#include <atomic>
#include <thread>
#include <vector>
using namespace dsrrl::runtime::v411;

static witness make(std::uintptr_t owner,std::uint64_t seq,
                    std::uint64_t epoch=3) {
    witness w{};
    w.owner=owner;w.wrapper=7;w.manager=8;
    w.native_sequence=seq;w.selector_epoch=epoch;
    w.row_a=static_cast<std::uint32_t>(seq);
    return w;
}
int main() {
    journal j;
    const auto first=make(0x219A4D80,25);
    assert(j.observe(first));
    auto r=j.lookup(first.owner,25,3);
    assert(r.state==status::matched_snapshot && r.prior.row_a==25);
    assert(j.lookup(first.owner,3,3).state==status::producer_changed);
    assert(j.lookup(first.owner,25,4).state==status::epoch_changed);
    assert(j.lookup(first.owner,25,3,8,8).state==status::context_tuple_changed);
    assert(j.lookup(first.owner+1,25,3).state==status::no_record);
    assert(j.observe(make(first.owner,26)));
    r=j.lookup(first.owner,25,3);
    assert(r.state==status::matched_snapshot && r.superseded_by_newer);
    assert(r.prior.native_sequence==25);
    assert(j.lookup(first.owner,26,3).state==status::matched_snapshot);
    assert(j.observe(make(first.owner,30,4)));
    assert(j.lookup(first.owner,30,3).state==status::epoch_changed);
    // An owner reused with another wrapper cannot identify the same lifetime.
    auto changed=make(first.owner,31,4);
    changed.wrapper=19;changed.manager=20;
    assert(j.observe(changed));
    r=j.lookup(first.owner,25,3);
    assert(r.state==status::ambiguous_context_identity);
    assert(r.had_pointer_reassignment);
    assert(j.lookup(first.owner,31,4).state==status::matched_snapshot);
    // Same producer sequence/epoch with different wrapper is ambiguous.
    auto duplicate=make(first.owner,31,4);
    duplicate.wrapper=21;
    assert(j.observe(duplicate));
    assert(j.lookup(first.owner,31,4).state==status::ambiguous_context_identity);
    // A reused sequence+epoch but different source payload is also ambiguous.
    auto payload=duplicate;payload.source_a=0x1234;
    assert(j.observe(payload));
    assert(j.lookup(first.owner,31,4).state==status::ambiguous_context_identity);
    // Expired history is not proof of nonexistence in the native engine.
    for(int n=32;n<=43;++n)assert(j.observe(make(first.owner,n,4)));
    r=j.lookup(first.owner,25,3);
    assert(r.state==status::history_evicted && r.history_has_evictions);
    assert(j.history_overwrites()>0);
    // Same serial across distinct owner pointers never joins.
    assert(j.observe(make(0x219A2800,43,4)));
    assert(j.lookup(0x219A2800,25,3).state==status::producer_changed);
    // 8 parallel workers, 8000 operations; no source/owner cross-contamination.
    std::atomic<int> passes{0};
    std::vector<std::thread> threads;
    for(int id=0;id<8;++id)threads.emplace_back([&,id] {
        journal local;
        for(int n=0;n<1000;++n) {
            auto w=make(0x30000u+id,n+1,7);
            assert(local.observe(w));
            const auto q=local.lookup(w.owner,w.native_sequence,w.selector_epoch);
            if(q.state==status::matched_snapshot)++passes;
        }
        assert(local.capacity_failures()==0);
    });
    for(auto &thread:threads)thread.join();
    assert(passes==8000);
}
