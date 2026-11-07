#pragma once
// Read-only RX34 source -> issued native packet certificate. NOT gameplay lifetime.
#include <cstdint>
#include "v448_live_builder_core.hpp"
namespace dsrrl::runtime::v450 {
enum class verdict : std::uint32_t {
 no_source=0,no_issued_builder=1,source_sequence_mismatch=2,
 context_mismatch=3,entity_mismatch=4,drawparam_mismatch=5,
 packet_mismatch=6,model_mismatch=7,native_ab_mismatch=8,
 row_mismatch=9,exact_native_packet_generation_open=10
};
struct decision { verdict state=verdict::no_source; std::uint64_t sequence=0; };
inline decision evaluate(const v447::source_ticket&p,const v448::builder_witness&w,
 std::uint64_t seq,std::uintptr_t context,std::uint32_t row) noexcept {
 decision o{};o.sequence=seq;
 if(!v447::source_well_formed(p))return o;
 if(!w.valid||w.builder_family!=1u){o.state=verdict::no_issued_builder;return o;}
 if(p.sequence!=seq){o.state=verdict::source_sequence_mismatch;return o;}
 if(p.context!=context||w.context!=context){o.state=verdict::context_mismatch;return o;}
 if(p.entity!=w.entity){o.state=verdict::entity_mismatch;return o;}
 if(p.drawparam!=w.dp||w.dp!=w.entity+0x108u){o.state=verdict::drawparam_mismatch;return o;}
 if(p.packet!=w.packet){o.state=verdict::packet_mismatch;return o;}
 if(p.model!=w.engine_model){o.state=verdict::model_mismatch;return o;}
 if(p.native_a!=w.a||p.native_b!=w.b||p.beta_bits!=w.beta_bits||
    p.native_a!=p.native_b||p.beta_bits!=0u){o.state=verdict::native_ab_mismatch;return o;}
 if(p.row_a!=row||p.row_b!=row||row!=(static_cast<std::uint32_t>(w.a)&0xffu)){
    o.state=verdict::row_mismatch;return o;}
 o.state=verdict::exact_native_packet_generation_open;return o;
}
}
