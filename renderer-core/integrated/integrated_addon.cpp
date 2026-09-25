#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/a1_create_pipeline_bridge.hpp"
#include "dsrrl/runtime/flver_identity_transport.hpp"
#include "dsrrl/runtime/flver_identity_registry.hpp"
#include "dsrrl/runtime/material_owner_selection.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/material_response_seed.hpp"
#include "dsrrl/runtime/stable_receiver_pipeline_registry.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/material_response_seed.hpp"

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

namespace {

dsrrl::core::renderer_core g_core;
dsrrl::runtime::a1_create_pipeline_bridge
    g_a1_bridge(g_core.features());
dsrrl::operators::material_response::material_response_island g_material_response;
std::atomic<std::uint64_t> g_mr_draw_eval{0};
std::atomic<std::uint64_t> g_mr_active{0};
std::atomic<std::uint64_t> g_mr_fail_open{0};
thread_local std::uint32_t g_bound_receiver_id = 0u;

std::atomic<std::uint64_t> g_present_count{0};

dsrrl::operators::material_response::material_response_island g_mr_probe;
std::atomic_bool g_mr_probe_ready{false};
std::atomic<std::uint64_t> g_draw_probe_events{0};
std::atomic<std::uint64_t> g_draw_probe_receiver_hits{0};
std::atomic<std::uint64_t> g_draw_probe_owner_hits{0};
std::atomic<std::uint64_t> g_draw_probe_joins{0};
std::atomic<std::uint64_t> g_draw_probe_active{0};
std::atomic<std::uint64_t> g_draw_probe_inactive{0};
std::atomic<std::uint64_t> g_draw_probe_owner_only{0};
std::atomic<std::uint64_t> g_draw_probe_receiver_only{0};

constexpr dsrrl::core::operator_id k_integrated_islands[] = {
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

void reset_draw_probe() noexcept
{
    dsrrl::runtime::stable_receiver_pipeline_reset();
    g_draw_probe_events.store(0);
    g_draw_probe_receiver_hits.store(0);
    g_draw_probe_owner_hits.store(0);
    g_draw_probe_joins.store(0);
    g_draw_probe_active.store(0);
    g_draw_probe_inactive.store(0);
    g_draw_probe_owner_only.store(0);
    g_draw_probe_receiver_only.store(0);
}

void observe_draw_identity(
    reshade::api::command_list *cmd_list) noexcept
{
    ++g_draw_probe_events;

    std::uint32_t receiver_id = 0u;
    const bool receiver_ok =
        dsrrl::runtime::stable_receiver_bound(
            cmd_list,
            receiver_id);

    dsrrl::operators::material_response::material_identity material{};
    const bool owner_ok =
        dsrrl::runtime::flver_identity_transport::
            consume_selector_owner_candidate(material);

    if (receiver_ok)
        ++g_draw_probe_receiver_hits;
    if (owner_ok)
        ++g_draw_probe_owner_hits;

    if (!receiver_ok || !owner_ok) {
        if (owner_ok && !receiver_ok)
            ++g_draw_probe_owner_only;
        if (receiver_ok && !owner_ok)
            ++g_draw_probe_receiver_only;
        return;
    }

    ++g_draw_probe_joins;

    if (!g_mr_probe_ready.load()) {
        ++g_draw_probe_inactive;
        return;
    }

    const auto decision =
        g_mr_probe.evaluate(
            receiver_id,
            material);

    if (decision.active)
        ++g_draw_probe_active;
    else
        ++g_draw_probe_inactive;
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
    const auto o =
        dsrrl::runtime::flver_identity_transport::
            selector_owner_stats();
    const auto r =
        dsrrl::runtime::stable_receiver_pipeline_stats();

    char line[1280]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s "
        "create=%llu candidate=%llu exact=%llu materialized=%llu "
        "unknown=%llu no_owner=%llu failopen=%llu init_ok=%llu "
        "init_bad=%llu binds=%llu quarantine=%u "
        "flver_hook=%u/%u/%u prov=%u owner_enrich=%u restore_fail=%u "
        "flver_ins=%llu flver_lookup=%llu flver_hit=%llu flver_miss=%llu flver_erase=%llu flver_bad=%llu "
        "sel=%llu owner_sha=%llu owner_mtd=%llu owner_ready=%llu owner_fail=%llu owner_cons=%llu owner_cons_miss=%llu "
        "rx_init=%llu rx_cand=%llu rx_exact=%llu rx_hash_miss=%llu rx_bind=%llu rx_exact_bind=%llu rx_unknown_bind=%llu "
        "draw=%llu draw_rx=%llu draw_owner=%llu draw_join=%llu mr_probe_ready=%u mr_would_active=%llu mr_inactive=%llu owner_only=%llu rx_only=%llu",
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
        static_cast<unsigned long long>(o.selector_events),
        static_cast<unsigned long long>(o.owner_sha_hits),
        static_cast<unsigned long long>(o.owner_mtd_hits),
        static_cast<unsigned long long>(o.exact_owner_ready),
        static_cast<unsigned long long>(o.owner_fail_open),
        static_cast<unsigned long long>(o.consumed),
        static_cast<unsigned long long>(o.consume_misses),
        static_cast<unsigned long long>(r.pipeline_inits),
        static_cast<unsigned long long>(r.candidate_size_hits),
        static_cast<unsigned long long>(r.exact_receiver_hits),
        static_cast<unsigned long long>(r.candidate_hash_misses),
        static_cast<unsigned long long>(r.pixel_binds),
        static_cast<unsigned long long>(r.exact_binds),
        static_cast<unsigned long long>(r.unknown_binds),
        static_cast<unsigned long long>(g_draw_probe_events.load()),
        static_cast<unsigned long long>(g_draw_probe_receiver_hits.load()),
        static_cast<unsigned long long>(g_draw_probe_owner_hits.load()),
        static_cast<unsigned long long>(g_draw_probe_joins.load()),
        g_mr_probe_ready.load() ? 1u : 0u,
        static_cast<unsigned long long>(g_draw_probe_active.load()),
        static_cast<unsigned long long>(g_draw_probe_inactive.load()),
        static_cast<unsigned long long>(g_draw_probe_owner_only.load()),
        static_cast<unsigned long long>(g_draw_probe_receiver_only.load()));

    reshade::log::message(reshade::log::level::info, line);
}

void on_init_device(reshade::api::device *device)
{
    g_a1_bridge.on_init_device(device);
}

void on_destroy_device(reshade::api::device *device)
{
    g_a1_bridge.on_destroy_device(device);
}

bool on_create_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline_layout layout,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects)
{
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

    g_bound_receiver_id = target ? receiver_id : 0u;

    if (target && first_plan != 0xFFFFu) {
        char line[240]{};
        std::snprintf(
            line,
            sizeof(line),
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] "
            "FIRST_BIND plan=%u owners=0x%08X ops=%u",
            static_cast<unsigned>(first_plan),
            static_cast<unsigned>(selected_owners),
            static_cast<unsigned>(selected_ops));
        reshade::log::message(reshade::log::level::info, line);
    }
}

