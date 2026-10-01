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

    runtime::pmetal_producer_state_clear();
    assert(!runtime::pmetal_producer_state_valid());
    return 0;
}
