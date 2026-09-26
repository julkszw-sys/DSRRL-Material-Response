#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/a1_create_pipeline_bridge.hpp"
#include "dsrrl/runtime/flver_identity_transport.hpp"
#include "dsrrl/runtime/flver_identity_registry.hpp"
#include "dsrrl/runtime/material_owner_selection.hpp"
#include "dsrrl/runtime/material_response_draw_transaction.hpp"
#include "dsrrl/runtime/material_resource_draw_runtime.hpp"
#include "dsrrl/runtime/envspec_resource_runtime.hpp"
#include "dsrrl/runtime/bloom_scene_sidecar_runtime.hpp"
#include "dsrrl/runtime/bloom_fx_draw_transport.hpp"
#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"
#include "dsrrl/runtime/texture_identity_transport.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/runtime/stable_receiver_pipeline_registry.hpp"
#include "dsrrl/runtime/hemenvlerp_pipeline_registry.hpp"
#include "dsrrl/runtime/subsurface_pipeline_registry.hpp"
#include "dsrrl/runtime/subsurface_draw_runtime.hpp"
#include "dsrrl/runtime/upper_lower_draw_runtime.hpp"
#include "dsrrl/runtime/upper_lower_pipeline_registry.hpp"
#include "dsrrl/runtime/upper_lower_hemenv_draw_runtime.hpp"
#include "dsrrl/runtime/hemdir3_mode_transport.hpp"
#include "dsrrl/runtime/hemdir3_pipeline_registry.hpp"
#include "dsrrl/runtime/hemdir3_draw_runtime.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/material_response_seed.hpp"
#include "dsrrl/operators/material_response/material_response_v211_materializer.hpp"
#include "dsrrl/operators/lightbank/hemdir3_b13_materializer.hpp"
#include "dsrrl/operators/lightbank/upper_lower_hemenv_materializer.hpp"
#include "dsrrl/operators/resource_bridges/spec_rgb_consumer_materializer.hpp"
#include "dsrrl/operators/env_spec/pmetal_rgba_materializer.hpp"
#include "dsrrl/operators/env_spec/pmetal_rgba_lerp_materializer.hpp"

#include <reshade.hpp>
#include <d3d11.h>

#if RESHADE_API_VERSION != 20
#error DSRRL Core+Islands requires ReShade Add-on API 20
#endif

#ifndef DSRRL_CORE_ISLANDS_VERSION
#error DSRRL_CORE_ISLANDS_VERSION must be supplied by integrated CMake
#endif

#ifndef DSRRL_CORE_ISLANDS_PRODUCT_LINE
#error DSRRL_CORE_ISLANDS_PRODUCT_LINE must be supplied by integrated CMake
#endif

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

dsrrl::core::renderer_core g_core;
dsrrl::runtime::a1_create_pipeline_bridge
    g_a1_bridge(g_core.features());
dsrrl::operators::material_response::material_response_island
    g_material_response;
dsrrl::runtime::draw_state_transaction_runtime
    g_draw_transactions(g_core);
dsrrl::runtime::material_response_draw_runtime
    g_mr_draw_runtime(g_draw_transactions);
dsrrl::runtime::material_resource_draw_runtime
    g_material_resources(g_core);
dsrrl::runtime::envspec_resource_runtime
    g_envspec_resources;
dsrrl::runtime::bloom_scene_sidecar_runtime
    g_bloom_scene_sidecar;
dsrrl::runtime::subsurface_draw_runtime
    g_subsurface(g_core, g_mr_draw_runtime, g_material_resources);
dsrrl::runtime::upper_lower_draw_runtime
    g_upper_lower(g_core);
dsrrl::runtime::upper_lower_hemenv_draw_runtime
    g_upper_lower_hemenv(g_core, g_upper_lower);
dsrrl::runtime::hemdir3_draw_runtime
    g_hemdir3(g_core, g_upper_lower);
dsrrl::runtime::pmetal_envspec_draw_runtime
    g_pmetal_envspec(
        g_core,
        g_upper_lower,
        g_envspec_resources,
        g_material_resources);

std::atomic<std::uint64_t> g_present_count{0};
std::atomic<std::uint64_t> g_mr_draw_eval{0};
std::atomic<std::uint64_t> g_mr_would_activate{0};
std::atomic<std::uint64_t> g_mr_fail_open{0};
std::atomic_bool g_mr_ready{false};
std::atomic<std::uint64_t> g_mr_payload_materialize_ok{0};
std::atomic<std::uint64_t> g_mr_payload_materialize_fail{0};
std::atomic<std::uint64_t> g_mr_ul_payload_materialize_ok{0};
std::atomic<std::uint64_t> g_mr_ul_payload_materialize_fail{0};
std::atomic<std::uint64_t> g_envspec_payload_materialize_ok{0};
std::atomic<std::uint64_t> g_envspec_payload_materialize_fail{0};
std::atomic<std::uint64_t> g_draw_events{0};
std::atomic<std::uint64_t> g_draw_receiver_hits{0};
std::atomic<std::uint64_t> g_draw_owner_hits{0};
std::atomic<std::uint64_t> g_draw_joins{0};
std::atomic<std::uint64_t> g_draw_owner_only{0};
std::atomic<std::uint64_t> g_draw_receiver_only{0};
std::atomic<std::uint64_t> g_bloom_fx_draw_snapshots{0};
std::atomic<std::uint64_t> g_bloom_fx_draw_authorized{0};
std::atomic<std::uint64_t> g_bloom_fx_draw_rejected{0};

constexpr dsrrl::core::operator_id k_integrated_islands[] = {
    dsrrl::core::operator_id::material_response,
    dsrrl::core::operator_id::spec_rgb,
    dsrrl::core::operator_id::diffuse,
    dsrrl::core::operator_id::normal,
    dsrrl::core::operator_id::subsurface,
    dsrrl::core::operator_id::upper_lower,
    dsrrl::core::operator_id::hemdir3,
    dsrrl::core::operator_id::env_spec,
    dsrrl::core::operator_id::terminal_sat_rgb,
    dsrrl::core::operator_id::diffuse_material_domain,
    dsrrl::core::operator_id::pointlight_pnts_attenuation,
    dsrrl::core::operator_id::envspec_nospc_delete,
    dsrrl::core::operator_id::fixed_postfog_identity
};

const reshade::api::shader_desc *find_pixel_shader(
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects) noexcept
{
    if (subobjects == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < subobject_count; ++i) {
        if (subobjects[i].type !=
                reshade::api::pipeline_subobject_type::pixel_shader ||
            subobjects[i].count != 1u ||
            subobjects[i].data == nullptr)
            continue;

        return static_cast<const reshade::api::shader_desc *>(
            subobjects[i].data);
    }

    return nullptr;
}

