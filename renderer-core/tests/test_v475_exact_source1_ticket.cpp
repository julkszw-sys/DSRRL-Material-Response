// V475 original material_identity + pmetal_envspec_source test contract.
// This tests ONLY the ticket carrier, not Win64 unwind, live hooks or pixels.
#include "dsrrl/runtime/v475_instance_source1_transport.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <thread>
using namespace dsrrl::runtime::v475;

material_identity mat(unsigned slot=0) {
    material_identity m{};
    m.valid = m.owner_tuple_exact = m.material_slot_valid =
        m.actual_material_exact = true;
    m.flver_sha256[0] = 0xed;
    m.raw_mtd_sha256[0] = 0xec;
    m.semantic_name_hash = 0x1515;
    m.material_family_hash = 0x2515;
    m.route_index = 345;
    m.material_slot = slot;
    return m;
}
source src(unsigned row) {
    source s{};
    s.a = s.b = {static_cast<float>(row), 0.2f, 0.6f};
    s.row_id_a = s.row_id_b = row;
    s.bank_signature_a = s.bank_signature_b = 0x5105;
    return s;
}
ticket tk(std::uintptr_t entity,unsigned handle,unsigned row) {
    ticket t{};
    t.entity = entity;
    t.draw_param = entity + 0x108;
    t.packet = 0x202fa000u + entity % 0x1000u;
    t.context = 0x21f00000u + entity % 0x1000u;
    t.engine_model = 0x20000000u + entity % 0x1000u;
    t.native_frame = 0x1ffaf000u + entity % 0x1000u;
    t.registration_handle = handle;
    t.packed_a = t.packed_b = row==30 ? 0x051e : 0x051f;
    t.selector_epoch = 5u;
    t.material = mat();
    t.exact_type = t.default_builder = true;
    return t;
}
int main() {
    const auto a = tk(0x2189d9f0u,0x81000f10u,30);
    const auto b = tk(0x218d25d0u,0x81000d8cu,31);
    assert(same_material(a.material,b.material));
    thread_scope_journal journal{};
    source out{};
    assert(journal.publish(a,src(30)));
    // Same material, different real instance: never return A's signal for B.
    assert(journal.take(b,out)==verdict::mismatch);
    assert(journal.take(a,out)==verdict::exact && out.row_id_a==30);
    assert(journal.take(a,out)==verdict::expired);
    journal.begin_selector();
    assert(journal.take(a,out)==verdict::absent);
    assert(journal.publish(b,src(31)));
    assert(journal.take(a,out)==verdict::mismatch);
    assert(journal.take(b,out)==verdict::exact && out.row_id_a==31);
    // A 16-bit or low-byte handle shortcut is forbidden.
    journal.begin_selector();
    assert(journal.publish(a,src(30)));
    auto bad=a;bad.registration_handle=0x82000f10u;
    assert(journal.take(bad,out)==verdict::mismatch);
    bad=a;bad.packet+=4;assert(journal.take(bad,out)==verdict::mismatch);
    bad=a;bad.native_frame+=8;assert(journal.take(bad,out)==verdict::mismatch);
    bad=a;bad.draw_param+=8;assert(journal.take(bad,out)==verdict::invalid);
    bad=a;bad.context+=8;assert(journal.take(bad,out)==verdict::mismatch);
    bad=a;bad.engine_model+=8;assert(journal.take(bad,out)==verdict::mismatch);
    bad=a;bad.selector_epoch+=1;assert(journal.take(bad,out)==verdict::absent);
    bad=a;bad.material=mat(1);assert(journal.take(bad,out)==verdict::absent);
    bad=a;bad.material.raw_mtd_sha256[0]^=0x01u;
    assert(journal.take(bad,out)==verdict::absent);
    assert(journal.take(a,out)==verdict::exact);
    // Independent PHN k135 carrier mismatch must poison repeated ticket.
    journal.begin_selector();
    assert(journal.publish(a,src(30)));
    auto altered=src(30);altered.phn_k135_valid=true;altered.phn_k135=2.0f;
    assert(!journal.publish(a,altered));
    assert(journal.take(a,out)==verdict::poisoned);
    // No global row lock; only authenticated native row30/31 steady path.
    bad=a;bad.packed_a=bad.packed_b=0x0520;
    assert(!journal.publish(bad,src(32)));
    bad=a;bad.beta_bits=0x3f000000u;
    assert(!journal.publish(bad,src(30)));
    bad=a;bad.registration_handle=0u;
    assert(!journal.publish(bad,src(30)));
    bad=a;bad.exact_type=false;
    assert(!journal.publish(bad,src(30)));
    // Thread-local isolation; no stale cross-thread reuse.
    journal.begin_selector();
    bool sibling_absent=false,local_ok=false;
    std::thread worker([&] {
        source z{};
        sibling_absent=(tls.take(a,z)==verdict::absent);
        tls.begin_selector();
        local_ok=tls.publish(b,src(31)) &&
            tls.take(b,z)==verdict::exact && z.row_id_a==31;
    });
    tls.begin_selector();
    assert(tls.publish(a,src(30)));
    worker.join();
    assert(sibling_absent && local_ok);
    assert(tls.take(a,out)==verdict::exact && out.row_id_a==30);
    std::cout<<"V475_SOURCE1_INSTANCE_FULL_TICKET PASS native row30/31 independent material twins, stale/ABA/packet/frame/full-handle fail-open, no PTDE pixel claim\n";
}
