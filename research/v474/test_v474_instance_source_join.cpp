// V474: unit tests over REAL production material_identity and source types.
// No EXE hook; no 2.3.1 source modification; source/instance binding proof only.
#include "research/v474/instance_source_journal.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>
using namespace dsrrl::research::v474;

material_identity m(std::uint32_t slot=0) {
 material_identity x{};
 x.valid=x.owner_tuple_exact=x.material_slot_valid=x.actual_material_exact=true;
 x.route_index=345u;x.material_slot=slot;x.semantic_name_hash=0x12345678u;
 x.material_family_hash=0x912345u;x.flver_sha256[0]=0xedu;
 x.raw_mtd_sha256[0]=0xecu;return x;
}
source s(std::uint32_t row) {
 source v{};v.row_id_a=v.row_id_b=row;
 v.bank_signature_a=v.bank_signature_b=0xa01u;
 v.a={static_cast<float>(row),0.f,0.f};v.b=v.a;
 return v;
}
exact_ticket t(std::uintptr_t address,std::uint32_t full_handle,
 std::uint64_t seq,std::uint64_t epoch=7u,std::uint32_t slot=0u) {
 exact_ticket x{};x.entity=address;x.draw_param=address+0x108u;
 x.full_handle=full_handle;x.native_builder_sequence=seq;x.selector_epoch=epoch;
 x.receiver=34;x.typed_model_entity=x.native_source1_issued=true;
 x.material=m(slot);return x;
}
int main() {
 exact_instance_journal j{};
 const auto a=t(0x2189d9f0u,0x81000f10u,20u);
 const auto b=t(0x218d25d0u,0x81000d8cu,21u);
 assert(same_material(a.material,b.material));
 assert(j.publish(a,s(30u)) && j.publish(b,s(31u)));
 source out{};
 assert(j.read(a,out)&&out.row_id_a==30u);
 assert(j.read(b,out)&&out.row_id_a==31u);
 // Reverse query: no latest-material alias.
 assert(j.read(a,out)&&out.row_id_a==30u);
 std::cout<<"V474_ENTITY_ISOLATION PASS row30 != row31 on same material\n";
 // A single incorrect low-byte handle or model pointer is insufficient.
 auto bad=t(0x2189d9f0u,0x82000f10u,20u);
 assert(!j.read(bad,out));
 bad=a;bad.draw_param+=0x10;assert(!j.read(bad,out));
 bad=a;bad.native_builder_sequence+=1;assert(!j.read(bad,out));
 bad=a;bad.selector_epoch+=1;assert(!j.read(bad,out));
 bad=a;bad.receiver=33;assert(!j.read(bad,out));
 bad=a;bad.typed_model_entity=false;assert(!j.read(bad,out));
 bad=a;bad.native_source1_issued=false;assert(!j.read(bad,out));
 bad=a;bad.material=m(1);assert(!j.read(bad,out));
 std::cout<<"V474_UNTRUSTED_JOIN_FAIL_OPEN PASS (handle/DP/seq/epoch/family/material)\n";
 // Deferred A draw must not consume B's newer update.
 const auto anewer=t(a.entity,a.full_handle,22u);
 assert(j.publish(anewer,s(32u)));
 assert(j.read(a,out) && out.row_id_a==30u);
 assert(j.read(anewer,out) && out.row_id_a==32u);
 // Same ticket conflicting source => poison and fail-open.
 assert(!j.publish(anewer,s(99u)));
 assert(!j.read(anewer,out));
 // Same complete registration in same epoch but different pointer quarantines.
 auto c=t(0x219aab30u,b.full_handle,40u);
 assert(!j.publish(c,s(40u)));
 assert(!j.read(b,out));
 std::cout<<"V474_STALE_DRAW_AND_HANDLE_ABA_FAIL_OPEN PASS\n";
 // Strongly check independent identity across threads under sharing.
 j.reset();const auto aa=t(0x22000000u,0x81000110u,100u),
 bb=t(0x22010000u,0x81000111u,101u);
 std::thread x([&]{assert(j.publish(aa,s(30u)));});
 std::thread y([&]{assert(j.publish(bb,s(31u)));});
 x.join();y.join();
 std::thread xx([&]{source z{};assert(j.read(bb,z)&&z.row_id_a==31u);});
 std::thread yy([&]{source z{};assert(j.read(aa,z)&&z.row_id_a==30u);});
 xx.join();yy.join();
 // Destruction invalidates exact model, not another.
 j.retire(aa.entity,aa.full_handle);
 assert(!j.read(aa,out));
 assert(j.read(bb,out)&&out.row_id_a==31u);
 std::cout<<"V474_CROSSTHREAD_AND_RETIRE PASS; PTDE_PIXEL=OPEN\n";
}
