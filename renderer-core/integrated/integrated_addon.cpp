#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/a1_create_pipeline_bridge.hpp"
#include "dsrrl/runtime/material_response_runtime_bridge.hpp"

#include <reshade.hpp>

#if RESHADE_API_VERSION != 20
#error DSRRL integrated construction addon requires ReShade Add-on API 20
#endif

#include <atomic>
#include <cstdint>
#include <cstdio>

namespace {

dsrrl::core::renderer_core g_core;

dsrrl::runtime::a1_create_pipeline_bridge
    g_a1_bridge(g_core.features());

dsrrl::runtime::material_response_runtime_bridge
    g_material_response_bridge(g_core.features());

std::atomic<std::uint64_t> g_present_count{0};
std::atomic_bool g_material_response_runtime_ready{false};

constexpr dsrrl::core::operator_id k_a1_islands[] = {
    dsrrl::core::operator_id::terminal_sat_rgb,
    dsrrl::core::operator_id::diffuse_material_domain,
    dsrrl::core::operator_id::pointlight_pnts_attenuation,
    dsrrl::core::operator_id::envspec_nospc_delete,
    dsrrl::core::operator_id::fixed_postfog_identity
};

bool enable_integrated_a1_islands() noexcept
{
    for (const auto op : k_a1_islands)
        if (!g_core.features().set(op, true))
            return false;

    return true;
}

void log_state(const char *tag) noexcept
{
    const auto a1 = g_a1_bridge.telemetry();
    const auto mr = g_material_response_bridge.telemetry();

    char line[1280]{};

    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL CORE INTEGRATED] %s "
        "A1{create=%llu candidate=%llu exact=%llu materialized=%llu unknown=%llu "
        "no_owner=%llu failopen=%llu init_ok=%llu init_bad=%llu binds=%llu quarantine=%u} "
        "MR{hooks=%u mtd=%llu donor=%llu unmapped=%llu selector=%llu selector_donor=%llu "
        "pipe=%llu alt=%llu binds=%llu draws=%llu c101=%llu c100=%llu "
        "b12_new=%llu b12_hit=%llu deferred=%llu failopen=%llu quarantine=%u}",
        tag,
        static_cast<unsigned long long>(a1.create_events),
        static_cast<unsigned long long>(a1.candidate_size_hits),
        static_cast<unsigned long long>(a1.exact_identity_hits),
        static_cast<unsigned long long>(a1.materialized),
        static_cast<unsigned long long>(a1.pass_unknown_identity),
        static_cast<unsigned long long>(a1.pass_no_enabled_owner),
        static_cast<unsigned long long>(a1.fail_open),
        static_cast<unsigned long long>(a1.init_attested),
        static_cast<unsigned long long>(a1.init_mismatch),
        static_cast<unsigned long long>(a1.target_binds),
        a1.quarantined ? 1u : 0u,
        mr.hooks_active ? 1u : 0u,
        static_cast<unsigned long long>(mr.mtd_seen),
        static_cast<unsigned long long>(mr.donor_registered),
        static_cast<unsigned long long>(mr.donor_unmapped),
        static_cast<unsigned long long>(mr.selector_seen),
        static_cast<unsigned long long>(mr.selector_donor),
        static_cast<unsigned long long>(mr.exact_pipeline_hits),
        static_cast<unsigned long long>(mr.alternate_pairs_ready),
        static_cast<unsigned long long>(mr.target_binds),
        static_cast<unsigned long long>(mr.replay_draws),
        static_cast<unsigned long long>(mr.c101_draws),
        static_cast<unsigned long long>(mr.c100_only_draws),
        static_cast<unsigned long long>(mr.b12_created),
        static_cast<unsigned long long>(mr.b12_cache_hits),
        static_cast<unsigned long long>(mr.deferred_fail_open),
        static_cast<unsigned long long>(mr.fail_open),
        mr.quarantined ? 1u : 0u);

    reshade::log::message(
        reshade::log::level::info,
        line);
}

void on_init_device(
    reshade::api::device *device)
{
    g_a1_bridge.on_init_device(device);

    if (g_material_response_runtime_ready.load())
        g_material_response_bridge.on_init_device(device);
}

void on_destroy_device(
    reshade::api::device *device)
{
    if (g_material_response_runtime_ready.load())
        g_material_response_bridge.on_destroy_device(device);

    g_a1_bridge.on_destroy_device(device);
}

bool on_create_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline_layout layout,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *
        subobjects)
{
    return g_a1_bridge.on_create_pipeline(
        device,
        layout,
        subobject_count,
        subobjects);
}

void on_init_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline_layout layout,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *
        subobjects,
    reshade::api::pipeline pipeline)
{
    g_a1_bridge.on_init_pipeline(
        device,
        layout,
        subobject_count,
        subobjects,
        pipeline);

    if (g_material_response_runtime_ready.load()) {
        g_material_response_bridge.on_init_pipeline(
            device,
            layout,
            subobject_count,
            subobjects,
            pipeline);
    }
}

