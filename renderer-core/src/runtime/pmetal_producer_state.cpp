#include "dsrrl/runtime/pmetal_producer_state.hpp"

#include <cstring>

namespace dsrrl::runtime {
namespace {

struct producer_record {
    operators::material_response::material_identity material{};
    pmetal_envspec_source source{};
    std::uint64_t epoch = 0u;
    std::uint64_t generation = 0u;
    bool valid = false;
};

thread_local producer_record g_record{};

bool same_material(
    const operators::material_response::material_identity &a,
    const operators::material_response::material_identity &b) noexcept
{
    return
        a.valid == b.valid &&
        a.owner_tuple_exact == b.owner_tuple_exact &&
        a.material_slot_valid == b.material_slot_valid &&
        a.flver_sha256 == b.flver_sha256 &&
        a.raw_mtd_sha256 == b.raw_mtd_sha256 &&
        a.material_slot == b.material_slot &&
        a.flver_identity_hash == b.flver_identity_hash &&
        a.semantic_name_hash == b.semantic_name_hash;
}

bool same_source_payload(
    const pmetal_envspec_source &a,
    const pmetal_envspec_source &b) noexcept
{
    return
        a.a == b.a &&
        a.b == b.b &&
        a.beta == b.beta &&
        a.bank_signature_a == b.bank_signature_a &&
        a.bank_signature_b == b.bank_signature_b &&
        a.row_id_a == b.row_id_a &&
        a.row_id_b == b.row_id_b;
}

} // namespace

void pmetal_producer_state_clear() noexcept
{
    g_record.valid = false;
}

void pmetal_producer_state_publish(
    const operators::material_response::material_identity &material,
    const pmetal_envspec_source &source,
    std::uint64_t epoch) noexcept
{
    const bool unchanged =
        g_record.valid &&
        g_record.epoch == epoch &&
        same_material(g_record.material, material) &&
        same_source_payload(g_record.source, source);

    const std::uint64_t next_generation =
        unchanged
            ? g_record.generation
            : g_record.generation + 1u;

    g_record.material = material;
    g_record.source = source;
    g_record.source.serial = epoch;
    g_record.source.generation = next_generation;
    g_record.epoch = epoch;
    g_record.generation = next_generation;
    g_record.valid = true;
}

bool pmetal_producer_state_latest(
    const operators::material_response::material_identity &material,
    std::uint64_t epoch,
    pmetal_envspec_source &out) noexcept
{
    out = {};
    if (!g_record.valid ||
        g_record.epoch != epoch ||
        !same_material(g_record.material, material))
        return false;

    out = g_record.source;
    return true;
}

bool pmetal_producer_state_valid() noexcept
{
    return g_record.valid;
}

} // namespace dsrrl::runtime