bool on_draw(
    reshade::api::command_list *cmd_list,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t)
{
    observe_draw_identity(cmd_list);
    return false;
}

bool on_draw_indexed(
    reshade::api::command_list *cmd_list,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t,
    std::int32_t,
    std::uint32_t)
{
    observe_draw_identity(cmd_list);
    return false;
}

bool on_draw_indexed(
    reshade::api::command_list *,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t,
    std::int32_t,
    std::uint32_t)
{
    const auto material =
        dsrrl::runtime::material_owner_selection_current();
    dsrrl::runtime::material_owner_selection_clear();

    if (g_bound_receiver_id == 0u || !material.has_value()) {
        ++g_mr_fail_open;
        return false;
    }

    ++g_mr_draw_eval;
    const auto decision =
        g_material_response.evaluate(g_bound_receiver_id, material);

    if (decision.active)
        ++g_mr_active;
    else
        ++g_mr_fail_open;

    return false;
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
    reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
    reshade::register_event<reshade::addon_event::draw>(on_draw);
    reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
    reshade::register_event<reshade::addon_event::present>(on_present);
}

void unregister_events()
{
    reshade::unregister_event<reshade::addon_event::present>(on_present);
    reshade::unregister_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
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
    g_present_count.store(0);
    g_mr_draw_eval.store(0);
    g_mr_active.store(0);
    g_mr_fail_open.store(0);
    g_bound_receiver_id = 0u;
    dsrrl::runtime::material_owner_selection_reset_stats();
    (void)dsrrl::operators::material_response::register_confirmed_material_receivers_v1(g_material_response);
    (void)dsrrl::operators::material_response::register_confirmed_material_routes_v1(g_material_response);
    reset_draw_probe();

    const auto seeded_receivers =
        dsrrl::operators::material_response::
            register_confirmed_material_receivers_v1(
                g_mr_probe);
    const auto seeded_routes =
        dsrrl::operators::material_response::
            register_confirmed_material_routes_v1(
                g_mr_probe);

    g_mr_probe_ready.store(
        seeded_receivers == 24u &&
        seeded_routes == 35u &&
        g_mr_probe.receiver_recipe_count() == 24u &&
        g_mr_probe.material_profile_count() == 35u);

    if (!enable_integrated_islands()) {
        disable_integrated_islands();
        reshade::unregister_addon(addon_module, reshade_module);
        return false;
    }

    register_events();

    const bool flver_hooks = dsrrl::runtime::flver_identity_transport::install();
    if (!flver_hooks) {
        reshade::log::message(reshade::log::level::warning,
            "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
            "] FLVER identity hooks FAIL-OPEN: stock DSR preserved for exact owner routing.");
    }

    reshade::log::message(
        reshade::log::level::info,
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION
        "] READY: native Renderer Core A1 islands only; frozen legacy monolith "
        "is not linked or executed.");

    return true;
}

extern "C" __declspec(dllexport)
void AddonUninit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    unregister_events();

    // Preserve the full live identity telemetry before successful teardown
    // clears the hook/registry state. A restore failure is logged separately
    // after uninstall so it cannot masquerade as a clean shutdown.
    log_state("PRE_UNLOAD");
    dsrrl::runtime::flver_identity_transport::uninstall();
    if (dsrrl::runtime::flver_identity_transport::status().restore_failed)
        log_state("UNLOAD_RESTORE_FAIL");

    dsrrl::runtime::stable_receiver_pipeline_reset();
    g_a1_bridge.reset();
    disable_integrated_islands();

    reshade::unregister_addon(addon_module, reshade_module);
}