bool observe_draw_identity(
    reshade::api::command_list *cmd_list,
    std::uint32_t &receiver_id,
    bool &hemenvlerp_bound,
    dsrrl::runtime::hemenvlerp_receiver_identity &hemenvlerp_identity,
    bool &subsurface_bound,
    bool &hemdir3_bound,
    dsrrl::runtime::hemdir3_receiver_identity &hemdir3_identity,
    bool &upper_lower_bound,
    dsrrl::runtime::upper_lower_receiver_identity &upper_lower_identity,
    dsrrl::operators::material_response::material_identity &out_material,
    dsrrl::operators::material_response::decision &out_decision) noexcept
{
    ++g_draw_events;

    receiver_id = 0u;
    hemenvlerp_bound = false;
    hemenvlerp_identity = {};
    subsurface_bound = false;
    hemdir3_bound = false;
    hemdir3_identity = {};
    upper_lower_bound = false;
    upper_lower_identity = {};

    const bool stable_receiver =
        dsrrl::runtime::stable_receiver_bound(
            cmd_list,
            receiver_id);

    const bool hemenvlerp_receiver =
        dsrrl::runtime::hemenvlerp_receiver_bound(
            cmd_list,
            hemenvlerp_identity);

    std::uint32_t subsurface_target = 0u;
    const bool subsurface_receiver =
        dsrrl::runtime::subsurface_receiver_bound(
            cmd_list,
            subsurface_target);

    const bool hemdir3_receiver =
        dsrrl::runtime::hemdir3_receiver_bound(
            cmd_list,
            hemdir3_identity);

    const bool upper_lower_receiver =
        dsrrl::runtime::upper_lower_receiver_bound(
            cmd_list,
            upper_lower_identity);

    const bool upper_lower_spc =
        upper_lower_receiver &&
        upper_lower_identity.stratum ==
            dsrrl::operators::lightbank::
                upper_lower_hemenv_stratum::spc;

    const bool upper_lower_nospc =
        upper_lower_receiver &&
        upper_lower_identity.stratum ==
            dsrrl::operators::lightbank::
                upper_lower_hemenv_stratum::nospc;

    const bool upper_lower_parallax =
        upper_lower_receiver &&
        (upper_lower_identity.family ==
             dsrrl::operators::lightbank::
                 upper_lower_hemenv_family::hemenv_parallax ||
         upper_lower_identity.family ==
             dsrrl::operators::lightbank::
                 upper_lower_hemenv_family::hemenvlerp_parallax);

    const bool upper_lower_spc_matches_stable =
        !upper_lower_spc ||
        upper_lower_parallax ||
        (upper_lower_identity.family ==
                 dsrrl::operators::lightbank::
                     upper_lower_hemenv_family::hemenv
             ? (stable_receiver &&
                receiver_id ==
                    upper_lower_identity.stable_receiver_id)
             : (hemenvlerp_receiver &&
                hemenvlerp_identity.semantic_receiver_id ==
                    upper_lower_identity.stable_receiver_id));

    const bool upper_lower_unpaired_nospc =
        upper_lower_nospc &&
        !upper_lower_parallax;

    const unsigned receiver_classes =
        (stable_receiver ? 1u : 0u) +
        (hemenvlerp_receiver ? 1u : 0u) +
        (subsurface_receiver ? 1u : 0u) +
        (hemdir3_receiver ? 1u : 0u) +
        (upper_lower_unpaired_nospc ? 1u : 0u) +
        (upper_lower_parallax ? 1u : 0u);

    const bool receiver_ok =
        receiver_classes == 1u &&
        upper_lower_spc_matches_stable;

    if (upper_lower_parallax) {
        // Exact Parallax executable identities are U/L-only here. Do not
        // borrow stable HemEnv/HemEnvLerp receiver IDs into MR, resources or
        // EnvSpec: the U/L consumer cut is independently certified by SHA.
        receiver_id = 0u;
        upper_lower_bound = true;
    } else if (hemenvlerp_receiver) {
        receiver_id =
            hemenvlerp_identity.semantic_receiver_id;
        hemenvlerp_bound = true;
        if (upper_lower_spc &&
            upper_lower_identity.family ==
                dsrrl::operators::lightbank::
                    upper_lower_hemenv_family::hemenvlerp)
            upper_lower_bound = true;
    } else if (subsurface_receiver) {
        receiver_id = subsurface_target;
        subsurface_bound = true;
    } else if (hemdir3_receiver) {
        // HemDir3 has its own exact receiver namespace. no-Spc deliberately
        // has no paired stable HemEnv receiver, and Spc pairing is metadata
        // only; never leak either into generic HemEnv routing.
        hemdir3_bound = true;
    } else if (upper_lower_nospc) {
        // U/L no-Spc has an exact HemEnv consumer identity but no stable
        // Material Response receiver. Keep generic receiver_id unset.
        receiver_id = 0u;
        upper_lower_bound = true;
    } else if (upper_lower_spc) {
        upper_lower_bound = true;
    }

    out_material = {};
    out_decision = {};

    const bool owner_ok =
        dsrrl::runtime::material_owner_selection_consume(
            out_material);

    if (receiver_ok)
        ++g_draw_receiver_hits;
    if (owner_ok)
        ++g_draw_owner_hits;

    if (!receiver_ok) {
        if (owner_ok)
            ++g_draw_owner_only;
        ++g_mr_fail_open;
        return false;
    }

    if (!owner_ok) {
        ++g_draw_receiver_only;
        ++g_mr_fail_open;
        return true;
    }

    ++g_draw_joins;

    if (subsurface_bound ||
        hemdir3_bound) {
        ++g_mr_fail_open;
        return true;
    }

    if (!g_mr_ready.load()) {
        ++g_mr_fail_open;
        return true;
    }

    ++g_mr_draw_eval;
    out_decision =
        g_material_response.evaluate(
            receiver_id,
            out_material);

    if (out_decision.active)
        ++g_mr_would_activate;
    else
        ++g_mr_fail_open;

    return true;
}

bool enable_integrated_islands() noexcept
{
    for (const auto op : k_integrated_islands)
        if (!g_core.features().set(op, true))
            return false;

    return true;
}

void disable_integrated_islands() noexcept
{
    for (const auto op : k_integrated_islands)
        (void)g_core.features().set(op, false);
}

