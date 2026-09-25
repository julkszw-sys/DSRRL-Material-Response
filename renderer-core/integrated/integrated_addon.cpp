#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/a1_create_pipeline_bridge.hpp"
#include "dsrrl/runtime/flver_identity_transport.hpp"
#include "dsrrl/runtime/flver_identity_registry.hpp"
#include "dsrrl/runtime/material_owner_selection.hpp"
#include "dsrrl/runtime/material_response_draw_transaction.hpp"
#include "dsrrl/runtime/stable_receiver_pipeline_registry.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/material_response_seed.hpp"
#include "dsrrl/operators/material_response/material_response_v211_materializer.hpp"

#include <reshade.hpp>

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

std::atomic<std::uint64_t> g_present_count{0};
std::atomic<std::uint64_t> g_mr_draw_eval{0};
std::atomic<std::uint64_t> g_mr_would_activate{0};
std::atomic<std::uint64_t> g_mr_fail_open{0};
std::atomic_bool g_mr_ready{false};
std::atomic<std::uint64_t> g_mr_payload_materialize_ok{0};
std::atomic<std::uint64_t> g_mr_payload_materialize_fail{0};
std::atomic<std::uint64_t> g_draw_events{0};
std::atomic<std::uint64_t> g_draw_receiver_hits{0};
std::atomic<std::uint64_t> g_draw_owner_hits{0};
std::atomic<std::uint64_t> g_draw_joins{0};
std::atomic<std::uint64_t> g_draw_owner_only{0};
std::atomic<std::uint64_t> g_draw_receiver_only{0};

constexpr dsrrl::core::operator_id k_integrated_islands[] = {
    dsrrl::core::operator_id::material_response,
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
    dsrrl::operators::material_response::decision &out_decision) noexcept
{
    ++g_draw_events;

    receiver_id = 0u;
    const bool receiver_ok =
        dsrrl::runtime::stable_receiver_bound(
            cmd_list,
            receiver_id);

    dsrrl::operators::material_response::material_identity material{};
    const bool owner_ok =
        dsrrl::runtime::material_owner_selection_consume(
            material);

    if (receiver_ok)
        ++g_draw_receiver_hits;
    if (owner_ok)
        ++g_draw_owner_hits;

    if (!receiver_ok || !owner_ok) {
        if (owner_ok && !receiver_ok)
            ++g_draw_owner_only;
        if (receiver_ok && !owner_ok)
            ++g_draw_receiver_only;
        ++g_mr_fail_open;
        return false;
    }

    ++g_draw_joins;

    if (!g_mr_ready.load()) {
        ++g_mr_fail_open;
        return false;
    }

    ++g_mr_draw_eval;
    out_decision =
        g_material_response.evaluate(
            receiver_id,
            material);

    if (out_decision.active) {
        ++g_mr_would_activate;
        return true;
    }

    ++g_mr_fail_open;
    return false;
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

    char line[1152]{};
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
        "mr_b12_create=%llu mr_b12_hit=%llu mr_b12_bind_fail=%llu "
        "mr_tx_eligible=%llu mr_tx_miss=%llu mr_replay=%llu mr_restore_fail=%llu mr_quarantine=%u "
        "tx_begin_ok=%llu tx_begin_fail=%llu tx_bind_fail=%llu tx_issued=%llu "
        "tx_restore_ok=%llu tx_restore_fail=%llu tx_quarantine=%u "
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
        static_cast<unsigned long long>(g_draw_events.load()),
        static_cast<unsigned long long>(g_draw_receiver_hits.load()),
        static_cast<unsigned long long>(g_draw_owner_hits.load()),
        static_cast<unsigned long long>(g_draw_joins.load()),
        static_cast<unsigned long long>(g_draw_owner_only.load()),
        static_cast<unsigned long long>(g_draw_receiver_only.load()));

    reshade::log::message(reshade::log::level::info, line);
}

void on_init_device(reshade::api::device *device)
{
    g_a1_bridge.on_init_device(device);
    g_mr_draw_runtime.on_init_device(device);
}

void on_destroy_device(reshade::api::device *device)
{
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
            if (!g_mr_draw_runtime.has_receiver_replacement(
                    mr.receiver_id)) {
                if (g_mr_draw_runtime.register_receiver_replacement(
                        mr.receiver_id,
                        mr_payload.data(),
                        mr_payload.size(),
                        mr.composed_owners))
                    ++g_mr_payload_materialize_ok;
                else
                    ++g_mr_payload_materialize_fail;
            }
        } else if (
            mr.result != mr_result::pass_not_candidate &&
            mr.result != mr_result::pass_unknown_exact_sha) {
            ++g_mr_payload_materialize_fail;
        }
    }

    return g_a1_bridge.on_create_pipeline(
        device, layout, subobject_count, subobjects);
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
    }
}

void on_destroy_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline pipeline)
{
    dsrrl::runtime::stable_receiver_forget_pipeline(
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

bool on_draw(
    reshade::api::command_list *cmd_list,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance)
{
    std::uint32_t receiver_id = 0u;
    dsrrl::operators::material_response::decision decision{};
    if (!observe_draw_identity(cmd_list, receiver_id, decision))
        return false;

    return g_mr_draw_runtime.replay_draw(
        cmd_list,
        decision,
        vertex_count,
        instance_count,
        first_vertex,
        first_instance);
}

bool on_draw_indexed(
    reshade::api::command_list *cmd_list,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance)
{
    std::uint32_t receiver_id = 0u;
    dsrrl::operators::material_response::decision decision{};
    if (!observe_draw_identity(cmd_list, receiver_id, decision))
        return false;

    return g_mr_draw_runtime.replay_draw_indexed(
        cmd_list,
        decision,
        index_count,
        instance_count,
        first_index,
        vertex_offset,
        first_instance);
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
    g_present_count.store(0);
    g_mr_draw_eval.store(0);
    g_mr_would_activate.store(0);
    g_mr_fail_open.store(0);
    g_mr_payload_materialize_ok.store(0);
    g_mr_payload_materialize_fail.store(0);
    g_draw_events.store(0);
    g_draw_receiver_hits.store(0);
    g_draw_owner_hits.store(0);
    g_draw_joins.store(0);
    g_draw_owner_only.store(0);
    g_draw_receiver_only.store(0);
    dsrrl::runtime::stable_receiver_pipeline_reset();
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

    const bool flver_hooks =
        dsrrl::runtime::flver_identity_transport::install();

    if (!flver_hooks) {
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] FLVER identity hooks FAIL-OPEN: stock DSR preserved for exact owner routing.");
    }

    reshade::log::message(
        reshade::log::level::info,
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
        "] READY: shared Core draw-state transaction layer (PS/CB/SRV/sampler) "
        "is active; Material Response V2.11 is the first migrated adapter on exact "
        "receiver/owner routing. Other draw-specific islands fail open until their own "
        "verified adapter payload is armed; frozen legacy monolith is not linked.");

    return true;
}

extern "C" __declspec(dllexport)
void AddonUninit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    unregister_events();
    log_state("PRE_UNLOAD");
    dsrrl::runtime::flver_identity_transport::uninstall();

    if (dsrrl::runtime::flver_identity_transport::status().restore_failed)
        log_state("UNLOAD_RESTORE_FAIL");

    dsrrl::runtime::stable_receiver_pipeline_reset();
    g_mr_draw_runtime.reset();
    g_draw_transactions.reset();
    g_a1_bridge.reset();
    disable_integrated_islands();

    reshade::unregister_addon(addon_module, reshade_module);
}
