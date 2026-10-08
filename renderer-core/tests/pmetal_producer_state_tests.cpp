#include "dsrrl/runtime/pmetal_producer_state.hpp"

#include <cassert>

using namespace dsrrl;

int main()
{
    operators::material_response::material_identity material{};
    material.valid = true;
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

    // Regression: a camera-dependent traversal can invoke selectors for many
    // *other* materials before our armor's deferred draw consumes its exact
    // source. Selector begin() with a different full identity must not evict
    // the previously published material through a hash-bucket collision.
    // Deliberately visit far more distinct keys than available cache slots.
    for (std::uint32_t i = 0u; i < 16384u; ++i) {
        auto other = material;
        other.material_slot = 100u + i;
        runtime::pmetal_producer_state_begin(
            other,
            100u);
        runtime::pmetal_producer_state_clear();

        runtime::pmetal_envspec_source retained{};
        assert(runtime::pmetal_producer_state_latest(
            material,
            100u,
            retained));
        assert(retained.beta == 0.75f);
        assert(retained.row_id_a == 1u);
    }

    // Even after unrelated collisions, a failed new selector for the SAME
    // exact armor slot must revoke the old source (fail open, never stale).
    runtime::pmetal_producer_state_begin(
        material,
        100u);
    runtime::pmetal_producer_state_clear();
    runtime::pmetal_envspec_source revoked{};
    assert(!runtime::pmetal_producer_state_latest(
        material,
        100u,
        revoked));

    return 0;
}
