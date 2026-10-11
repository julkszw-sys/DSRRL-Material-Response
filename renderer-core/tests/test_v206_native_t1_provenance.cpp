#include "dsrrl/runtime/v206_native_t1_provenance.hpp"
#include <cstdio>
#include <cstring>
using dsrrl::runtime::v206_native_t1::registry;
#define CHECK(x) do { if(!(x)){ std::fprintf(stderr,"V206 FAIL line %d: %s\n",__LINE__,#x);return 1; } }while(false)
int main(){
    registry<32,8> r{};
    CHECK(!r.resolve(0x1010,0x2010));
    CHECK(!r.observe_writer(0x1010,0x2010,"BD_M_9380_1_s",13,1));
    const auto a=r.init(1,0x1010,0x2010);
    CHECK(a!=0);
    CHECK(!r.observe_writer(0x1010,0x2010,"c5330_s",7,1)); // NPC is forbidden
    CHECK(!r.observe_writer(0x1010,0x2010,"BD_M_9380_1_s",13,0));
    CHECK(!r.observe_writer(0x1010,0x2020,"BD_M_9380_1_s",13,1));
    CHECK(r.observe_writer(0x1010,0x2010,"BD_M_9380_1_s",13,1));
    auto w=r.resolve(0x1010,0x2010);
    CHECK(w.epoch==a && w.source_id==1);
    CHECK(std::strcmp(w.logical_name.data(),"BD_M_9380_1_s")==0);
    CHECK(!r.resolve(0x1010,0x2020));
    CHECK(r.init(1,0x1010,0x2010)==0);
    CHECK(!r.resolve(0x1010,0x2010)); // duplicate live view poisons
    r.destroy(1,0x1010);
    const auto b=r.init(1,0x1010,0x2020);
    CHECK(b>a && !r.resolve(0x1010,0x2020)); // reused view has no stale writer
    CHECK(r.observe_writer(0x1010,0x2020,"LG_M_9430_s",11,2));
    CHECK(r.resolve(0x1010,0x2020).epoch==b);
    r.destroy_device(1);
    CHECK(!r.resolve(0x1010,0x2020));
    CHECK(r.init(2,0x1010,0x2010)!=0);
    CHECK(r.observe_writer(0x1010,0x2010,"AM_M_9430_s",11,3));
    CHECK(r.resolve(0x1010,0x2010));
    CHECK(r.observe_writer(0x1010,0x2010,"AM_M_9430_s",11,4));
    CHECK(!r.resolve(0x1010,0x2010)); // ambiguous second writer
    // Exact capacity limits never authorize unknowns.
    registry<2,1> bounded{};
    CHECK(bounded.init(1,0x1100,0x2100));
    CHECK(bounded.observe_writer(0x1100,0x2100,"HD_M_9430_s",11,1));
    CHECK(!bounded.observe_writer(0x1100,0x2100,"HD_M_9430_s",11,2));
    CHECK(bounded.init(1,0x1101,0x2101));
    CHECK(!bounded.init(1,0x1102,0x2102));
    std::puts("V206_CPU_SRV_RESOURCE_EPOCH_POSITIVE_AND_NEGATIVE_PASS");
    return 0;
}