void log_state(const char *tag) noexcept
{
    const auto t = g_a1_bridge.telemetry();
    const auto f = dsrrl::runtime::flver_identity_stats();
    const auto h = dsrrl::runtime::flver_identity_transport::status();
    const auto m = dsrrl::runtime::material_owner_selection_stats();
    const auto mr_tx = g_mr_draw_runtime.telemetry();
    const auto tx = g_draw_transactions.telemetry();
    const auto resources = g_material_resources.telemetry();
    const auto tex = dsrrl::runtime::texture_identity_transport::status();
    const auto ul = g_upper_lower.telemetry();
    const auto subs = g_subsurface.telemetry();
    const auto mode =
        dsrrl::runtime::hemdir3_mode_transport::status();

    char line[1984]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s "
        "create=%llu candidate=%llu exact=%llu materialized=%llu "
        "unknown=%llu no_owner=%llu failopen=%llu init_ok=%llu "
        "init_bad=%llu binds=%llu quarantine=%u "
        "flver_hook=%u/%u/%u prov=%u owner_enrich=%u restore_fail=%u "
        "inserts=%llu lookups=%llu hits=%llu misses=%llu erases=%llu invalid=%llu "
        "owner_sel=%llu owner_enriched=%llu owner_auth=%llu owner_fo=%llu "
        "mr_ready=%u mr_eval=%llu mr_would_activate=%llu mr_fo=%llu "
        "mr_mat_ok=%llu mr_mat_fail=%llu mr_payload_ok=%llu mr_payload_fail=%llu "
        "mr_ul_payload=%llu/%llu mr_ul_reg=%llu/%llu mr_ul_prepare=%llu mr_ul_miss=%llu "
        "mr_b12_create=%llu mr_b12_hit=%llu mr_b12_bind_fail=%llu "
        "mr_tx_eligible=%llu mr_tx_miss=%llu mr_replay=%llu mr_restore_fail=%llu mr_quarantine=%u "
        "tx_begin_ok=%llu tx_begin_fail=%llu tx_bind_fail=%llu tx_issued=%llu "
        "tx_restore_ok=%llu tx_restore_fail=%llu tx_quarantine=%u "
        "tex_hook=%u/%u tex_restore_fail=%u res_named=%llu res_ready=%llu res_missing=%llu "
        "res_unsupported=%llu spec_req=%llu diff_req=%llu norm_req=%llu res_fo=%llu "
        "ul_hook=%u ul_q=%u ul_restore_fail=%u ul_pub=%llu ul_sel=%llu/%llu/%llu ul_tuple_miss=%llu "
        "ul_steady=%llu/%llu ul_blend=%llu/%llu/%llu ul_b13=%llu/%llu ul_req=%llu "
        "sub_candidate=%llu sub_prepared=%llu sub_pipe_reject=%llu sub_mat_reject=%llu sub_surface_reject=%llu "
        "mode_hook=%u/%u mode_q=%u mode_restore_fail=%u mode_begin=%llu mode_in2=%llu mode_obs=%llu mode2=%llu mode_snap=%llu/%llu "
        "draw=%llu draw_rx=%llu draw_owner=%llu draw_join=%llu owner_only=%llu rx_only=%llu",
        tag,
        static_cast<unsigned long long>(t.create_events),
        static_cast<unsigned long long>(t.candidate_size_hits),
        static_cast<unsigned long long>(t.exact_identity_hits),
        static_cast<unsigned long long>(t.materialized),
        static_cast<unsigned long long>(t.pass_unknown_identity),
        static_cast<unsigned long long>(t.pass_no_enabled_owner),
        static_cast<unsigned long long>(t.fail_open),
        static_cast<unsigned long long>(t.init_attested),
        static_cast<unsigned long long>(t.init_mismatch),
        static_cast<unsigned long long>(t.target_binds),
        t.quarantined ? 1u : 0u,
        h.parser_armed ? 1u : 0u,
        h.selector_armed ? 1u : 0u,
        h.destructor_armed ? 1u : 0u,
        h.provenance_ok ? 1u : 0u,
        h.selector_owner_enrichment ? 1u : 0u,
        h.restore_failed ? 1u : 0u,
        static_cast<unsigned long long>(f.inserts),
        static_cast<unsigned long long>(f.lookups),
        static_cast<unsigned long long>(f.hits),
        static_cast<unsigned long long>(f.misses),
        static_cast<unsigned long long>(f.erases),
        static_cast<unsigned long long>(f.invalid_raw),
        static_cast<unsigned long long>(m.selector_events),
        static_cast<unsigned long long>(m.owner_enriched),
        static_cast<unsigned long long>(m.owner_authenticated),
        static_cast<unsigned long long>(m.fail_open),
        g_mr_ready.load() ? 1u : 0u,
        static_cast<unsigned long long>(g_mr_draw_eval.load()),
        static_cast<unsigned long long>(g_mr_would_activate.load()),
        static_cast<unsigned long long>(g_mr_fail_open.load()),
        static_cast<unsigned long long>(g_mr_payload_materialize_ok.load()),
        static_cast<unsigned long long>(g_mr_payload_materialize_fail.load()),
        static_cast<unsigned long long>(g_mr_ul_payload_materialize_ok.load()),
        static_cast<unsigned long long>(g_mr_ul_payload_materialize_fail.load()),
        static_cast<unsigned long long>(mr_tx.combined_ul_register_ok),
        static_cast<unsigned long long>(mr_tx.combined_ul_register_fail),
        static_cast<unsigned long long>(mr_tx.combined_ul_prepare),
        static_cast<unsigned long long>(mr_tx.combined_ul_miss),
        static_cast<unsigned long long>(mr_tx.replacement_register_ok),
        static_cast<unsigned long long>(mr_tx.replacement_register_fail),
        static_cast<unsigned long long>(mr_tx.b12_create),
        static_cast<unsigned long long>(mr_tx.b12_hit),
        static_cast<unsigned long long>(mr_tx.b12_bind_fail),
        static_cast<unsigned long long>(mr_tx.eligible_draws),
        static_cast<unsigned long long>(mr_tx.replacement_miss),
        static_cast<unsigned long long>(mr_tx.replay_ok),
        static_cast<unsigned long long>(mr_tx.replay_restore_fail),
        mr_tx.quarantined ? 1u : 0u,
        static_cast<unsigned long long>(tx.begin_ok),
        static_cast<unsigned long long>(tx.begin_fail),
        static_cast<unsigned long long>(tx.bind_fail),
        static_cast<unsigned long long>(tx.draws_issued),
        static_cast<unsigned long long>(tx.restore_ok),
        static_cast<unsigned long long>(tx.restore_fail),
        tx.quarantined ? 1u : 0u,
        tex.name_hook_armed ? 1u : 0u,
        tex.clear_hook_armed ? 1u : 0u,
        tex.restore_failed ? 1u : 0u,
        static_cast<unsigned long long>(resources.named_views),
        static_cast<unsigned long long>(resources.sidecar_ready),
        static_cast<unsigned long long>(resources.sidecar_missing),
        static_cast<unsigned long long>(resources.sidecar_unsupported),
        static_cast<unsigned long long>(resources.spec_requests),
        static_cast<unsigned long long>(resources.diffuse_requests),
        static_cast<unsigned long long>(resources.normal_requests),
        static_cast<unsigned long long>(resources.fail_open),
        ul.producer_hooks_armed ? 1u : 0u,
        ul.quarantined ? 1u : 0u,
        ul.restore_failed ? 1u : 0u,
        static_cast<unsigned long long>(ul.snapshot_publish),
        static_cast<unsigned long long>(ul.selector_seen),
        static_cast<unsigned long long>(ul.selector_match),
        static_cast<unsigned long long>(ul.selector_miss),
        static_cast<unsigned long long>(ul.tuple_mismatch),
        static_cast<unsigned long long>(ul.steady_seen),
        static_cast<unsigned long long>(ul.steady_pass),
        static_cast<unsigned long long>(ul.blend_seen),
        static_cast<unsigned long long>(ul.blend_upper),
        static_cast<unsigned long long>(ul.blend_lower),
        static_cast<unsigned long long>(ul.b13_create),
        static_cast<unsigned long long>(ul.b13_hit),
        static_cast<unsigned long long>(ul.requests),
        static_cast<unsigned long long>(subs.candidates),
        static_cast<unsigned long long>(subs.prepared),
        static_cast<unsigned long long>(subs.pipeline_rejects),
        static_cast<unsigned long long>(subs.material_rejects),
        static_cast<unsigned long long>(subs.surface_rejects),
        mode.lt5_hook_armed ? 1u : 0u,
        mode.selector_end_hook_armed ? 1u : 0u,
        mode.quarantined ? 1u : 0u,
        mode.restore_failed ? 1u : 0u,
        static_cast<unsigned long long>(mode.selector_begin),
        static_cast<unsigned long long>(mode.incoming_mode2),
        static_cast<unsigned long long>(mode.effective_observed),
        static_cast<unsigned long long>(mode.mode2_observed),
        static_cast<unsigned long long>(mode.snapshot_hits),
        static_cast<unsigned long long>(mode.snapshot_misses),
        static_cast<unsigned long long>(g_draw_events.load()),
        static_cast<unsigned long long>(g_draw_receiver_hits.load()),
        static_cast<unsigned long long>(g_draw_owner_hits.load()),
        static_cast<unsigned long long>(g_draw_joins.load()),
        static_cast<unsigned long long>(g_draw_owner_only.load()),
        static_cast<unsigned long long>(g_draw_receiver_only.load()));

    reshade::log::message(reshade::log::level::info, line);

    const auto ul_pipe =
        dsrrl::runtime::upper_lower_receiver_pipeline_stats();
    const auto ul_draw =
        g_upper_lower_hemenv.telemetry();

    char ul_line[768]{};
    std::snprintf(
        ul_line,
        sizeof(ul_line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s_UL "
        "pipe_attest=%llu pipe_conflict=%llu pipe_init=%llu exact_nospc=%llu exact_spc=%llu "
        "init_bad=%llu binds=%llu/%llu/%llu unknown=%llu lookup=%llu/%llu/%llu q=%u "
        "repl=%llu/%llu candidate=%llu carrier=%llu/%llu nospc_ready=%llu spc_ready=%llu "
        "spc_mr_hold=%llu req=%llu ul_q=%u",
        tag,
        static_cast<unsigned long long>(ul_pipe.created_code_attested),
        static_cast<unsigned long long>(ul_pipe.created_code_conflict),
        static_cast<unsigned long long>(ul_pipe.pipeline_inits),
        static_cast<unsigned long long>(ul_pipe.exact_nospc_hits),
        static_cast<unsigned long long>(ul_pipe.exact_spc_hits),
        static_cast<unsigned long long>(ul_pipe.init_mismatch),
        static_cast<unsigned long long>(ul_pipe.pixel_binds),
        static_cast<unsigned long long>(ul_pipe.nospc_binds),
        static_cast<unsigned long long>(ul_pipe.spc_binds),
        static_cast<unsigned long long>(ul_pipe.unknown_binds),
        static_cast<unsigned long long>(ul_pipe.lookups),
        static_cast<unsigned long long>(ul_pipe.lookup_hits),
        static_cast<unsigned long long>(ul_pipe.lookup_misses),
        ul_pipe.quarantined ? 1u : 0u,
        static_cast<unsigned long long>(ul_draw.replacement_register_ok),
        static_cast<unsigned long long>(ul_draw.replacement_register_fail),
        static_cast<unsigned long long>(ul_draw.candidates),
        static_cast<unsigned long long>(ul_draw.carrier_ready),
        static_cast<unsigned long long>(ul_draw.carrier_rejects),
        static_cast<unsigned long long>(ul_draw.nospc_ready),
        static_cast<unsigned long long>(ul_draw.spc_ready),
        static_cast<unsigned long long>(ul_draw.spc_mr_hold),
        static_cast<unsigned long long>(ul_draw.requests),
        ul_draw.quarantined ? 1u : 0u);

    reshade::log::message(
        reshade::log::level::info,
        ul_line);

    const auto h3_pipe =
        dsrrl::runtime::hemdir3_receiver_pipeline_stats();
    const auto h3_draw =
        g_hemdir3.telemetry();

    char h3_line[1024]{};
    std::snprintf(
        h3_line,
        sizeof(h3_line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s_H3 "
        "pipe_attest=%llu pipe_conflict=%llu pipe_init=%llu exact_nospc=%llu exact_spc=%llu "
        "init_bad=%llu binds=%llu/%llu/%llu unknown=%llu lookup=%llu/%llu/%llu q=%u "
        "repl=%llu/%llu candidate=%llu mode2=%llu mode_reject=%llu carrier=%llu/%llu "
        "nospc_ready=%llu spc_ready=%llu donor=%llu/%llu b12=%llu/%llu spc_hold=%llu "
        "readiness_reject=%llu req=%llu h3_q=%u "
        "d123_steady=%llu d123_blend_dir=%llu d123_blend_col=%llu d123_publish=%llu "
        "h3_b13=%llu/%llu h3_carrier_req=%llu",
        tag,
        static_cast<unsigned long long>(h3_pipe.created_code_attested),
        static_cast<unsigned long long>(h3_pipe.created_code_conflict),
        static_cast<unsigned long long>(h3_pipe.pipeline_inits),
        static_cast<unsigned long long>(h3_pipe.exact_nospc_hits),
        static_cast<unsigned long long>(h3_pipe.exact_spc_hits),
        static_cast<unsigned long long>(h3_pipe.init_mismatch),
        static_cast<unsigned long long>(h3_pipe.pixel_binds),
        static_cast<unsigned long long>(h3_pipe.nospc_binds),
        static_cast<unsigned long long>(h3_pipe.spc_binds),
        static_cast<unsigned long long>(h3_pipe.unknown_binds),
        static_cast<unsigned long long>(h3_pipe.lookups),
        static_cast<unsigned long long>(h3_pipe.lookup_hits),
        static_cast<unsigned long long>(h3_pipe.lookup_misses),
        h3_pipe.quarantined ? 1u : 0u,
        static_cast<unsigned long long>(h3_draw.replacement_register_ok),
        static_cast<unsigned long long>(h3_draw.replacement_register_fail),
        static_cast<unsigned long long>(h3_draw.candidates),
        static_cast<unsigned long long>(h3_draw.mode2_hits),
        static_cast<unsigned long long>(h3_draw.mode_rejects),
        static_cast<unsigned long long>(h3_draw.carrier_ready),
        static_cast<unsigned long long>(h3_draw.carrier_rejects),
        static_cast<unsigned long long>(h3_draw.nospc_ready),
        static_cast<unsigned long long>(h3_draw.spc_ready),
        static_cast<unsigned long long>(h3_draw.spc_donor_hit),
        static_cast<unsigned long long>(h3_draw.spc_donor_miss),
        static_cast<unsigned long long>(h3_draw.spc_b12_create),
        static_cast<unsigned long long>(h3_draw.spc_b12_hit),
        static_cast<unsigned long long>(h3_draw.spc_b12_hold),
        static_cast<unsigned long long>(h3_draw.readiness_rejects),
        static_cast<unsigned long long>(h3_draw.requests),
        h3_draw.quarantined ? 1u : 0u,
        static_cast<unsigned long long>(ul.d123_steady),
        static_cast<unsigned long long>(ul.d123_blend_direction),
        static_cast<unsigned long long>(ul.d123_blend_color),
        static_cast<unsigned long long>(ul.d123_snapshot_publish),
        static_cast<unsigned long long>(ul.hemdir3_b13_create),
        static_cast<unsigned long long>(ul.hemdir3_b13_hit),
        static_cast<unsigned long long>(ul.hemdir3_carrier_requests));

    reshade::log::message(
        reshade::log::level::info,
        h3_line);

    const auto env_res =
        g_envspec_resources.telemetry();
    const auto env_draw =
        g_pmetal_envspec.telemetry();
    const auto env_lerp =
        dsrrl::runtime::
            hemenvlerp_receiver_pipeline_stats();

    char env_line[1280]{};
    std::snprintf(
        env_line,
        sizeof(env_line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s_ENVSPEC "
        "ps_mat=%llu/%llu src_steady=%llu src_blend=%llu src_miss=%llu src_hook=%u "
        "native=%llu/%llu hash_miss=%llu views=%llu pack=%llu/%llu pack_ready=%u sampler=%u "
        "cube=%llu/%llu prepare=%llu/%llu candidate=%llu material_reject=%llu semantic_reject=%llu "
        "source_reject=%llu blend_hold=%llu probe_reject=%llu spec_reject=%llu ul=%llu/%llu req=%llu q=%u "
        "lerp_reg=%llu/%llu lerp_candidate=%llu lerp_req=%llu lerp_pipe=%llu/%llu bind=%llu/%llu miss=%llu conflict=%llu",
        tag,
        static_cast<unsigned long long>(
            g_envspec_payload_materialize_ok.load()),
        static_cast<unsigned long long>(
            g_envspec_payload_materialize_fail.load()),
        static_cast<unsigned long long>(
            ul.pmetal_env_steady),
        static_cast<unsigned long long>(
            ul.pmetal_env_blend),
        static_cast<unsigned long long>(
            ul.pmetal_env_miss),
        ul.pmetal_env_hook_armed ? 1u : 0u,
        static_cast<unsigned long long>(
            env_res.native_candidates),
        static_cast<unsigned long long>(
            env_res.native_matches),
        static_cast<unsigned long long>(
            env_res.native_hash_miss),
        static_cast<unsigned long long>(
            env_res.view_matches),
        static_cast<unsigned long long>(
            env_res.pack_admit_ok),
        static_cast<unsigned long long>(
            env_res.pack_admit_fail),
        env_res.pack_ready ? 1u : 0u,
        env_res.sampler_ready ? 1u : 0u,
        static_cast<unsigned long long>(
            env_res.cube_created),
        static_cast<unsigned long long>(
            env_res.cube_fail),
        static_cast<unsigned long long>(
            env_res.prepare_ok),
        static_cast<unsigned long long>(
            env_res.prepare_fail),
        static_cast<unsigned long long>(
            env_draw.candidates),
        static_cast<unsigned long long>(
            env_draw.material_rejects),
        static_cast<unsigned long long>(
            env_draw.semantic_rejects),
        static_cast<unsigned long long>(
            env_draw.source_rejects),
        static_cast<unsigned long long>(
            env_draw.blended_receiver_hold),
        static_cast<unsigned long long>(
            env_draw.probe_rejects),
        static_cast<unsigned long long>(
            env_draw.spec_rgb_rejects),
        static_cast<unsigned long long>(
            env_draw.upper_lower_ready),
        static_cast<unsigned long long>(
            env_draw.upper_lower_fallback),
        static_cast<unsigned long long>(
            env_draw.requests),
        env_draw.quarantined ? 1u : 0u,
        static_cast<unsigned long long>(
            env_draw.lerp_replacement_register_ok),
        static_cast<unsigned long long>(
            env_draw.lerp_replacement_register_fail),
        static_cast<unsigned long long>(
            env_draw.lerp_candidates),
        static_cast<unsigned long long>(
            env_draw.lerp_requests),
        static_cast<unsigned long long>(
            env_lerp.exact_hits),
        static_cast<unsigned long long>(
            env_lerp.hash_misses),
        static_cast<unsigned long long>(
            env_lerp.exact_binds),
        static_cast<unsigned long long>(
            env_lerp.unknown_binds),
        static_cast<unsigned long long>(
            env_lerp.lookup_misses),
        static_cast<unsigned long long>(
            env_lerp.handle_conflicts));

    reshade::log::message(
        reshade::log::level::info,
        env_line);

    const auto bloom_q8 =
        g_bloom_scene_sidecar.telemetry();

    char bloom_line[640]{};
    std::snprintf(
        bloom_line,
        sizeof(bloom_line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s_BLOOM_Q8 "
        "init=%llu create=%llu/%llu ready=%u auth=%llu/%llu armed=%u "
        "begin=%llu/%llu commit=%llu/%llu rtv=%llu srv=%llu acquire_fail=%llu "
        "valid=%u frame=%llu destroy=%llu",
        tag,
        static_cast<unsigned long long>(bloom_q8.init_calls),
        static_cast<unsigned long long>(bloom_q8.create_ok),
        static_cast<unsigned long long>(bloom_q8.create_fail),
        bloom_q8.resource_ready ? 1u : 0u,
        static_cast<unsigned long long>(bloom_q8.authorize_ok),
        static_cast<unsigned long long>(bloom_q8.authorize_fail),
        bloom_q8.proof_authorized ? 1u : 0u,
        static_cast<unsigned long long>(bloom_q8.begin_ok),
        static_cast<unsigned long long>(bloom_q8.begin_fail),
        static_cast<unsigned long long>(bloom_q8.commit_ok),
        static_cast<unsigned long long>(bloom_q8.commit_fail),
        static_cast<unsigned long long>(bloom_q8.rtv_acquire_ok),
        static_cast<unsigned long long>(bloom_q8.srv_acquire_ok),
        static_cast<unsigned long long>(bloom_q8.acquire_fail),
        bloom_q8.contents_valid ? 1u : 0u,
        static_cast<unsigned long long>(bloom_q8.frame_serial),
        static_cast<unsigned long long>(bloom_q8.destroy_calls));

    reshade::log::message(
        reshade::log::level::info,
        bloom_line);

    const auto bloom_fx =
        dsrrl::runtime::bloom_fx_draw_transport::status();

    char bloom_fx_line[640]{};
    std::snprintf(
        bloom_fx_line,
        sizeof(bloom_fx_line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s_BLOOM_FX "
        "prov=%u hooks=%u/%u model=%u/%u q=%u restore_fail=%u "
        "events=%llu/%llu exact=%llu reject=%llu state=%llu/%llu "
        "state_exact=%llu/%llu links=%llu/%llu model_evt=%llu/%llu "
        "model_join=%llu/%llu join_ch=%llu/%llu/%llu "
        "ww_publish=%llu/%llu ww_same=%llu registry=%llu "
        "snap=%llu/%llu api_snap=%llu ww_auth=%llu/%llu",
        tag,
        bloom_fx.provenance_ok ? 1u : 0u,
        bloom_fx.particle_hook_armed ? 1u : 0u,
        bloom_fx.cluster_hook_armed ? 1u : 0u,
        bloom_fx.particle_model_ctor_hook_armed ? 1u : 0u,
        bloom_fx.particle_model_dtor_hook_armed ? 1u : 0u,
        bloom_fx.quarantined ? 1u : 0u,
        bloom_fx.restore_failed ? 1u : 0u,
        static_cast<unsigned long long>(bloom_fx.particle_events),
        static_cast<unsigned long long>(bloom_fx.cluster_events),
        static_cast<unsigned long long>(bloom_fx.exact_entity_hits),
        static_cast<unsigned long long>(bloom_fx.entity_rejects),
        static_cast<unsigned long long>(bloom_fx.state_ready_hits),
        static_cast<unsigned long long>(bloom_fx.state_missing),
        static_cast<unsigned long long>(bloom_fx.exact_state_hits),
        static_cast<unsigned long long>(bloom_fx.state_vtable_rejects),
        static_cast<unsigned long long>(bloom_fx.source_links_ready),
        static_cast<unsigned long long>(bloom_fx.source_links_missing),
        static_cast<unsigned long long>(bloom_fx.particle_model_ctor_events),
        static_cast<unsigned long long>(bloom_fx.particle_model_dtor_events),
        static_cast<unsigned long long>(bloom_fx.particle_model_join_hits),
        static_cast<unsigned long long>(bloom_fx.particle_model_join_misses),
        static_cast<unsigned long long>(bloom_fx.particle_model_owner_join_hits),
        static_cast<unsigned long long>(bloom_fx.particle_model_source_primary_join_hits),
        static_cast<unsigned long long>(bloom_fx.particle_model_source_secondary_join_hits),
        static_cast<unsigned long long>(bloom_fx.waterwave_publish_ok),
        static_cast<unsigned long long>(bloom_fx.waterwave_publish_fail),
        static_cast<unsigned long long>(bloom_fx.waterwave_same_instance_hits),
        static_cast<unsigned long long>(bloom_fx.particle_model_registry_size),
        static_cast<unsigned long long>(bloom_fx.snapshot_hits),
        static_cast<unsigned long long>(bloom_fx.snapshot_misses),
        static_cast<unsigned long long>(g_bloom_fx_draw_snapshots.load()),
        static_cast<unsigned long long>(g_bloom_fx_draw_authorized.load()),
        static_cast<unsigned long long>(g_bloom_fx_draw_rejected.load()));

    reshade::log::message(
        reshade::log::level::info,
        bloom_fx_line);
}

void on_init_device(reshade::api::device *device)
{
    g_bloom_scene_sidecar.on_init_device(device);
    g_a1_bridge.on_init_device(device);
    g_mr_draw_runtime.on_init_device(device);
    g_pmetal_envspec.on_init_device(device);
    g_upper_lower_hemenv.on_init_device(device);
    g_hemdir3.on_init_device(device);
}

void on_destroy_device(reshade::api::device *device)
{
    g_bloom_scene_sidecar.on_destroy_device(device);
    g_upper_lower.on_destroy_device(device);
    g_upper_lower_hemenv.on_destroy_device(device);
    g_hemdir3.on_destroy_device(device);
    g_pmetal_envspec.on_destroy_device(device);
    g_mr_draw_runtime.on_destroy_device(device);
    g_a1_bridge.on_destroy_device(device);
}

bool on_create_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline_layout layout,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects)
{
    const auto *pixel_shader =
        find_pixel_shader(
            subobject_count,
            subobjects);

    dsrrl::operators::lightbank::
        hemdir3_b13_materialize_outcome h3{};
    dsrrl::operators::lightbank::
        upper_lower_hemenv_materialize_outcome ul{};
    bool h3_identity_ready = false;
    bool h3_replacement_ready = false;
    bool ul_identity_ready = false;
    bool ul_replacement_ready = false;

    if (pixel_shader != nullptr &&
        pixel_shader->code != nullptr &&
        pixel_shader->code_size != 0u) {
        const auto *source =
            static_cast<const std::uint8_t *>(
                pixel_shader->code);

        std::vector<std::uint8_t> mr_payload;
        const auto mr =
            dsrrl::operators::material_response::
                materialize_v211_stable_receiver(
                    g_core.features(),
                    source,
                    pixel_shader->code_size,
                    mr_payload);

        using mr_result =
            dsrrl::operators::material_response::
                v211_materialize_result;

        if (mr.result == mr_result::applied) {
            const auto spec_owner =
                dsrrl::core::operator_bit(
                    dsrrl::core::operator_id::spec_rgb);

            std::vector<std::uint8_t> mr_spec_payload;
            const auto spec_result =
                dsrrl::operators::resource_bridges::
                    materialize_spec_rgb_consumer(
                        mr_payload.data(),
                        mr_payload.size(),
                        mr_spec_payload);

            if (spec_result ==
                dsrrl::operators::resource_bridges::
                    spec_rgb_consumer_result::applied) {
                if (!g_mr_draw_runtime.has_receiver_replacement(
                        mr.receiver_id)) {
                    if (g_mr_draw_runtime.register_receiver_replacement(
                            mr.receiver_id,
                            mr_spec_payload.data(),
                            mr_spec_payload.size(),
                            mr.composed_owners |
                                spec_owner))
                        ++g_mr_payload_materialize_ok;
                    else
                        ++g_mr_payload_materialize_fail;
                }
            } else {
                ++g_mr_payload_materialize_fail;
            }

            std::vector<std::uint8_t> mr_ul_payload;
            const auto mr_ul =
                dsrrl::operators::lightbank::
                    augment_upper_lower_hemenv_verified_base(
                        source,
                        pixel_shader->code_size,
                        mr_payload.data(),
                        mr_payload.size(),
                        4u,
                        mr_ul_payload);

            if (mr_ul.result ==
                    dsrrl::operators::lightbank::
                        upper_lower_hemenv_materialize_result::applied &&
                mr_ul.stratum ==
                    dsrrl::operators::lightbank::
                        upper_lower_hemenv_stratum::spc &&
                mr_ul.stable_receiver_id ==
                    mr.receiver_id) {
                std::vector<std::uint8_t> mr_ul_spec_payload;
                const auto ul_spec_result =
                    dsrrl::operators::resource_bridges::
                        materialize_spec_rgb_consumer(
                            mr_ul_payload.data(),
                            mr_ul_payload.size(),
                            mr_ul_spec_payload);

                if (ul_spec_result ==
                    dsrrl::operators::resource_bridges::
                        spec_rgb_consumer_result::applied) {
                    if (!g_mr_draw_runtime.
                            has_receiver_upper_lower_replacement(
                                mr.receiver_id)) {
                        if (g_mr_draw_runtime.
                                register_receiver_upper_lower_replacement(
                                    mr.receiver_id,
                                    mr_ul_spec_payload.data(),
                                    mr_ul_spec_payload.size(),
                                    mr.composed_owners |
                                        spec_owner))
                            ++g_mr_ul_payload_materialize_ok;
                        else
                            ++g_mr_ul_payload_materialize_fail;
                    }
                } else {
                    ++g_mr_ul_payload_materialize_fail;
                }
            } else if (
                mr_ul.result !=
                    dsrrl::operators::lightbank::
                        upper_lower_hemenv_materialize_result::
                            pass_not_candidate &&
                mr_ul.result !=
                    dsrrl::operators::lightbank::
                        upper_lower_hemenv_materialize_result::
                            pass_unknown_exact_sha) {
                ++g_mr_ul_payload_materialize_fail;
            }
        } else if (
            mr.result != mr_result::pass_not_candidate &&
            mr.result != mr_result::pass_unknown_exact_sha) {
            ++g_mr_payload_materialize_fail;
        }

        if (g_core.features().enabled(
                dsrrl::core::operator_id::env_spec)) {
            for (const bool with_upper_lower :
                 {false, true}) {
                std::vector<std::uint8_t>
                    envspec_payload;

                const auto envspec =
                    dsrrl::operators::env_spec::
                        materialize_pmetal_rgba_receiver(
                            g_core.features(),
                            source,
                            pixel_shader->code_size,
                            with_upper_lower,
                            envspec_payload);

                using envspec_result =
                    dsrrl::operators::env_spec::
                        pmetal_rgba_materialize_result;

                if (envspec.result ==
                    envspec_result::applied) {
                    if (g_pmetal_envspec.
                            register_replacement(
                                envspec,
                                envspec_payload.data(),
                                envspec_payload.size()))
                        ++g_envspec_payload_materialize_ok;
                    else
                        ++g_envspec_payload_materialize_fail;
                } else if (
                    envspec.result !=
                        envspec_result::pass_not_candidate &&
                    envspec.result !=
                        envspec_result::pass_unknown_exact_sha) {
                    ++g_envspec_payload_materialize_fail;
                }
            }
        }

        if (g_core.features().enabled(
                dsrrl::core::operator_id::env_spec)) {
            std::vector<std::uint8_t> envspec_lerp_payload;
            const auto envspec_lerp =
                dsrrl::operators::env_spec::
                    materialize_pmetal_rgba_lerp_receiver(
                        source,
                        pixel_shader->code_size,
                        envspec_lerp_payload);

            using envspec_lerp_result =
                dsrrl::operators::env_spec::
                    pmetal_rgba_lerp_materialize_result;

            if (envspec_lerp.result ==
                    envspec_lerp_result::applied) {
                if (g_pmetal_envspec.register_lerp_replacement(
                        envspec_lerp,
                        envspec_lerp_payload.data(),
                        envspec_lerp_payload.size()))
                    ++g_envspec_payload_materialize_ok;
                else
                    ++g_envspec_payload_materialize_fail;
            } else if (
                envspec_lerp.result !=
                    envspec_lerp_result::pass_not_candidate &&
                envspec_lerp.result !=
                    envspec_lerp_result::pass_unknown_exact_sha) {
                ++g_envspec_payload_materialize_fail;
            }
        }

        std::vector<std::uint8_t> ul_payload;
        ul =
            dsrrl::operators::lightbank::
                materialize_upper_lower_hemenv_receiver(
                    g_core.features(),
                    source,
                    pixel_shader->code_size,
                    ul_payload);

        if (ul.result ==
            dsrrl::operators::lightbank::
                upper_lower_hemenv_materialize_result::applied) {
            ul_identity_ready = true;
            ul_replacement_ready =
                g_upper_lower_hemenv.register_replacement(
                    ul,
                    ul_payload.data(),
                    ul_payload.size());
        }

        std::vector<std::uint8_t> h3_payload;
        h3 =
            dsrrl::operators::lightbank::
                materialize_hemdir3_b13_receiver(
                    g_core.features(),
                    source,
                    pixel_shader->code_size,
                    h3_payload);

        if (h3.result ==
            dsrrl::operators::lightbank::
                hemdir3_b13_materialize_result::applied) {
            h3_identity_ready = true;
            h3_replacement_ready =
                g_hemdir3.register_replacement(
                    h3,
                    h3_payload.data(),
                    h3_payload.size());
        }
    }

    const bool a1_changed =
        g_a1_bridge.on_create_pipeline(
            device,
            layout,
            subobject_count,
            subobjects);

    if (ul_identity_ready) {
        const auto *created_shader =
            find_pixel_shader(
                subobject_count,
                subobjects);

        if (created_shader == nullptr ||
            created_shader->code == nullptr ||
            created_shader->code_size == 0u) {
            ul_identity_ready = false;
        } else {
            dsrrl::runtime::upper_lower_receiver_identity identity{};
            identity.plan_index = ul.plan_index;
            identity.shader_index = ul.shader_index;
            identity.stable_receiver_id =
                ul.stable_receiver_id;
            identity.stratum = ul.stratum;
            identity.family = ul.family;

            ul_identity_ready =
                dsrrl::runtime::
                    upper_lower_receiver_attest_created_code(
                        created_shader->code,
                        created_shader->code_size,
                        identity);
        }
    }

    if (h3_identity_ready) {
        const auto *created_shader =
            find_pixel_shader(
                subobject_count,
                subobjects);

        if (created_shader == nullptr ||
            created_shader->code == nullptr ||
            created_shader->code_size == 0u) {
            h3_identity_ready = false;
        } else {
            dsrrl::runtime::hemdir3_receiver_identity identity{};
            identity.plan_index = h3.plan_index;
            identity.shader_index = h3.shader_index;
            identity.stratum = h3.stratum;
            identity.paired_stable_receiver_id =
                h3.paired_stable_receiver_id;

            h3_identity_ready =
                dsrrl::runtime::
                    hemdir3_receiver_attest_created_code(
                        created_shader->code,
                        created_shader->code_size,
                        identity);
        }
    }

    (void)ul_identity_ready;
    (void)ul_replacement_ready;
    (void)h3_identity_ready;
    (void)h3_replacement_ready;
    return a1_changed;
}

void on_init_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline_layout layout,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects,
    reshade::api::pipeline pipeline)
{
    g_a1_bridge.on_init_pipeline(
        device, layout, subobject_count, subobjects, pipeline);

    const auto *pixel_shader =
        find_pixel_shader(
            subobject_count,
            subobjects);

    if (pixel_shader != nullptr &&
        pixel_shader->code != nullptr &&
        pixel_shader->code_size != 0u) {
        (void)dsrrl::runtime::
            stable_receiver_observe_pipeline(
                pipeline.handle,
                pixel_shader->code,
                pixel_shader->code_size);
        (void)dsrrl::runtime::
            hemenvlerp_receiver_observe_pipeline(
                pipeline.handle,
                pixel_shader->code,
                pixel_shader->code_size);
        (void)dsrrl::runtime::
            subsurface_receiver_observe_pipeline(
                pipeline.handle,
                pixel_shader->code,
                pixel_shader->code_size);
        (void)dsrrl::runtime::
            hemdir3_receiver_observe_pipeline(
                pipeline.handle,
                pixel_shader->code,
                pixel_shader->code_size);
        (void)dsrrl::runtime::
            upper_lower_receiver_observe_pipeline(
                pipeline.handle,
                pixel_shader->code,
                pixel_shader->code_size);
    }
}

void on_destroy_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline pipeline)
{
    dsrrl::runtime::stable_receiver_forget_pipeline(
        pipeline.handle);
    dsrrl::runtime::hemenvlerp_receiver_forget_pipeline(
        pipeline.handle);
    dsrrl::runtime::subsurface_receiver_forget_pipeline(
        pipeline.handle);
    dsrrl::runtime::hemdir3_receiver_forget_pipeline(
        pipeline.handle);
    dsrrl::runtime::upper_lower_receiver_forget_pipeline(
        pipeline.handle);
    g_a1_bridge.on_destroy_pipeline(device, pipeline);
}

void on_bind_pipeline(
    reshade::api::command_list *cmd_list,
    reshade::api::pipeline_stage stages,
    reshade::api::pipeline pipeline)
{
    const bool pixel_stage_bound =
        (static_cast<std::uint32_t>(stages) &
         static_cast<std::uint32_t>(
             reshade::api::pipeline_stage::pixel_shader)) != 0u;

    dsrrl::runtime::stable_receiver_observe_bind(
        cmd_list,
        pixel_stage_bound,
        pipeline.handle);
    dsrrl::runtime::hemenvlerp_receiver_observe_bind(
        cmd_list,
        pixel_stage_bound,
        pipeline.handle);
    dsrrl::runtime::subsurface_receiver_observe_bind(
        cmd_list,
        pixel_stage_bound,
        pipeline.handle);
    dsrrl::runtime::hemdir3_receiver_observe_bind(
        cmd_list,
        pixel_stage_bound,
        pipeline.handle);
    dsrrl::runtime::upper_lower_receiver_observe_bind(
        cmd_list,
        pixel_stage_bound,
        pipeline.handle);

    std::uint16_t first_plan = 0xFFFFu;
    dsrrl::core::operator_mask selected_owners = 0u;
    std::uint16_t selected_ops = 0u;
    std::uint32_t receiver_id = 0u;

    const bool target =
        g_a1_bridge.on_bind_pipeline(
            stages,
            pipeline,
            &first_plan,
            &selected_owners,
            &selected_ops,
            &receiver_id);

    if (target && first_plan != 0xFFFFu) {
        char line[240]{};
        std::snprintf(
            line,
            sizeof(line),
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] "
            "FIRST_BIND plan=%u receiver=%u owners=0x%08X ops=%u",
            static_cast<unsigned>(first_plan),
            static_cast<unsigned>(receiver_id),
            static_cast<unsigned>(selected_owners),
            static_cast<unsigned>(selected_ops));
        reshade::log::message(reshade::log::level::info, line);
    }
}

