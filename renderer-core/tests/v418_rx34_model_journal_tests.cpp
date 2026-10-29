#include <cassert>
#include <cstdint>
#include <memory>
#include "dsrrl/runtime/v411_owner_sequence_journal.hpp"

using namespace dsrrl::runtime::v411;
static witness sample(std::uint64_t sequence,std::uintptr_t model,
    std::uint32_t handle) {
    witness w{};
    w.owner=0x2191bd80u;
    w.wrapper=0x216b7b20u;
    w.manager=0x216b2cf0u;
    w.native_sequence=sequence;
    w.selector_epoch=sequence+100u;
    w.source_a=0x1a000u;
    w.source_b=0x1a000u;
    w.selector_a=1310;
    w.selector_b=1310;
    w.row_a=30u;
    w.row_b=30u;
    w.context_vtable=0x1412e6b60u;
    w.context_manager=w.manager;
    w.vtable_readable=true;
    w.manager_field_readable=true;
    w.context_manager_matches_wrapper=true;
    w.model_this=model;
    w.fg_parent=model+0x40u;
    w.model_class=1u;
    w.model_material_index=0;
    w.fg_registry_id=handle;
    w.model_verified=true;
    w.fg_reciprocal=true;
    w.registry_readable=true;
    w.registry_allocated=(handle&0x80000000u)!=0u;
    return w;
}
int main(){
    auto journal1=std::make_unique<journal>();
    auto a=sample(83,0x1000u,0x83000042u);
    assert(journal1->observe(a));
    auto out=journal1->lookup(a.owner,a.native_sequence,
        a.selector_epoch,a.wrapper,a.manager);
    assert(out.state==status::matched_snapshot);
    assert(out.prior.model_this==0x1000u);
    assert(out.prior.fg_parent==0x1040u);
    assert(out.prior.fg_registry_id==0x83000042u);
    assert(out.candidates==1u);
    // Legitimate repeat must not create a second journal witness.
    assert(journal1->observe(a));
    out=journal1->lookup(a.owner,a.native_sequence,
        a.selector_epoch,a.wrapper,a.manager);
    assert(out.candidates==1u);
    // Conflicting model for same native producer sequence MUST be rejected
    // observationally even if context, wrapper, manager and light rows agree.
    auto b=a;b.model_this=0x2000u;b.fg_parent=0x2040u;
    assert(journal1->observe(b));
    out=journal1->lookup(a.owner,a.native_sequence,
        a.selector_epoch,a.wrapper,a.manager);
    assert(out.state==status::ambiguous_context_identity);
    assert(out.candidates==2u);
    // Generation change on the same model+parent with one native producer
    // sequence is also a semantic conflict.
    auto c=a;c.fg_registry_id=0x84000042u;
    assert(journal1->observe(c));
    out=journal1->lookup(a.owner,a.native_sequence,
        a.selector_epoch,a.wrapper,a.manager);
    assert(out.state==status::ambiguous_context_identity);
    assert(out.candidates==3u);
    auto journal2=std::make_unique<journal>();
    auto first=sample(90,0x1000u,0x81000042u);
    auto next=sample(91,0x2000u,0x82000042u);
    next.row_a=31u;next.row_b=31u;
    assert(journal2->observe(first));
    assert(journal2->observe(next));
    auto prev=journal2->lookup(first.owner,first.native_sequence,
        first.selector_epoch,first.wrapper,first.manager);
    assert(prev.state==status::matched_snapshot);
    assert(prev.superseded_by_newer);
    auto latest=journal2->lookup(next.owner,next.native_sequence,
        next.selector_epoch,next.wrapper,next.manager);
    assert(latest.state==status::matched_snapshot);
    assert(!latest.superseded_by_newer);
    assert(latest.prior.fg_registry_id==0x82000042u);
}
