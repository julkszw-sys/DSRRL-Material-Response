#include "dsrrl/runtime/v411_owner_sequence_journal.hpp"
#include <cassert>
#include <atomic>
#include <thread>
#include <vector>
using namespace dsrrl::runtime::v411;
int main() {
    journal j;
    witness a{};a.owner=0x219A4D80;a.wrapper=7;a.manager=8;
    a.native_sequence=25;a.selector_epoch=3;a.row_a=25;
    assert(j.observe(a));
    auto result=j.lookup(a.owner,25,3);
    assert(result.state==status::matched_snapshot && result.prior.row_a==25);
    assert(j.lookup(a.owner,3,3).state==status::producer_changed);
    assert(j.lookup(a.owner,25,4).state==status::epoch_changed);
    assert(j.lookup(a.owner,25,3,8,8).state==status::context_tuple_changed);
    assert(j.lookup(a.owner+1,25,3).state==status::no_record);
    a.wrapper=12;a.native_sequence=26;assert(j.observe(a));
    result=j.lookup(a.owner,26,3);
    assert(result.had_pointer_reassignment && result.observed_version==2);
    std::atomic<int> passes{0};std::vector<std::thread> threads;
    for(int tid=0;tid<8;++tid) {
        threads.emplace_back([&,tid]{
            witness w{};w.owner=0x10000u+tid;w.wrapper=1;w.manager=2;
            w.selector_epoch=9;
            for(int n=0;n<1000;++n) {
                w.native_sequence=n+1;
                assert(j.observe(w));
                auto q=j.lookup(w.owner,w.native_sequence,w.selector_epoch);
                if(q.state==status::matched_snapshot)++passes;
            }
        });
    }
    for(auto &t:threads){t.join();}
    assert(passes==8000);assert(j.capacity_failures()==0);
}