struct prepared_island_batch {
    dsrrl::runtime::island_draw_batch batch{};
    dsrrl::runtime::prepared_material_response_draw mr{};
    dsrrl::runtime::prepared_material_resource_draw resources{};
    dsrrl::runtime::prepared_subsurface_draw subsurface{};
    dsrrl::runtime::prepared_hemdir3_draw hemdir3{};
    dsrrl::runtime::prepared_upper_lower_hemenv_draw upper_lower{};
    dsrrl::runtime::prepared_pmetal_envspec_draw envspec{};
    bool mr_in_batch = false;
    bool upper_lower_combined = false;
    bool subsurface_in_batch = false;
    bool hemdir3_in_batch = false;
    bool envspec_in_batch = false;
};

void release_prepared_island_batch(
    prepared_island_batch &prepared) noexcept
{
    if (prepared.hemdir3_in_batch) {
        g_hemdir3.release_prepared_draw(
            prepared.hemdir3);
    } else if (prepared.subsurface_in_batch) {
        g_subsurface.release(
            prepared.subsurface);
    } else if (prepared.envspec_in_batch) {
        g_pmetal_envspec.release(
            prepared.envspec);
    } else {
        g_upper_lower_hemenv.release_prepared_draw(
            prepared.upper_lower);
        g_material_resources.release_prepared_draw(
            prepared.resources);
        g_mr_draw_runtime.release_prepared_draw(
            prepared.mr);
    }

    prepared = {};
}

