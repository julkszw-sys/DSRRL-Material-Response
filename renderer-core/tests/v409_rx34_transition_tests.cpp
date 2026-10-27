#include "dsrrl/runtime/v409_rx34_transition_tracker.hpp"
#include <cassert>
#include <atomic>
#include <thread>
#include <vector>
using namespace dsrrl::runtime::v409;
int main() {
 source_transition_tracker t;
 identity k{}; k.owner=123; k.receiver=34; k.route=345;
 source_snapshot a{};a.kind=2;a.row=30;a.native_sequence=10;
 assert(t.observe(k,a).kind==observation_kind::first);
 a.native_sequence++;
 assert(t.observe(k,a).kind==observation_kind::unchanged);
 a.kind=1;a.row=25;
 auto r=t.observe(k,a);
 assert(r.kind==observation_kind::transition&&r.previous.row==30);
 auto k2=k;k2.owner++;
 assert(t.observe(k2,a).kind==observation_kind::first);
 std::atomic<int> count{0};std::vector<std::thread> threads;
 for(int i=0;i<8;++i)threads.emplace_back([&,i](){
   auto key=k;key.owner=1000+i;
   for(int j=0;j<1000;++j){
     auto v=a;v.row=25u+(j%2);
     if(t.observe(key,v).kind==observation_kind::transition)++count;
   }
 });
 for(auto &w:threads) { w.join(); }
 assert(count==8*999);
}
