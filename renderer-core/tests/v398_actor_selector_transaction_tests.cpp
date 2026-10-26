#include "dsrrl/runtime/pmetal_producer_state.hpp"
#include <cassert>
#include <atomic>
#include <thread>

using namespace dsrrl;
using namespace dsrrl::runtime;
int main() {
    operators::material_response::material_identity actor{};
    actor.valid=true; actor.owner_tuple_exact=true;
    actor.model_owner_instance=0x21962800u;
    actor.flver_sha256[0]=7u;
    pmetal_envspec_source light{};
    light.row_id_a=30u;light.row_id_b=30u;
    light.a={1.2f,0.8f,0.65f};light.b=light.a;
    light.bank_signature_a=88u;light.bank_signature_b=88u;
    pmetal_envspec_source out{};
    // Producer and consumer are different contexts; use synchronized path.
    pmetal_producer_state_begin(actor,100u,
        reinterpret_cast<void*>(0x12340u),1310,
        reinterpret_cast<void*>(0x12340u),1310,0.0f);
    pmetal_producer_state_publish(actor,light,100u);
    pmetal_producer_state_clear();
    assert(pmetal_producer_state_latest(actor,100u,out));
    assert(out.row_id_a==30u);
    const auto generation=out.generation;
    // Exact native tuple repeat followed by witness cache miss.
    pmetal_producer_state_begin(actor,100u,
        reinterpret_cast<void*>(0x12340u),1310,
        reinterpret_cast<void*>(0x12340u),1310,0.0f);
    assert(pmetal_producer_state_latest(actor,100u,out));
    assert(out.generation==generation);
    assert(pmetal_producer_state_actor_tracked(actor,100u));
    // Every FLVER slot and MTD on this actor sees the same native bank.
    auto armor=actor;armor.material_slot=9u;
    armor.flver_sha256[0]^=0x7fu;armor.raw_mtd_sha256[0]=5u;
    assert(pmetal_producer_state_latest(armor,100u,out));
    // Another actor sharing all assets never receives this donor.
    auto other=actor;other.model_owner_instance=0x21960280u;
    assert(!pmetal_producer_state_latest(other,100u,out));
    assert(!pmetal_producer_state_actor_tracked(other,100u));
    // A changed row invalidates immediately; do not force row5 or row0.
    pmetal_producer_state_begin(actor,100u,
        reinterpret_cast<void*>(0x12340u),1311,
        reinterpret_cast<void*>(0x12340u),1311,0.0f);
    assert(!pmetal_producer_state_latest(actor,100u,out));
    assert(pmetal_producer_state_actor_tracked(actor,100u));
    light.row_id_a=31u;light.row_id_b=31u;
    pmetal_producer_state_publish(actor,light,100u);
    assert(pmetal_producer_state_latest(armor,100u,out));
    assert(out.row_id_a==31u);
    // Same row different area is a distinct full selector.
    pmetal_producer_state_begin(actor,100u,
        reinterpret_cast<void*>(0x12340u),1055,
        reinterpret_cast<void*>(0x12340u),1055,0.0f);
    assert(!pmetal_producer_state_latest(actor,100u,out));
    // Genuine zero c86 is allowed (not substituted with brightness).
    light.row_id_a=0u;light.row_id_b=0u;light.a={0,0,0};light.b=light.a;
    pmetal_producer_state_begin(actor,100u,
        reinterpret_cast<void*>(0x12340u),1280,
        reinterpret_cast<void*>(0x12340u),1280,0.0f);
    pmetal_producer_state_publish(actor,light,100u);
    assert(pmetal_producer_state_latest(actor,100u,out));
    assert(out.a[0]==0.0f && out.row_id_a==0u);
    // Cross-thread source switch: stale publish must not replace new row.
    std::atomic<int> phase{0};
    std::thread old_writer([&]{
        pmetal_producer_state_begin(actor,100u,
            reinterpret_cast<void*>(0x12340u),1282,
            reinterpret_cast<void*>(0x12340u),1282,0.0f);
        phase.store(1,std::memory_order_release);
        while(phase.load(std::memory_order_acquire)<2)
            std::this_thread::yield();
        auto late=light;late.row_id_a=2u;late.row_id_b=2u;
        pmetal_producer_state_publish(actor,late,100u);
    });
    while(phase.load(std::memory_order_acquire)<1)
        std::this_thread::yield();
    pmetal_producer_state_begin(actor,100u,
        reinterpret_cast<void*>(0x12340u),1283,
        reinterpret_cast<void*>(0x12340u),1283,0.0f);
    light.row_id_a=3u;light.row_id_b=3u;light.a={3,3,3};light.b=light.a;
    pmetal_producer_state_publish(actor,light,100u);
    phase.store(2,std::memory_order_release);
    old_writer.join();
    assert(pmetal_producer_state_latest(actor,100u,out));
    assert(out.row_id_a==3u);
    // Reproduce a real 4096-bucket collision: both aligned owner pointers
    // index bucket 72, but must keep separate immutable LightBank states.
    auto collide_a=actor, collide_b=actor;
    collide_a.model_owner_instance=0x21900080u;
    collide_b.model_owner_instance=0x21901080u;
    auto row_a=light,row_b=light;
    row_a.row_id_a=4u;row_a.row_id_b=4u;
    row_b.row_id_a=5u;row_b.row_id_b=5u;
    pmetal_producer_state_begin(collide_a,100u,
        reinterpret_cast<void*>(0x12340u),1284,
        reinterpret_cast<void*>(0x12340u),1284,0.0f);
    pmetal_producer_state_publish(collide_a,row_a,100u);
    pmetal_producer_state_begin(collide_b,100u,
        reinterpret_cast<void*>(0x12340u),1285,
        reinterpret_cast<void*>(0x12340u),1285,0.0f);
    pmetal_producer_state_publish(collide_b,row_b,100u);
    pmetal_producer_state_clear();
    assert(pmetal_producer_state_latest(collide_a,100u,out));
    assert(out.row_id_a==4u);
    assert(pmetal_producer_state_latest(collide_b,100u,out));
    assert(out.row_id_a==5u);
    // An unobserved consumer must not occupy a cache entry.
    auto unobserved=actor;
    unobserved.model_owner_instance=0x21903080u;
    assert(!pmetal_producer_state_latest(unobserved,100u,out));
    assert(!pmetal_producer_state_actor_tracked(unobserved,100u));

    // New source epoch fails open rather than serving older actor state.
    assert(!pmetal_producer_state_latest(actor,101u,out));
    assert(!pmetal_producer_state_actor_tracked(actor,101u));
    return 0;
}