struct draw_semantic_selection_guard {
    ~draw_semantic_selection_guard()
    {
        g_upper_lower.consume_draw_selection();
        dsrrl::runtime::hemdir3_mode_transport::
            consume_draw_selection();
    }
};

bool prepare_island_batch(
    reshade::api::command_list *cmd_list,
    std::uint32_t receiver_id,
    bool hemenvlerp_bound,
    bool subsurface_bound,
    bool hemdir3_bound,
    const dsrrl::runtime::hemdir3_receiver_identity &hemdir3_identity,
    bool upper_lower_bound,
    const dsrrl::runtime::upper_lower_receiver_identity &upper_lower_identity,
    const dsrrl::operators::material_response::material_identity &material,
    const dsrrl::operators::material_response::decision &decision,
    prepared_island_batch &prepared) noexcept
{
    prepared = {};

    if (hemdir3_bound) {
        if (!g_hemdir3.prepare_draw_request(
                cmd_list,
                hemdir3_identity,
                material,
                prepared.hemdir3))
            return false;

        prepared.hemdir3_in_batch = true;

        if (dsrrl::runtime::append_island_draw_request(
                prepared.batch,
                prepared.hemdir3.request) !=
            dsrrl::runtime::island_draw_batch_result::ready) {
            release_prepared_island_batch(prepared);
            return false;
        }

        return true;
    }

    if (subsurface_bound) {
        if (!g_subsurface.prepare(
                cmd_list,
                material,
                prepared.subsurface))
            return false;

        prepared.subsurface_in_batch = true;
        prepared.mr_in_batch = true;

        if (dsrrl::runtime::append_island_draw_request(
                prepared.batch,
                prepared.subsurface.subsurface) !=
            dsrrl::runtime::island_draw_batch_result::ready) {
            release_prepared_island_batch(prepared);
            return false;
        }

        if (dsrrl::runtime::append_island_draw_request(
                prepared.batch,
                prepared.subsurface.mr.request) !=
            dsrrl::runtime::island_draw_batch_result::ready) {
            release_prepared_island_batch(prepared);
            return false;
        }

        for (std::uint32_t i = 0u;
             i < prepared.subsurface.resources.request_count;
             ++i) {
            if (dsrrl::runtime::append_island_draw_request(
                    prepared.batch,
                    prepared.subsurface.resources.requests[i]) !=
                dsrrl::runtime::island_draw_batch_result::ready) {
                release_prepared_island_batch(prepared);
                return false;
            }
        }

        return true;
    }

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());

    const bool envspec_ul_verified =
        upper_lower_bound &&
        upper_lower_identity.stratum ==
            dsrrl::operators::lightbank::
                upper_lower_hemenv_stratum::spc &&
        upper_lower_identity.stable_receiver_id ==
            decision.receiver_id;

    // P_Metal EnvSpec owns the complete PS+b12+t12/t14+s12/s14 semantic
    // island. If any exact source/material/probe/resource precondition fails,
    // fall through to ordinary MR/U-L/resource routing for this draw.
    const auto envspec_family =
        hemenvlerp_bound
            ? dsrrl::runtime::pmetal_envspec_receiver_family::hemenvlerp
            : dsrrl::runtime::pmetal_envspec_receiver_family::stable_hemenv;

    if (decision.active &&
        g_pmetal_envspec.prepare(
            cmd_list,
            material,
            decision,
            envspec_family,
            envspec_ul_verified &&
                !hemenvlerp_bound,
            prepared.envspec)) {
        prepared.envspec_in_batch = true;

        if (dsrrl::runtime::append_island_draw_request(
                prepared.batch,
                prepared.envspec.request) !=
            dsrrl::runtime::island_draw_batch_result::ready) {
            release_prepared_island_batch(prepared);
            return false;
        }

        for (std::uint32_t i = 0u;
             i < prepared.envspec.material_resources.request_count;
             ++i) {
            if (dsrrl::runtime::append_island_draw_request(
                    prepared.batch,
                    prepared.envspec.material_resources.requests[i]) !=
                dsrrl::runtime::island_draw_batch_result::ready) {
                release_prepared_island_batch(prepared);
                return false;
            }
        }

        return true;
    }

    // HemEnvLerp is a distinct executable consumer. P_Metal EnvSpec above
    // owns its exact composed MR+EnvSpec+SpecRGB+U/L route. For every other
    // exact U/L-live HemEnvLerp receiver, use only the stock-derived U/L
    // replacement plus fresh b13. This restores the U/L operator without
    // silently importing stable-HemEnv MR/resource semantics into Lerp.
    if (hemenvlerp_bound) {
        if (upper_lower_bound &&
            upper_lower_identity.family ==
                dsrrl::operators::lightbank::
                    upper_lower_hemenv_family::hemenvlerp &&
            g_upper_lower_hemenv.prepare_draw_request(
                cmd_list,
                upper_lower_identity,
                false,
                prepared.upper_lower)) {
            if (dsrrl::runtime::append_island_draw_request(
                    prepared.batch,
                    prepared.upper_lower.request) !=
                dsrrl::runtime::island_draw_batch_result::ready) {
                release_prepared_island_batch(prepared);
                return false;
            }

            return true;
        }

        return false;
    }

    const bool ul_spc =
        upper_lower_bound &&
        upper_lower_identity.stratum ==
            dsrrl::operators::lightbank::
                upper_lower_hemenv_stratum::spc;

    if (decision.active &&
        ul_spc &&
        context != nullptr &&
        g_upper_lower.prepare_upper_lower_carrier(
            context,
            prepared.upper_lower.carrier)) {
        if (g_mr_draw_runtime.
                prepare_draw_request_with_upper_lower(
                    decision,
                    prepared.upper_lower.carrier.b13,
                    prepared.mr)) {
            if (dsrrl::runtime::append_island_draw_request(
                    prepared.batch,
                    prepared.mr.request) !=
                dsrrl::runtime::island_draw_batch_result::ready) {
                release_prepared_island_batch(prepared);
                return false;
            }

            prepared.mr_in_batch = true;
            prepared.upper_lower_combined = true;
        } else {
            g_upper_lower_hemenv.release_prepared_draw(
                prepared.upper_lower);
        }
    }

    // Independent fallback: if combined MR+U/L is unavailable, keep MR
    // active by itself and leave U/L at stock DSR for this draw.
    if (!prepared.mr_in_batch &&
        decision.active &&
        g_mr_draw_runtime.prepare_draw_request(
            decision,
            prepared.mr)) {
        if (dsrrl::runtime::append_island_draw_request(
                prepared.batch,
                prepared.mr.request) !=
            dsrrl::runtime::island_draw_batch_result::ready) {
            release_prepared_island_batch(prepared);
            return false;
        }
        prepared.mr_in_batch = true;
    }

    if (upper_lower_bound &&
        !prepared.upper_lower_combined &&
        g_upper_lower_hemenv.prepare_draw_request(
            cmd_list,
            upper_lower_identity,
            prepared.mr_in_batch,
            prepared.upper_lower)) {
        if (dsrrl::runtime::append_island_draw_request(
                prepared.batch,
                prepared.upper_lower.request) !=
            dsrrl::runtime::island_draw_batch_result::ready) {
            release_prepared_island_batch(prepared);
            return false;
        }
    }

    dsrrl::operators::material_response::mtd_semantic_query query{};
    query.material = material;
    query.receiver_id = receiver_id;
    query.ownership.flver_sha256 =
        material.flver_sha256;
    query.ownership.flver_identity_hash =
        material.flver_identity_hash;
    query.ownership.material_slot =
        material.material_slot;
    query.ownership.material_slot_valid =
        material.material_slot_valid;
    query.ownership.exact =
        material.owner_tuple_exact;

    if (context != nullptr) {
        (void)g_material_resources.prepare_draw_requests(
            context,
            receiver_id,
            query,
            prepared.mr_in_batch,
            prepared.resources);

        for (std::uint32_t i = 0u;
             i < prepared.resources.request_count;
             ++i) {
            if (dsrrl::runtime::append_island_draw_request(
                    prepared.batch,
                    prepared.resources.requests[i]) !=
                dsrrl::runtime::island_draw_batch_result::ready) {
                release_prepared_island_batch(prepared);
                return false;
            }
        }
    }

    return prepared.batch.island_count != 0u;
}