void on_destroy_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline pipeline)
{
    if (g_material_response_runtime_ready.load()) {
        g_material_response_bridge.on_destroy_pipeline(
            device,
            pipeline);
    }

    g_a1_bridge.on_destroy_pipeline(
        device,
        pipeline);
}

void on_bind_pipeline(
    reshade::api::command_list *command_list,
    reshade::api::pipeline_stage stages,
    reshade::api::pipeline pipeline)
{
    std::uint16_t first_plan = 0xFFFFu;

    const bool target =
        g_a1_bridge.on_bind_pipeline(
            stages,
            pipeline,
            &first_plan);

    if (target &&
        first_plan != 0xFFFFu) {
        char line[160]{};

        std::snprintf(
            line,
            sizeof(line),
            "[DSRRL CORE INTEGRATED] FIRST_BIND plan=%u",
            static_cast<unsigned>(
                first_plan));

        reshade::log::message(
            reshade::log::level::info,
            line);
    }

    if (g_material_response_runtime_ready.load()) {
        g_material_response_bridge.on_bind_pipeline(
            command_list,
            stages,
            pipeline);
    }
}

bool on_draw_indexed(
    reshade::api::command_list *command_list,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance)
{
    if (!g_material_response_runtime_ready.load())
        return false;

    return g_material_response_bridge.on_draw_indexed(
        command_list,
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
    const auto present =
        ++g_present_count;

    if (present == 1u ||
        (present % 300u) == 0u)
        log_state("LIVE");
}

void register_events()
{
    reshade::register_event<
        reshade::addon_event::init_device>(
            on_init_device);

    reshade::register_event<
        reshade::addon_event::destroy_device>(
            on_destroy_device);

    reshade::register_event<
        reshade::addon_event::create_pipeline>(
            on_create_pipeline);

    reshade::register_event<
        reshade::addon_event::init_pipeline>(
            on_init_pipeline);

    reshade::register_event<
        reshade::addon_event::destroy_pipeline>(
            on_destroy_pipeline);

    reshade::register_event<
        reshade::addon_event::bind_pipeline>(
            on_bind_pipeline);

    reshade::register_event<
        reshade::addon_event::draw_indexed>(
            on_draw_indexed);

    reshade::register_event<
        reshade::addon_event::present>(
            on_present);
}

void unregister_events()
{
    reshade::unregister_event<
        reshade::addon_event::present>(
            on_present);

    reshade::unregister_event<
        reshade::addon_event::draw_indexed>(
            on_draw_indexed);

    reshade::unregister_event<
        reshade::addon_event::bind_pipeline>(
            on_bind_pipeline);

    reshade::unregister_event<
        reshade::addon_event::destroy_pipeline>(
            on_destroy_pipeline);

    reshade::unregister_event<
        reshade::addon_event::init_pipeline>(
            on_init_pipeline);

    reshade::unregister_event<
        reshade::addon_event::create_pipeline>(
            on_create_pipeline);

    reshade::unregister_event<
        reshade::addon_event::destroy_device>(
            on_destroy_device);

    reshade::unregister_event<
        reshade::addon_event::init_device>(
            on_init_device);
}

} // namespace

extern "C" __declspec(dllexport)
const char *NAME =
    "DSRRL Renderer Core Integrated Construction";

extern "C" __declspec(dllexport)
const char *AUTHOR =
    "DSR Restored Lighting";

extern "C" __declspec(dllexport)
const char *DESCRIPTION =
    "Single-addon Renderer Core construction target; exact A1 create-time islands plus exact material-response draw carrier.";

extern "C" __declspec(dllexport)
bool AddonInit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    if (!reshade::register_addon(
            addon_module,
            reshade_module))
        return false;

    g_a1_bridge.reset();
    g_present_count.store(0);
    g_material_response_runtime_ready.store(false);

    if (!enable_integrated_a1_islands()) {
        reshade::unregister_addon(
            addon_module,
            reshade_module);
        return false;
    }

    const bool material_response_ready =
        g_material_response_bridge.start();

    if (material_response_ready) {
        if (!g_core.features().set(
                dsrrl::core::operator_id::material_response,
                true)) {
            g_material_response_bridge.stop();
            reshade::unregister_addon(
                addon_module,
                reshade_module);
            return false;
        }

        g_material_response_runtime_ready.store(true);
    } else {
        static_cast<void>(
            g_core.features().set(
                dsrrl::core::operator_id::material_response,
                false));

        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE INTEGRATED] Material Response runtime carrier failed provenance/hook preflight; island disabled fail-open.");
    }

    register_events();

    reshade::log::message(
        reshade::log::level::info,
        "[DSRRL CORE INTEGRATED] READY API20 D3D11; five exact A1 create-time islands enabled; Material Response enabled only when exact retail provenance/hooks pass; resource islands still gated.");

    return true;
}

extern "C" __declspec(dllexport)
void AddonUninit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    unregister_events();
    log_state("UNLOAD");

    g_material_response_runtime_ready.store(false);
    static_cast<void>(
        g_core.features().set(
            dsrrl::core::operator_id::material_response,
            false));
    g_material_response_bridge.stop();

    g_a1_bridge.reset();

    reshade::unregister_addon(
        addon_module,
        reshade_module);
}
