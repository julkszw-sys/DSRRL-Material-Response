#include "dsrrl/runtime/v447_actor_source_router.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
using namespace dsrrl::runtime::v447;
static source_ticket src(u64 seq,addr entity,addr context,std::uint16_t selector){
    source_ticket t{};
    t.sequence=seq;t.entity=entity;t.drawparam=entity+0x108;
    t.packet=entity+0x800;t.context=context;t.model=entity+0x400;
    t.native_a=selector;t.native_b=selector;t.row_a=selector&0xff;
    t.row_b=selector&0xff;return t;
}
static receiver_ticket recv(const source_ticket&t){
    receiver_ticket r{};r.native_sequence=t.sequence;r.context=t.context;
    r.entity=t.entity;r.drawparam=t.drawparam;r.packet=t.packet;
    r.model=t.model;r.row_a=t.row_a;r.actor_generation=t.actor_generation;
    return r;
}
int main(){
    registry a;
    auto bright=src(100,0x10000,0x90000,0x051e);
    auto dark=src(101,0x20000,0x90000,0x051f);
    assert(a.observe(bright)==verdict::exact_actor_packet);
    assert(a.observe(dark)==verdict::exact_actor_packet);
    assert(a.observe(bright)==verdict::exact_actor_packet);
    // Shared FrpgDrawContext absolutely cannot certify the actor.
    auto b=recv(bright);auto d=recv(dark);
    assert(a.resolve(b).state==verdict::missing_generation);
    assert(a.resolve(d).state==verdict::missing_generation);
    auto issued=receiver_ticket{};issued.native_sequence=100;
    issued.context=0x90000;issued.row_a=30;
    assert(a.resolve(issued).state==verdict::missing_receiver_identity);
    issued.native_sequence=101;issued.row_a=31;
    assert(a.resolve(issued).producer.entity==dark.entity);
    assert(a.resolve(issued).state==verdict::missing_receiver_identity);
    // Deliberately selecting the dark row for the bright actor is not allowed.
    auto swapped=recv(bright);swapped.native_sequence=101;swapped.row_a=31;
    assert(a.resolve(swapped).state==verdict::actor_mismatch);
    auto badctx=recv(bright);badctx.context=0x91000;
    assert(a.resolve(badctx).state==verdict::context_mismatch);
    auto badrow=recv(bright);badrow.row_a=31;
    assert(a.resolve(badrow).state==verdict::row_mismatch);
    // A single producer sequence cannot attest two different actors.
    auto same_seq_other=dark;same_seq_other.sequence=100;
    assert(a.observe(same_seq_other)==verdict::serial_conflict);
    assert(a.resolve(b).state==verdict::serial_conflict);
    // Generational ABA and shared context are independently checked.
    registry generational;
    bright.actor_generation=7;
    assert(generational.observe(bright)==verdict::exact_actor_packet);
    auto old=recv(bright);old.actor_generation=8;
    assert(generational.resolve(old).state==verdict::generation_mismatch);
    auto good=recv(bright);assert(generational.resolve(good).state==verdict::exact_actor_packet);
    good.packet++;
    assert(generational.resolve(good).state==verdict::packet_mismatch);
    good=recv(bright);good.model++;
    assert(generational.resolve(good).state==verdict::model_mismatch);
    auto malformed=bright;malformed.drawparam++;
    assert(generational.observe(malformed)==verdict::malformed_source);
    // Serial ring eviction must fail closed, not return stale source.
    auto newer=src(100+8192u,0x30000,0x90000,0x051e);
    assert(generational.observe(newer)==verdict::exact_actor_packet);
    assert(generational.resolve(recv(bright)).state==verdict::absent);
    // Concurrent publishers with distinct serials must not share source identity.
    registry parallel;
    std::vector<std::thread> workers;
    for(unsigned i=0;i<8;++i)workers.emplace_back([&parallel,i]{
        for(unsigned j=0;j<100;++j){
            const auto t=src(2000+i*100+j,0x100000+(i*100+j)*0x1000,
                             0xB0000,static_cast<std::uint16_t>(0x0500+(j%100)));
            assert(parallel.observe(t)==verdict::exact_actor_packet);
        }
    });
    for(auto &w:workers)w.join();
    for(unsigned i=0;i<8;++i)for(unsigned j=0;j<100;++j){
        const auto t=src(2000+i*100+j,0x100000+(i*100+j)*0x1000,
                         0xB0000,static_cast<std::uint16_t>(0x0500+(j%100)));
        assert(parallel.resolve(recv(t)).state==verdict::missing_generation);
    }
    std::cout<<"V447 actor-specific source ledger: 14 categories + 800 concurrent cases PASS; NO runtime/pixel claim\n";
}
