#include "dsrrl/runtime/v411_owner_sequence_journal.hpp"
#include <cassert>
#include <atomic>
#include <thread>
#include <vector>
using namespace dsrrl::runtime::v411;
static witness item(std::uintptr_t owner,std::uint64_t seq,
                    std::uintptr_t wrapper=10) {
    witness w{};w.owner=owner;w.wrapper=wrapper;w.manager=wrapper+1;
    w.native_sequence=seq;w.selector_epoch=7;
    w.row_a=static_cast<std::uint32_t>(seq);return w;
}
int main(){
    journal j;
    auto a=item(0x1010,10),b=item(0x1010,11),
         c=item(0x1010,12,20),d=item(0x1010,13,20);
    assert(j.observe(a)&&j.observe(b)&&j.observe(c)&&j.observe(d));
    auto old=j.lookup(a.owner,10,7,10,11);
    assert(old.state==status::ambiguous_context_identity);
    assert(old.matched_context_run==1 && old.latest_context_run==2);
    auto now=j.lookup(a.owner,12,7,20,21);
    assert(now.state==status::matched_snapshot && now.superseded_by_newer);
    assert(now.matched_context_run==2 && now.latest_context_run==2);
    assert(j.lookup(a.owner,12,7,10,11).state==status::context_tuple_changed);
    auto x=item(0x2020,20),y=item(0x2020,20,100);
    assert(j.observe(x)&&j.observe(y));
    assert(j.lookup(x.owner,20,7).state==status::ambiguous_context_identity);
    assert(j.lookup(0,20,7).state==status::no_record);
    std::atomic<int> done{0};std::vector<std::thread> threads;
    for(int i=0;i<8;++i)threads.emplace_back([&,i]{
        journal local;auto w=item(0x9000+i,2);
        for(int n=0;n<1000;++n) {
            w.native_sequence=n+1;assert(local.observe(w));
            auto r=local.lookup(w.owner,w.native_sequence,
                w.selector_epoch,w.wrapper,w.manager);
            assert(r.state==status::matched_snapshot);
            ++done;
        }
    });
    for(auto &thread:threads){thread.join();}
    assert(done==8000);
}
