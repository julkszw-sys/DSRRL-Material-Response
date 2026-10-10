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

    // New regression: begin() without a preceding clear() must revoke
    // SAME-material TLS authority as well as the synchronized record.
    // Otherwise latest() returns an old, pre-selector PTDE LightBank source.
    runtime::pmetal_producer_state_clear();
    runtime::pmetal_producer_state_begin(material, 100u);
    runtime::pmetal_producer_state_publish(material, source, 100u);
    runtime::pmetal_envspec_source before_begin{};
    assert(runtime::pmetal_producer_state_latest(
        material, 100u, before_begin));
    assert(runtime::pmetal_producer_state_valid());
    const auto previous_generation = before_begin.generation;

    // Deliberately skip selector_clear(): a failed new source selection
    // must fail open rather than resurrect the old material's TLS source.
    runtime::pmetal_producer_state_begin(material, 100u);
    assert(!runtime::pmetal_producer_state_valid());
    runtime::pmetal_envspec_source after_begin{};
    assert(!runtime::pmetal_producer_state_latest(
        material, 100u, after_begin));

    // An identical successful source after this boundary may reuse the
    // existing generation; invalidation must not erase the source payload.
    runtime::pmetal_producer_state_publish(material, source, 100u);
    runtime::pmetal_envspec_source after_publish{};
    assert(runtime::pmetal_producer_state_latest(
        material, 100u, after_publish));
    assert(after_publish.generation == previous_generation);
    assert(after_publish.beta == 0.75f);

    return 0;
}
