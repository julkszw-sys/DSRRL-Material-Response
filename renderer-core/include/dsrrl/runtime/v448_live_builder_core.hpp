#pragma once
// V448: exact LIVE builder transaction witness. Never a producer/actor lifetime claim.
// All structs are read-only. No row selection or shader/CB mutation.
#include <cstdint>
#include <cstddef>
#include "v447_actor_source_router.hpp"

namespace dsrrl::runtime::v448 {
using u64=std::uint64_t;
using addr=std::uintptr_t;
struct builder_witness {
    addr entity=0,dp=0,packet=0,context=0,engine_model=0,frame_rbp=0;
    std::uint16_t a=0,b=0;
    std::uint32_t beta_bits=0;
    std::uint32_t builder_family=0;  // 1 = FrpgModelDrawEntity default only
    bool valid=false;
};
struct selector_witness {
    builder_witness builder{};
    addr selector_flver_model=0;
    std::int32_t material_index=-1;
    u64 sequence=0;
    bool valid=false;
};
struct issue_witness {
    builder_witness builder{};
    addr context=0;
    u64 native_sequence=0;
    std::uint32_t issued_row=0;
};
enum class verdict : std::uint32_t {
    no_selector=0, no_issued_builder=1, missing_packer=2,
    source_seq_mismatch=3, context_mismatch=4, source_actor_mismatch=5,
    source_packet_mismatch=6, source_model_mismatch=7,
    source_ab_mismatch=8, selector_issue_mismatch=9, row_mismatch=10,
    live_builder_chain_only_generation_open=11
};
struct decision { verdict state=verdict::no_selector; u64 sequence=0; addr entity=0,packet=0; };
inline bool same_builder(const builder_witness&a,const builder_witness&b) noexcept {
    return a.valid && b.valid && a.builder_family==1 && b.builder_family==1 &&
        a.entity==b.entity && a.dp==b.dp && a.packet==b.packet &&
        a.context==b.context && a.engine_model==b.engine_model &&
        a.frame_rbp==b.frame_rbp && a.a==b.a && a.b==b.b &&
        a.beta_bits==b.beta_bits;
}
inline decision evaluate(const v447::source_ticket &p,
                         const selector_witness &s,const issue_witness &i) noexcept {
    decision d{}; d.sequence=i.native_sequence;
    if(!s.valid || !s.builder.valid)return d;
    d.entity=s.builder.entity;d.packet=s.builder.packet;
    if(!i.builder.valid){d.state=verdict::no_issued_builder;return d;}
    if(!p.sequence){d.state=verdict::missing_packer;return d;}
    if(p.sequence!=i.native_sequence || s.sequence!=i.native_sequence){d.state=verdict::source_seq_mismatch;return d;}
    if(p.context!=i.context||s.builder.context!=i.context||i.builder.context!=i.context){d.state=verdict::context_mismatch;return d;}
    if(p.entity!=s.builder.entity||p.drawparam!=s.builder.dp){d.state=verdict::source_actor_mismatch;return d;}
    if(p.packet!=s.builder.packet || p.packet!=i.builder.packet){d.state=verdict::source_packet_mismatch;return d;}
    if(p.model!=s.builder.engine_model){d.state=verdict::source_model_mismatch;return d;}
    if(p.native_a!=s.builder.a||p.native_b!=s.builder.b||p.beta_bits!=s.builder.beta_bits ||
       p.native_a!=i.builder.a||p.native_b!=i.builder.b||p.beta_bits!=i.builder.beta_bits){d.state=verdict::source_ab_mismatch;return d;}
    if(!same_builder(s.builder,i.builder)){d.state=verdict::selector_issue_mismatch;return d;}
    if(p.row_a!=i.issued_row || p.row_a!=(i.builder.a&0xffu)){
        d.state=verdict::row_mismatch;return d;
    }
    // LIVE stack evidence, not a constructor/destructor actor-generation token.
    // It cannot authorize runtime bridge activation before game evidence.
    d.state=verdict::live_builder_chain_only_generation_open;return d;
}
struct thread_transport {
    v447::source_ticket producer{};
    selector_witness selector{};
    selector_witness consumed{};
    bool producer_valid=false;
    void packer(const v447::source_ticket &p) noexcept {
        producer=p;producer_valid=v447::source_well_formed(p);
        // A new producer can precede a model select; it never turns an older
        // selector into a new owner.
        selector={};consumed={};
    }
    void select(const builder_witness&b,addr flver_model,std::int32_t index) noexcept {
        consumed={};selector={};
        if(!producer_valid || !b.valid || b.builder_family!=1)return;
        if(producer.entity!=b.entity || producer.drawparam!=b.dp ||
           producer.packet!=b.packet || producer.context!=b.context ||
           producer.model!=b.engine_model || producer.native_a!=b.a ||
           producer.native_b!=b.b || producer.beta_bits!=b.beta_bits)return;
        selector={b,flver_model,index,producer.sequence,true};
    }
    void consume(addr context) noexcept {
        consumed={};
        if(selector.valid && selector.builder.context==context)consumed=selector;
        selector={};
    }
    selector_witness take(addr context) noexcept {
        auto c=consumed;consumed={};
        if(!c.valid||c.builder.context!=context)return {};
        return c;
    }
    void cancel() noexcept {selector={};consumed={};}
};
inline thread_local thread_transport tls{};
} // namespace dsrrl::runtime::v448