void observe_bloom_fx_draw_authority() noexcept
{
    dsrrl::runtime::bloom_fx_draw_transport::fx_draw_snapshot snapshot{};
    if (!dsrrl::runtime::bloom_fx_draw_transport::snapshot(snapshot))
        return;

    ++g_bloom_fx_draw_snapshots;

    const auto authority =
        dsrrl::runtime::bloom_fx_draw_transport::
            validate_waterwave_draw_authority(snapshot);

    if (authority ==
        dsrrl::runtime::bloom_fx_draw_transport::
            waterwave_draw_authority_result::authorized)
        ++g_bloom_fx_draw_authorized;
    else
        ++g_bloom_fx_draw_rejected;
}

bool on_draw(
    reshade::api::command_list *cmd_list,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance)
{
    observe_bloom_fx_draw_authority();

    draw_semantic_selection_guard semantic_guard{};
    std::uint32_t receiver_id = 0u;
    bool hemenvlerp_bound = false;
    dsrrl::runtime::hemenvlerp_receiver_identity hemenvlerp_identity{};
    bool subsurface_bound = false;
    bool hemdir3_bound = false;
    dsrrl::runtime::hemdir3_receiver_identity hemdir3_identity{};
    bool upper_lower_bound = false;
    dsrrl::runtime::upper_lower_receiver_identity upper_lower_identity{};
    dsrrl::operators::material_response::material_identity material{};
    dsrrl::operators::material_response::decision decision{};

    if (!observe_draw_identity(
            cmd_list,
            receiver_id,
            hemenvlerp_bound,
            hemenvlerp_identity,
            subsurface_bound,
            hemdir3_bound,
            hemdir3_identity,
            upper_lower_bound,
            upper_lower_identity,
            material,
            decision))
        return false;

    prepared_island_batch prepared{};
    if (!prepare_island_batch(
            cmd_list,
            receiver_id,
            hemenvlerp_bound,
            subsurface_bound,
            hemdir3_bound,
            hemdir3_identity,
            upper_lower_bound,
            upper_lower_identity,
            material,
            decision,
            prepared))
        return false;

    const auto dispatch =
        dsrrl::runtime::dispatch_island_draw_batch(
            g_draw_transactions,
            cmd_list,
            prepared.batch,
            vertex_count,
            instance_count,
            first_vertex,
            first_instance);

    const bool mr_in_batch =
        prepared.mr_in_batch;

    release_prepared_island_batch(
        prepared);

    if (mr_in_batch)
        g_mr_draw_runtime.account_dispatch_result(
            dispatch.transaction);

    return dsrrl::runtime::draw_tx_issued(
        dispatch.transaction);
}

