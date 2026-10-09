#include "dsrrl/runtime/pmetal_map_source.hpp"
#include <cassert>
#include <thread>
#include <atomic>
#include <iostream>
using namespace dsrrl::runtime;
using namespace dsrrl::runtime::pmetal_map_source;
int main() {
    static_assert(probe_area(0)==10 && probe_area(86)==10);
    static_assert(probe_area(87)==11 && probe_area(332)==17);
    static_assert(probe_area(333)==18 && probe_area(341)==18);
    static_assert(probe_area(342)==0);
    material m{};
    m.valid=true;m.owner_tuple_exact=true;
    m.material_slot_valid=true;m.actual_material_exact=false; // RC1 verified FLVER owner is sufficient
    m.material_slot=1;m.route_index=345;
    m.flver_sha256[0]=0x67;
    m.raw_mtd_sha256[0]=0xec;
    material other=m;
    other.flver_sha256[31]=1;
    pmetal_envspec_source m10{},m18{},check{};
    m10.bank_signature_a=0x4c594553d201d80cULL;
    m10.bank_signature_b=m10.bank_signature_a;
    m10.row_id_a=0;
    m18.bank_signature_a=0x1ecfd1e617c59071ULL;
    m18.bank_signature_b=m18.bank_signature_a;
    m18.row_id_a=32;
    assert(!publish(m,m10,18));
    assert(!publish(m,m18,10));
    assert(publish(m,m10,10));
    assert(publish(m,m18,18));
    assert(latest(m,18,check) && check.bank_signature_a==m18.bank_signature_a);
    assert(latest(m,10,check) && check.bank_signature_a==m10.bank_signature_a);
    assert(!latest(other,18,check));
    // Switching light banks in one shader family must not overwrite other area.
    for(int i=0;i<100;++i) {
        m10.row_id_a=static_cast<std::uint32_t>(i%64);
        assert(publish(m,m10,10));
        assert(latest(m,18,check) && check.row_id_a==32);
    }
    m18.beta=0.25f;m18.bank_signature_b=m10.bank_signature_a;
    assert(!publish(m,m18,18)); // never admit cross-area blend
    m18.beta=0.0f;m18.bank_signature_b=m18.bank_signature_a;
    std::atomic_bool ok{true};
    std::thread writer([&] {
        for(int i=0;i<1000;++i) {
            auto v=m18;v.row_id_a=static_cast<std::uint32_t>(i%64);
            if(!publish(m,v,18))ok=false;
        }
    });
    std::thread reader([&] {
        for(int i=0;i<1000;++i) {
            pmetal_envspec_source v{};
            if(!latest(m,18,v) || bank_area(v.bank_signature_a)!=18)ok=false;
        }
    });
    writer.join();reader.join();
    assert(ok.load());
    // Whole-bank DSR source is valid *inside* the PTDE bridge. It must
    // remain separate from m10/m18 map records, and a later m18 update
    // must supersede it when the native selector changes back.
    pmetal_envspec_source common99{};
    common99.bank_signature_a=0x34bdd6493ca1a91aULL;
    common99.bank_signature_b=common99.bank_signature_a;
    assert(publish(m,common99,99));
    unsigned chosen=0u;
    assert(latest_for_probe(m,18,check,chosen) && chosen==99u);
    assert(check.bank_signature_a==common99.bank_signature_a);
    auto next18=m18;next18.beta=0.0f;
    assert(publish(m,next18,18));
    assert(latest_for_probe(m,18,check,chosen) && chosen==18u);
    assert(check.bank_signature_a==next18.bank_signature_a);
    pmetal_envspec_source native_default{};
    native_default.bank_signature_a=0x96ece3bed03eed01ULL;
    native_default.bank_signature_b=native_default.bank_signature_a;
    assert(publish(m,native_default,100));
    assert(latest_for_probe(m,18,check,chosen) && chosen==100u);
    assert(check.bank_signature_a==native_default.bank_signature_a);
    assert(publish(m,m10,10));
    assert(latest_for_probe(m,18,check,chosen) && chosen==100u);
    assert(!latest_for_probe(other,18,check,chosen));
    auto cross_common=native_default;
    cross_common.beta=0.5f;
    cross_common.bank_signature_b=next18.bank_signature_a;
    assert(!publish(m,cross_common,100));
    assert(!publish(m,native_default,18));
    invalidate();
    assert(!latest_for_probe(m,18,check,chosen));
    assert(!latest(m,18,check)); // no stale bank across source generation
    assert(!latest(m,10,check));
    std::cout<<"PASS exact material+map source isolation, cross-area blend refusal, concurrency, reset\n";
}
