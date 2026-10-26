#include "dsrrl/runtime/pmetal_producer_state.hpp"

#include <cassert>

using namespace dsrrl;

int main()
{
    operators::material_response::material_identity material{};
    material.valid = true;
    material.owner_tuple_exact = true;
    material.model_owner_instance = 0x1000u;
    material.owner_tuple_exact = true;
    material.material_slot_valid = true;
    material.material_slot = 7u;
    material.flver_identity_hash = 0x1234u;
    material.semantic_name_hash = 0x5678u;
    material.flver_sha256[0] = 0x11u;
    material.raw_mtd_sha256[0] = 0x22u;

    runtime::pmetal_envspec_source source{};
    source.a = {1.0f, 2.0f, 3.0f};
    source.b = {4.0f, 5.0f, 6.0f};
    source.beta = 0.25f;
    source.bank_signature_a = 10u;
    source.bank_signature_b = 20u;
    source.row_id_a = 1u;
    source.row_id_b = 2u;

    runtime::pmetal_producer_state_clear();
    assert(!runtime::pmetal_producer_state_valid());

    runtime::pmetal_producer_state_begin(
        material,
        100u);
    runtime::pmetal_producer_state_publish(
        material,
        source,
        100u);

    runtime::pmetal_envspec_source out{};
    assert(runtime::pmetal_producer_state_latest(
        material,
        100u,
        out));
    assert(out.generation == 1u);

    runtime::pmetal_producer_state_publish(
        material,
        source,
        100u);

    runtime::pmetal_envspec_source unchanged{};
    assert(runtime::pmetal_producer_state_latest(
        material,
        100u,
        unchanged));
    assert(unchanged.generation == 1u);

    source.beta = 0.75f;
    runtime::pmetal_producer_state_publish(
        material,
        source,
        100u);

    runtime::pmetal_envspec_source changed{};
    assert(runtime::pmetal_producer_state_latest(
        material,
        100u,
        changed));
    assert(changed.generation == 2u);

    assert(!runtime::pmetal_producer_state_latest(
        material,
        101u,
        out));

    // Simulate selector -> draw thread handoff by dropping only TLS. The exact
    // immutable synchronized value must remain available.
    runtime::pmetal_producer_state_clear();
    assert(!runtime::pmetal_producer_state_valid());
    runtime::pmetal_envspec_source synchronized{};
    assert(runtime::pmetal_producer_state_latest(
        material,
        100u,
        synchronized));
    assert(synchronized.beta == 0.75f);

    // A new exact selector attempt invalidates the old synchronized value
    // before decode. No stale source may survive a failed selector.
    runtime::pmetal_producer_state_begin(
        material,
        100u);
    runtime::pmetal_envspec_source stale{};
    assert(!runtime::pmetal_producer_state_latest(
        material,
        100u,
        stale));

    runtime::pmetal_producer_state_publish(
        material,
        source,
        100u);
    runtime::pmetal_producer_state_clear();
    runtime::pmetal_envspec_source republished{};
    assert(runtime::pmetal_producer_state_latest(
        material,
        100u,
        republished));
    assert(republished.beta == 0.75f);

    // flver_identity_hash is an optional legacy/cache token. A selector may
    // publish it while the authenticated draw material carries zero. The full
    // FLVER SHA-256 + slot + semantic/raw-MTD tuple must remain sufficient.
    auto draw_material = material;
    draw_material.flver_identity_hash = 0u;

    runtime::pmetal_producer_state_begin(
        material,
        100u);
    runtime::pmetal_producer_state_publish(
        material,
        source,
        100u);
    runtime::pmetal_producer_state_clear();

    runtime::pmetal_envspec_source optional_hash_bridge{};
    assert(runtime::pmetal_producer_state_latest(
        draw_material,
        100u,
        optional_hash_bridge));
    assert(optional_hash_bridge.beta == 0.75f);

    // A LightBank belongs to the native model, not its MTD or FLVER slot.
    // One authentic model producer must feed multiple different material
    // identities without accidentally reusing another actor's source.
    auto other_slot = material;
    other_slot.material_slot = 3u;
    other_slot.route_index = 160u;
    other_slot.semantic_name_hash = 0x12345678u;
    other_slot.raw_mtd_sha256[0] = 0xEEu;
    other_slot.material_family_hash = 0xAAu;
    runtime::pmetal_producer_state_begin(material,100u);
    runtime::pmetal_producer_state_publish(material,source,100u);
    runtime::pmetal_producer_state_clear();
    runtime::pmetal_envspec_source same_model{};
    assert(runtime::pmetal_producer_state_latest(
        other_slot,100u,same_model));
    assert(same_model.beta == source.beta);

    auto other_actor = other_slot;
    other_actor.model_owner_instance = 0x2000u;
    runtime::pmetal_envspec_source actor_leak{};
    assert(!runtime::pmetal_producer_state_latest(
        other_actor,100u,actor_leak));

    auto missing_actor = other_slot;
    missing_actor.model_owner_instance = 0u;
    assert(!runtime::pmetal_producer_state_latest(
        missing_actor,100u,actor_leak));
    auto swapped_model = other_slot;
    swapped_model.flver_sha256[0] ^= 0x01u;
    assert(!runtime::pmetal_producer_state_latest(
        swapped_model,100u,actor_leak));
    // A new selector for another slot of THIS instance invalidates the
    // common model source until a new authentic producer publishes.
    runtime::pmetal_producer_state_begin(other_slot,100u);
    assert(!runtime::pmetal_producer_state_latest(
        material,100u,actor_leak));
    runtime::pmetal_producer_state_publish(other_slot,source,100u);
    assert(runtime::pmetal_producer_state_latest(
        material,100u,actor_leak));

    // No MTD is needed for producer/lightbank eligibility; the material
    // response gate remains at the receiving shader, outside this module.
    auto no_mtd = material;
    no_mtd.route_index = 0u;
    no_mtd.semantic_name_hash = 0u;
    no_mtd.raw_mtd_sha256 = {};
    runtime::pmetal_producer_state_clear();
    assert(runtime::pmetal_producer_state_latest(
        no_mtd,100u,actor_leak));

    return 0;
}
}