bool on_draw_indexed(
    reshade::api::command_list *cmd_list,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance)
{
    observe_bloom_fx_draw_authority();

    draw_semantic_selection_guard semantic_guard{};
    std::uint32_t receiver_id = 0u;
    bool hemenvlerp_bound = false;
    dsrrl::runtime::hemenvlerp_receiver_identity hemenvlerp_identity{};
    bool subsurface_bound = false;
    bool hemdir3_bound = false;
    dsrrl::runtime::hemdir3_receiver_identity hemdir3_identity{};
    bool upper_lower_bound = false;
    dsrrl::runtime::upper_lower_receiver_identity upper_lower_identity{};
    dsrrl::operators::material_response::material_identity material{};
    dsrrl::operators::material_response::decision decision{};

    if (!observe_draw_identity(
            cmd_list,
            receiver_id,
            hemenvlerp_bound,
            hemenvlerp_identity,
            subsurface_bound,
            hemdir3_bound,
            hemdir3_identity,
            upper_lower_bound,
            upper_lower_identity,
            material,
            decision))
        return false;

    prepared_island_batch prepared{};
    if (!prepare_island_batch(
            cmd_list,
            receiver_id,
            hemenvlerp_bound,
            subsurface_bound,
            hemdir3_bound,
            hemdir3_identity,
            upper_lower_bound,
            upper_lower_identity,
            material,
            decision,
            prepared))
        return false;

    const auto dispatch =
        dsrrl::runtime::dispatch_island_draw_indexed_batch(
            g_draw_transactions,
            cmd_list,
            prepared.batch,
            index_count,
            instance_count,
            first_index,
            vertex_offset,
            first_instance);

    const bool mr_in_batch =
        prepared.mr_in_batch;

    release_prepared_island_batch(
        prepared);

    if (mr_in_batch)
        g_mr_draw_runtime.account_dispatch_result(
            dispatch.transaction);

    return dsrrl::runtime::draw_tx_issued(
        dispatch.transaction);
}

