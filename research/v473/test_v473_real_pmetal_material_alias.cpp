// V473: compile the REAL production pmetal_producer_state.cpp, not a mock.
// Pure CPU semantic contract test; no ReShade runtime, no EXE modifications.
// This proves material-key non-injectivity with respect to renderable/actor
// identity, NOT that a particular user-visible pixel is wrong.
#include "dsrrl/runtime/pmetal_producer_state.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <thread>
#include <type_traits>
#include <utility>

using dsrrl::operators::material_response::material_identity;
using dsrrl::runtime::pmetal_envspec_source;
using namespace dsrrl::runtime;

template<class T,class=void>
struct has_native_instance:std::false_type{};
template<class T>
struct has_native_instance<T,std::void_t<decltype(std::declval<T>().native_entity_full_handle)>>:std::true_type{};
// The material identity is a RESOURCE/MTD tuple, NOT an instance identity.
static_assert(!has_native_instance<material_identity>::value,
              "Material key unexpectedly gained native entity handle: re-audit this test");

material_identity material(std::uint32_t slot=0) {
    material_identity m{};
    m.valid=true;
    m.owner_tuple_exact=true;
    m.material_slot_valid=true;
    m.actual_material_exact=true;
    m.route_index=345;
    m.material_slot=slot;
    m.semantic_name_hash=0x345a1255u;
    m.material_family_hash=0x9090u;
    m.flver_sha256[0]=0xabu;
    m.raw_mtd_sha256[0]=0xcdu;
    return m;
}
pmetal_envspec_source source(std::uint32_t row) {
    pmetal_envspec_source s{};
    s.row_id_a=row;
    s.row_id_b=row;
    s.bank_signature_a=0xa01u;
    s.bank_signature_b=0xa01u;
    s.a={float(row),0.f,0.f};
    s.b=s.a;
    return s;
}
void publish(const material_identity&m,std::uint64_t epoch,std::uint32_t row) {
    pmetal_producer_state_clear();
    pmetal_producer_state_begin(m,epoch);
    pmetal_producer_state_publish(m,source(row),epoch);
}
int main() {
    const auto m=material();
    constexpr std::uint64_t epoch=0x713au;
    // Two distinct hypothetical FrpgModel/receiver instances that share
    // exact FLVER/MTD/slot identity. The actual API has NO instance parameter.
    constexpr std::uint32_t instanceA=0x81000f10u, instanceB=0x81000d8cu;
    static_assert(instanceA != instanceB, "different native model instances");
    std::thread a([&] { publish(m,epoch,30u); });
    a.join();
    pmetal_envspec_source out{};
    std::thread consumerA([&]{
        assert(pmetal_producer_state_latest(m,epoch,out));
    });
    consumerA.join();
    assert(out.row_id_a==30);
    std::thread b([&]{publish(m,epoch,31u);});
    b.join();
    std::thread lateA([&]{
        assert(pmetal_producer_state_latest(m,epoch,out));
    });
    lateA.join();
    // The same material-only query can ONLY retrieve the latest row31,
    // even when the query conceptually belongs to distinct instanceA.
    assert(out.row_id_a==31u && out.row_id_b==31u);
    std::cout<<"V473_REAL_SOURCE_MATERIAL_ALIAS_CONFIRMED row_after_second_actor="
             <<out.row_id_a<<" (instance parameter missing)\n";

    // Different *material* tuples remain independently addressable.
    const auto other=material(1u);
    publish(other,epoch,48u);
    std::thread lastM([&]{ assert(pmetal_producer_state_latest(m,epoch,out)); });
    lastM.join();
    assert(out.row_id_a==31u);
    std::thread lastOther([&]{assert(pmetal_producer_state_latest(other,epoch,out));});
    lastOther.join();
    assert(out.row_id_a==48u);
    std::cout<<"V473_DISTINCT_MATERIAL_TUPLES_REMAIN_ISOLATED rows=31,48\n";
    std::cout<<"STATUS SOURCE_KEY_NON_INJECTIVE_FOR_ACTOR; PIXEL=OPEN; RX34_ACTOR_JOIN=OPEN\n";
}
