#pragma once

#include "dsrrl/operators/postprocess/waterwave_authored_identity.hpp"

#include <cstdint>

namespace dsrrl::runtime::bloom_fx_draw_transport {

enum class fx_draw_entity_kind : std::uint8_t {
    unknown = 0,
    particle,
    cluster
};

enum class fx_particle_model_join_channel : std::uint8_t {
    none = 0,
    appearance_owner,
    appearance_source_primary,
    appearance_source_secondary
};

struct fx_draw_snapshot {
    std::uint64_t generation = 0;
    std::uint64_t particle_model_generation = 0;
    std::uint64_t backend_semantic_generation = 0;
    fx_draw_entity_kind kind = fx_draw_entity_kind::unknown;
    fx_particle_model_join_channel particle_model_join_channel =
        fx_particle_model_join_channel::none;
    void *entity = nullptr;
    void *appearance_state = nullptr;
    void *appearance_owner = nullptr;
    void *appearance_source_primary = nullptr;
    void *appearance_source_secondary = nullptr;
    void *particle_model_instance = nullptr;
    void *particle_model_arg2 = nullptr;
    void *particle_model_arg3 = nullptr;
    void *particle_model_arg4 = nullptr;
    void *particle_model_arg5 = nullptr;
    void *draw_context = nullptr;
    std::uint32_t mode_token = 0;
    std::uint32_t appearance_semantic_word = 0;
    std::uint32_t appearance_backend_key = 0;
    std::uint32_t waterwave_runtime_semantic_index = 0;
    bool exact_entity_vtable = false;
    bool exact_appearance_vtable = false;
    bool appearance_state_ready = false;
    bool source_links_ready = false;
    bool particle_model_instance_join = false;
    bool waterwave_authored_identity_exact = false;
    bool waterwave_same_model_instance = false;
    bool appearance_backend_key_observed = false;
    bool backend_key_matches_waterwave_runtime_index = false;
    bool ready = false;
};

enum class waterwave_draw_authority_result : std::uint8_t {
    authorized = 0,
    draw_snapshot_not_ready,
    wrong_entity_kind,
    model_instance_not_joined,
    model_join_channel_missing,
    model_generation_missing,
    authored_identity_not_exact,
    same_instance_join_not_closed
};

inline waterwave_draw_authority_result
validate_waterwave_draw_authority(
    const fx_draw_snapshot &snapshot) noexcept
{
    if (!snapshot.ready ||
        !snapshot.exact_entity_vtable ||
        !snapshot.exact_appearance_vtable ||
        snapshot.draw_context == nullptr)
        return waterwave_draw_authority_result::
            draw_snapshot_not_ready;

    if (snapshot.kind !=
        fx_draw_entity_kind::particle)
        return waterwave_draw_authority_result::
            wrong_entity_kind;

    if (!snapshot.particle_model_instance_join ||
        snapshot.particle_model_instance == nullptr)
        return waterwave_draw_authority_result::
            model_instance_not_joined;

    if (snapshot.particle_model_join_channel ==
        fx_particle_model_join_channel::none)
        return waterwave_draw_authority_result::
            model_join_channel_missing;

    if (snapshot.particle_model_generation == 0u)
        return waterwave_draw_authority_result::
            model_generation_missing;

    if (!snapshot.waterwave_authored_identity_exact)
        return waterwave_draw_authority_result::
            authored_identity_not_exact;

    if (!snapshot.waterwave_same_model_instance)
        return waterwave_draw_authority_result::
            same_instance_join_not_closed;

    return waterwave_draw_authority_result::authorized;
}

struct telemetry {
    bool provenance_ok = false;
    bool particle_hook_armed = false;
    bool cluster_hook_armed = false;
    bool particle_model_ctor_hook_armed = false;
    bool particle_model_dtor_hook_armed = false;
    bool particle_state_update_hook_armed = false;
    bool semantic_index_getter_attested = false;
    bool restore_failed = false;
    bool quarantined = false;