void on_present(
    reshade::api::command_queue *,
    reshade::api::swapchain *,
    const reshade::api::rect *,
    const reshade::api::rect *,
    std::uint32_t,
    const reshade::api::rect *)
{
    const auto present = ++g_present_count;
    if (present == 1u || (present % 300u) == 0u)
        log_state("LIVE");
}

void register_events()
{
    reshade::register_event<reshade::addon_event::init_device>(on_init_device);
    reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
    reshade::register_event<reshade::addon_event::create_pipeline>(on_create_pipeline);
    reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
    reshade::register_event<reshade::addon_event::destroy_pipeline>(on_destroy_pipeline);
    reshade::register_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
    reshade::register_event<reshade::addon_event::draw>(on_draw);
    reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
    reshade::register_event<reshade::addon_event::present>(on_present);
}

void unregister_events()
{
    reshade::unregister_event<reshade::addon_event::present>(on_present);
    reshade::unregister_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
    reshade::unregister_event<reshade::addon_event::draw>(on_draw);
    reshade::unregister_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
    reshade::unregister_event<reshade::addon_event::destroy_pipeline>(on_destroy_pipeline);
    reshade::unregister_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
    reshade::unregister_event<reshade::addon_event::create_pipeline>(on_create_pipeline);
    reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
    reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
}

} // namespace

extern "C" __declspec(dllexport)
const char *NAME =
    DSRRL_CORE_ISLANDS_PRODUCT_LINE " " DSRRL_CORE_ISLANDS_VERSION;

extern "C" __declspec(dllexport)
const char *AUTHOR =
    "DSR Restored Lighting";

extern "C" __declspec(dllexport)
const char *DESCRIPTION =
    DSRRL_CORE_ISLANDS_PRODUCT_LINE " " DSRRL_CORE_ISLANDS_VERSION
    " active main development line; native Renderer Core operator islands.";

extern "C" __declspec(dllexport)
bool AddonInit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    if (!reshade::register_addon(addon_module, reshade_module))
        return false;

    g_a1_bridge.reset();
    g_draw_transactions.reset();
    g_mr_draw_runtime.reset();
    g_material_resources.reset();
    g_envspec_resources.reset_stats();
    g_bloom_scene_sidecar.reset();
    dsrrl::runtime::bloom_fx_draw_transport::reset_stats();
    g_pmetal_envspec.reset();
    g_upper_lower.reset();
    g_upper_lower_hemenv.reset();
    g_hemdir3.reset();
    dsrrl::runtime::hemdir3_mode_transport::reset_stats();
    g_present_count.store(0);
    g_mr_draw_eval.store(0);
    g_mr_would_activate.store(0);
    g_mr_fail_open.store(0);
    g_mr_payload_materialize_ok.store(0);
    g_mr_payload_materialize_fail.store(0);
    g_mr_ul_payload_materialize_ok.store(0);
    g_mr_ul_payload_materialize_fail.store(0);
    g_envspec_payload_materialize_ok.store(0);
    g_envspec_payload_materialize_fail.store(0);
    g_draw_events.store(0);
    g_draw_receiver_hits.store(0);
    g_draw_owner_hits.store(0);
    g_draw_joins.store(0);
    g_draw_owner_only.store(0);
    g_draw_receiver_only.store(0);
    g_bloom_fx_draw_snapshots.store(0);
    g_bloom_fx_draw_authorized.store(0);
    g_bloom_fx_draw_rejected.store(0);
    dsrrl::runtime::stable_receiver_pipeline_reset();
    dsrrl::runtime::hemenvlerp_receiver_pipeline_reset();
    dsrrl::runtime::subsurface_receiver_pipeline_reset();
    dsrrl::runtime::hemdir3_receiver_pipeline_reset();
    dsrrl::runtime::upper_lower_receiver_pipeline_reset();
    g_subsurface.reset();
    dsrrl::runtime::material_owner_selection_reset_stats();

    const auto receivers =
        dsrrl::operators::material_response::
            register_confirmed_material_receivers_v1(
                g_material_response);
    const auto routes =
        dsrrl::operators::material_response::
            register_confirmed_material_routes_v1(
                g_material_response);

    g_mr_ready.store(
        receivers == 24u &&
        routes == 35u &&
        g_material_response.receiver_recipe_count() == 24u &&
        g_material_response.material_profile_count() == 35u);

    if (!enable_integrated_islands()) {
        disable_integrated_islands();
        reshade::unregister_addon(addon_module, reshade_module);
        return false;
    }

    register_events();

    if (!g_material_resources.register_events()) {
        unregister_events();
        disable_integrated_islands();
        reshade::unregister_addon(addon_module, reshade_module);
        return false;
    }

    const bool envspec_resource_events =
        g_envspec_resources.register_events();

    if (!envspec_resource_events) {
        (void)g_core.features().set(
            dsrrl::core::operator_id::env_spec,
            false);
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] EnvSpec resource events FAIL-OPEN: P_Metal EnvSpec stays stock; other islands remain active.");
    }

    const bool texture_hooks =
        dsrrl::runtime::texture_identity_transport::install();

    if (!texture_hooks) {
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] texture identity hooks FAIL-OPEN: SpecRGB/Diffuse/Normal sidecars remain stock.");
    }

    const bool flver_hooks =
        dsrrl::runtime::flver_identity_transport::install();

    if (!flver_hooks) {
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] FLVER identity hooks FAIL-OPEN: stock DSR preserved for exact owner routing.");
    }

    const bool bloom_fx_hooks =
        flver_hooks &&
        dsrrl::runtime::bloom_fx_draw_transport::install();

    if (!bloom_fx_hooks) {
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] Bloom FX draw transport FAIL-OPEN: Q8 sidecar remains unauthorised; stock SFX preserved.");
    }

    const bool hemdir3_mode_hooks =
        flver_hooks &&
        dsrrl::runtime::hemdir3_mode_transport::install();

    if (!hemdir3_mode_hooks) {
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] HemDir3 effective-mode hooks FAIL-OPEN: HemDir3 remains stock.");
    }

    const bool upper_lower_hooks =
        flver_hooks &&
        g_upper_lower.install();

    if (!upper_lower_hooks) {
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] Upper/Lower producer hooks FAIL-OPEN: stock DSR b13 preserved.");
    }

    reshade::log::message(
        reshade::log::level::info,
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
        "] READY: shared Core draw-state transaction layer (PS/CB/SRV/sampler) "
        "is active; exact Material Response, SpecRGB, Upper/Lower, Subsurface and native "
        "HemDir3 no-Spc/Spc routes are construction-armed. Exact P_Metal EnvSpec uses its "
        "own material/source/probe-gated PS+b12(+b13)+t10+t12/t14+s12/s14 single replay. "
        "Runtime activation and PTDE pixel behavior remain separate validation stages; "
        "frozen legacy monolith is not linked.");

    return true;
}

extern "C" __declspec(dllexport)
void AddonUninit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    unregister_events();
    log_state("PRE_UNLOAD");
    g_upper_lower.uninstall();

    if (g_upper_lower.telemetry().restore_failed)
        log_state("UL_UNLOAD_RESTORE_FAIL");

    dsrrl::runtime::hemdir3_mode_transport::uninstall();

    if (dsrrl::runtime::hemdir3_mode_transport::status().restore_failed)
        log_state("MODE2_UNLOAD_RESTORE_FAIL");

    dsrrl::runtime::bloom_fx_draw_transport::uninstall();

    if (dsrrl::runtime::bloom_fx_draw_transport::status().restore_failed)
        log_state("BLOOM_FX_UNLOAD_RESTORE_FAIL");

    dsrrl::runtime::flver_identity_transport::uninstall();

    if (dsrrl::runtime::flver_identity_transport::status().restore_failed)
        log_state("UNLOAD_RESTORE_FAIL");

    dsrrl::runtime::stable_receiver_pipeline_reset();
    dsrrl::runtime::hemenvlerp_receiver_pipeline_reset();
    dsrrl::runtime::subsurface_receiver_pipeline_reset();
    dsrrl::runtime::hemdir3_receiver_pipeline_reset();
    dsrrl::runtime::upper_lower_receiver_pipeline_reset();
    dsrrl::runtime::texture_identity_transport::uninstall();
    g_envspec_resources.unregister_events();
    g_material_resources.unregister_events();
    g_pmetal_envspec.reset();
    g_envspec_resources.reset_stats();
    g_bloom_scene_sidecar.reset();
    dsrrl::runtime::bloom_fx_draw_transport::reset_stats();
    g_material_resources.reset();
    g_upper_lower.reset();
    g_upper_lower_hemenv.reset();
    g_hemdir3.reset();
    g_mr_draw_runtime.reset();
    g_draw_transactions.reset();
    g_a1_bridge.reset();
    disable_integrated_islands();

    reshade::unregister_addon(addon_module, reshade_module);
}
