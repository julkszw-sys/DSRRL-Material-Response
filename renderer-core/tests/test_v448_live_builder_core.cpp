#include "v448_live_builder_core.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
using namespace dsrrl::runtime;
using namespace dsrrl::runtime::v448;
static builder_witness builder(std::uintptr_t entity,std::uintptr_t frame,std::uintptr_t ctx,unsigned row){
    builder_witness b{};b.entity=entity;b.dp=entity+0x108;
    b.frame_rbp=frame;b.packet=frame-0x30;b.context=ctx;
    b.engine_model=entity+0x40;b.a=b.b=static_cast<std::uint16_t>(0x0500+row);
    b.beta_bits=0;b.builder_family=1;b.valid=true;return b;
}
static v447::source_ticket source(const builder_witness&b,std::uint64_t seq){
    v447::source_ticket s{};s.sequence=seq;s.entity=b.entity;s.drawparam=b.dp;
    s.packet=b.packet;s.context=b.context;s.model=b.engine_model;
    s.native_a=b.a;s.native_b=b.b;s.beta_bits=b.beta_bits;
    s.row_a=b.a&0xff;s.row_b=b.b&0xff;return s;
}
int main(){
    auto bright=builder(0x10000,0x900000,0xCCCC00,30);
    auto dark=builder(0x20000,0x910000,0xCCCC00,31);
    auto bs=source(bright,100),ds=source(dark,101);
    thread_transport tls;
    tls.packer(bs);tls.select(bright,0x55550,1);tls.consume(bright.context);
    auto select=tls.take(bright.context);
    assert(select.valid&&select.sequence==100&&select.builder.entity==bright.entity);
    issue_witness issue{bright,bright.context,100,30};
    assert(evaluate(bs,select,issue).state==verdict::live_builder_chain_only_generation_open);
    assert(!tls.take(bright.context).valid); // one-shot
    auto mutate=[&](auto f,verdict v){auto p=bs;auto s=select;auto i=issue;f(p,s,i);assert(evaluate(p,s,i).state==v);};
    mutate([](auto &,auto &s,auto &){s.valid=false;},verdict::no_selector);
    mutate([](auto &,auto &,auto &i){i.builder.valid=false;},verdict::no_issued_builder);
    mutate([](auto &p,auto &,auto &){p.sequence=0;},verdict::missing_packer);
    mutate([](auto &,auto &s,auto &){s.sequence++;},verdict::source_seq_mismatch);
    mutate([](auto &p,auto &,auto &){p.context++;},verdict::context_mismatch);
    mutate([](auto &p,auto &,auto &){p.entity=0x80000;},verdict::source_actor_mismatch);
    mutate([](auto &p,auto &,auto &){p.packet++;},verdict::source_packet_mismatch);
    mutate([](auto &p,auto &,auto &){p.model++;},verdict::source_model_mismatch);
    mutate([](auto &p,auto &,auto &){p.native_a++;},verdict::source_ab_mismatch);
    mutate([](auto &,auto &,auto &i){i.builder.frame_rbp++;},verdict::selector_issue_mismatch);
    mutate([](auto &,auto &,auto &i){i.issued_row++;},verdict::row_mismatch);
    // Shared context does not merge two actor identities; this is the root
    // failure mode targeted by the producer-to-receiver certificate.
    auto swapped=issue;swapped.builder=dark;swapped.native_sequence=101;swapped.issued_row=31;
    assert(evaluate(bs,select,swapped).state==verdict::source_seq_mismatch);
    swapped.native_sequence=100;
    assert(evaluate(bs,select,swapped).state==verdict::source_packet_mismatch);
    swapped.builder.packet=bright.packet;swapped.builder.a=bright.a;swapped.builder.b=bright.b;swapped.issued_row=30;
    assert(evaluate(bs,select,swapped).state==verdict::selector_issue_mismatch);
    tls.packer(bs);tls.select(bright,0x55550,1);tls.consume(bright.context);
    assert(!tls.take(0xDEADDC).valid);
    tls.packer(bs);tls.select(bright,0x55550,1);
    tls.packer(ds);tls.select(dark,0x77770,0);tls.consume(dark.context);
    auto sd=tls.take(dark.context);
    assert(sd.valid&&sd.sequence==101);
    assert(evaluate(ds,sd,issue_witness{dark,dark.context,101,31}).state==verdict::live_builder_chain_only_generation_open);
    std::vector<std::thread> threads;
    for(int j=0;j<8;++j)threads.emplace_back([j]{
        auto b=builder(0x200000+j*0x1000,0x600000+j*0x2000,0xB00000,30+j%2);
        auto p=source(b,1000+j);
        for(int i=0;i<1000;++i){thread_transport t;t.packer(p);t.select(b,0x50000+j,0);t.consume(b.context);
          auto s=t.take(b.context);assert(s.valid);auto result=evaluate(p,s,issue_witness{b,b.context,p.sequence,p.row_a});
          assert(result.state==verdict::live_builder_chain_only_generation_open);}
    });
    for(auto&t:threads)t.join();
    std::cout<<"V448 core 16 negative/identity scenarios + 8000 threaded cases PASS; actor lifetime OPEN, pixel OPEN\n";
}