    std::uint64_t particle_events = 0;
    std::uint64_t cluster_events = 0;
    std::uint64_t exact_entity_hits = 0;
    std::uint64_t entity_rejects = 0;
    std::uint64_t state_ready_hits = 0;
    std::uint64_t state_missing = 0;
    std::uint64_t exact_state_hits = 0;
    std::uint64_t state_vtable_rejects = 0;
    std::uint64_t source_links_ready = 0;
    std::uint64_t source_links_missing = 0;
    std::uint64_t particle_model_ctor_events = 0;
    std::uint64_t particle_model_dtor_events = 0;
    std::uint64_t particle_model_join_hits = 0;
    std::uint64_t particle_model_join_misses = 0;
    std::uint64_t particle_model_owner_join_hits = 0;
    std::uint64_t particle_model_source_primary_join_hits = 0;
    std::uint64_t particle_model_source_secondary_join_hits = 0;
    std::uint64_t particle_state_update_events = 0;
    std::uint64_t backend_key_reads = 0;
    std::uint64_t backend_key_read_failures = 0;
    std::uint64_t waterwave_runtime_index_reads = 0;
    std::uint64_t backend_key_waterwave_matches = 0;
    std::uint64_t backend_semantic_snapshot_hits = 0;
    std::uint64_t backend_semantic_snapshot_misses = 0;
    std::uint64_t waterwave_publish_ok = 0;
    std::uint64_t waterwave_publish_fail = 0;
    std::uint64_t waterwave_same_instance_hits = 0;
    std::uint64_t particle_model_registry_size = 0;
    std::uint64_t snapshot_hits = 0;
    std::uint64_t snapshot_misses = 0;
};

// Diagnostic-only transport at the homologous FXHG draw-entity semantic cut.
//
// PTDE collector vfunc index 10 dispatches through Particle 0x556CF0 /
// Cluster 0x5576D0 and both delegate through entity+0x30 appearance state.
// Retail DSR preserves the same index-10 contract at RVA 0xFFCF30 / 0xFFDCE0
// and the same entity+0x30 appearance-state carrier. DSR Particle appearance
// state has a distinct per-instance owner channel at +0x10 plus seed/source
// links at +0x30/+0x38; Cluster retains source links at +0x50/+0x58 and an
// additional semantic word at +0x60. Static RE does NOT prove that any one of
// these pointers is the FrpgFxParticleAppearance_Model object. The diagnostic
// model hook therefore tests owner/source pointer equality as separately
// attributed runtime hypotheses and records which channel, if any, joins the
// exact live model instance. The exact model destructor is also observed so
// pointer reuse cannot turn a stale registry entry into false identity. No
// channel is promoted to WaterWave authored-MTD authority without a live join.
//
// Particle appearance update RVA 0x118CCF0 provides a second, same-appearance
// diagnostic axis: its key object is dispatched by DWORD key, while DSR's exact
// semantic-index getter RVA 0x1BBC00 maps authored semantic ID 0xE35 to the
// runtime registry index. Their equality is observed and attributed to the same
// appearance pointer, but remains diagnostic evidence only until runtime data
// proves that this dispatcher key is the WaterWave semantic carrier.
//
// This transport authenticates the exact retail executable indirectly through
// the already source-complete FLVER provenance gate, then byte-attests both
// hook sites and exact Particle/Cluster vtables. It observes identity/liveness
// only: it does not authorize WaterWaveSfx, Q8 writes, Bloom, or pixels.
bool install() noexcept;
void uninstall() noexcept;

// Publish an already-authenticated authored WaterWave identity onto the exact
// live FrpgFxParticleAppearance_Model instance. This is intentionally a
// separate handoff: the FX draw transport never infers authored material
// identity from parameter index, shader name, blend mode, or collector token.
bool publish_waterwave_model_identity(
    void *particle_model_instance,
    const operators::postprocess::waterwave_authored_identity &identity) noexcept;

bool snapshot(fx_draw_snapshot &out) noexcept;
void consume() noexcept;

telemetry status() noexcept;
void reset_stats() noexcept;

} // namespace dsrrl::runtime::bloom_fx_draw_transport
