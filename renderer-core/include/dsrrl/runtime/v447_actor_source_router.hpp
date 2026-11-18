#pragma once
// V447: provenance-only, exact per-actor LightBank source certificate.
// No row policy, no actor inference from shared context, no render mutations.
// Standalone C++17. Validated states can be used by a future opt-in bridge;
// current V447 integration only observes, never authorizes an RX34 patch.
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>

namespace dsrrl::runtime::v447 {
using u64=std::uint64_t;
using addr=std::uintptr_t;
struct source_ticket {
    u64 sequence=0;
    addr entity=0,drawparam=0,packet=0,context=0,model=0;
    u64 actor_generation=0; // ZERO until exact constructor/destructor witness exists.
    std::uint16_t native_a=0,native_b=0;
    std::uint32_t beta_bits=0,row_a=0,row_b=0;
};
struct receiver_ticket {
    u64 native_sequence=0;
    addr context=0,entity=0,drawparam=0,packet=0,model=0;
    u64 actor_generation=0;
    std::uint32_t row_a=0;
};
enum class verdict:unsigned {
    absent=0, malformed_source=1, serial_conflict=2,
    context_mismatch=3, row_mismatch=4,
    missing_receiver_identity=5, actor_mismatch=6,
    missing_generation=7, generation_mismatch=8,
    packet_mismatch=9, model_mismatch=10,
    exact_actor_packet=11
};
struct resolution {verdict state=verdict::absent;source_ticket producer{};};
inline bool source_well_formed(const source_ticket &t) noexcept {
    // An entity address is not a generation and a context is not an actor.
    // V443 verifies model draw entity's embedded FrpgDrawParam at +0x108.
    return t.sequence && t.entity && t.entity <= UINTPTR_MAX-0x108u &&
        t.drawparam==t.entity+0x108u && t.packet && t.context && t.model &&
        t.native_a!=0xffffu && t.native_b!=0xffffu &&
        t.row_a==(static_cast<unsigned>(t.native_a)&0xffu) &&
        t.row_b==(static_cast<unsigned>(t.native_b)&0xffu);
}
inline bool same_source(const source_ticket &a,const source_ticket &b) noexcept {
    return a.sequence==b.sequence && a.entity==b.entity &&
        a.drawparam==b.drawparam && a.packet==b.packet &&
        a.context==b.context && a.model==b.model &&
        a.actor_generation==b.actor_generation &&
        a.native_a==b.native_a && a.native_b==b.native_b &&
        a.beta_bits==b.beta_bits && a.row_a==b.row_a && a.row_b==b.row_b;
}
class registry {
    static constexpr std::size_t capacity=8192u;
    struct entry {source_ticket source{};bool valid=false,conflict=false;};
    std::array<entry,capacity> slots_{};
    mutable std::mutex mutex_{};
public:
    verdict observe(const source_ticket &t) noexcept {
        if(!source_well_formed(t))return verdict::malformed_source;
        std::lock_guard<std::mutex> lock(mutex_);
        entry &e=slots_[t.sequence%capacity];
        if(e.valid && e.source.sequence==t.sequence){
            if(e.conflict || !same_source(e.source,t)){
                e.conflict=true;return verdict::serial_conflict;
            }
            return verdict::exact_actor_packet; // duplicate identical witness
        }
        e.source=t;e.valid=true;e.conflict=false;
        return verdict::exact_actor_packet; // valid packet witness, NOT receiver authority
    }
    resolution resolve(const receiver_ticket &r) const noexcept {
        if(!r.native_sequence)return {};
        std::lock_guard<std::mutex> lock(mutex_);
        const entry &e=slots_[r.native_sequence%capacity];
        if(!e.valid || e.source.sequence!=r.native_sequence)return {};
        resolution out{};out.producer=e.source;
        if(e.conflict){out.state=verdict::serial_conflict;return out;}
        if(e.source.context!=r.context){out.state=verdict::context_mismatch;return out;}
        if(e.source.row_a!=r.row_a){out.state=verdict::row_mismatch;return out;}
        // These are intentionally unavailable at a generic RX34 issued callback.
        // A model/FLVER pointer, shared context or source serial MUST NOT substitute.
        if(!r.entity || !r.drawparam || !r.packet || !r.model){
            out.state=verdict::missing_receiver_identity;return out;
        }
        if(r.entity!=e.source.entity || r.drawparam!=e.source.drawparam){
            out.state=verdict::actor_mismatch;return out;
        }
        if(!r.actor_generation || !e.source.actor_generation){
            out.state=verdict::missing_generation;return out;
        }
        if(r.actor_generation!=e.source.actor_generation){
            out.state=verdict::generation_mismatch;return out;
        }
        if(r.packet!=e.source.packet){out.state=verdict::packet_mismatch;return out;}
        if(r.model!=e.source.model){out.state=verdict::model_mismatch;return out;}
        out.state=verdict::exact_actor_packet;
        return out;
    }
};
inline registry &observations() noexcept {static registry r;return r;}
} // namespace dsrrl::runtime::v447
