#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/a1_create_pipeline_bridge.hpp"

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

std::atomic<std::uint64_t> g_present_count{0};

constexpr dsrrl::core::operator_id k_integrated_islands[] = {
    dsrrl::core::operator_id::terminal_sat_rgb,
    dsrrl::core::operator_id::diffuse_material_domain,
    dsrrl::core::operator_id::pointlight_pnts_attenuation,
    dsrrl::core::operator_id::envspec_nospc_delete,
    dsrrl::core::operator_id::fixed_postfog_identity
};

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

    char line[560]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL CORE+ISLANDS " DSRRL_CORE_ISLANDS_VERSION "] %s "
        "create=%llu candidate=%llu exact=%llu materialized=%llu "
        "unknown=%llu no_owner=%llu failopen=%llu init_ok=%llu "
        "init_bad=%llu binds=%llu quarantine=%u",
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
        t.quarantined ? 1u : 0u);

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
}

void on_destroy_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline pipeline)
{
    g_a1_bridge.on_destroy_pipeline(device, pipeline);
}

void on_bind_pipeline(
    reshade::api::command_list *,
    reshade::api::pipeline_stage stages,
    reshade::api::pipeline pipeline)
{
    std::uint16_t first_plan = 0xFFFFu;
    dsrrl::core::operator_mask selected_owners = 0u;
    std::uint16_t selected_ops = 0u;
    const bool target =
        g_a1_bridge.on_bind_pipeline(
            stages,
            pipeline,
            &first_plan,
            &selected_owners,
            &selected_ops);

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
    reshade::register_event<reshade::addon_event::present>(on_present);
}

void unregister_events()
{
    reshade::unregister_event<reshade::addon_event::present>(on_present);
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

    if (!enable_integrated_islands()) {
        disable_integrated_islands();
        reshade::unregister_addon(addon_module, reshade_module);
        return false;
    }

    register_events();

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
    log_state("UNLOAD");
    g_a1_bridge.reset();
    disable_integrated_islands();

    reshade::unregister_addon(addon_module, reshade_module);
}
