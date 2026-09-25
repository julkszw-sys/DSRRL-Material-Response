#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/a1_create_pipeline_bridge.hpp"
#include "dsrrl/runtime/integrated_feature_policy.hpp"

#include "dsrrl/runtime/asset_bridges.hpp"
#include "dsrrl/runtime/engine_hooks.hpp"
#include "dsrrl/runtime/envspec_runtime.hpp"
#include "dsrrl/runtime/mr_island.hpp"
#include "dsrrl/runtime/upper_lower_runtime.hpp"

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

std::atomic<std::uint64_t> g_present_count{0};
bool g_ul_producer_preflight_registered = false;
bool g_envspec_resource_preflight_registered = false;

bool apply_integrated_feature_policy() noexcept
{
    for (const auto &entry :
         dsrrl::runtime::k_integrated_feature_policy) {
        const bool enabled =
            entry.boot_policy ==
            dsrrl::runtime::integrated_boot_policy::enable_immediately;

        if (!g_core.features().set(entry.op, enabled))
            return false;
    }

    return true;
}

void disable_integrated_features() noexcept
{
    for (const auto &entry :
         dsrrl::runtime::k_integrated_feature_policy)
        (void)g_core.features().set(entry.op, false);
}

void log_state(const char *tag) noexcept
{
    const auto t = g_a1_bridge.telemetry();

    char line[640]{};

    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL CORE INTEGRATED A2] %s create=%llu candidate=%llu exact=%llu "
        "materialized=%llu unknown=%llu no_owner=%llu failopen=%llu "
        "init_ok=%llu init_bad=%llu binds=%llu quarantine=%u boot_islands=%llu",
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
        static_cast<unsigned long long>(
            dsrrl::runtime::integrated_boot_enabled_count()));

    reshade::log::message(
        reshade::log::level::info,
        line);
}

void a1_init_device(reshade::api::device *device)
{
    g_a1_bridge.on_init_device(device);
}

void a1_destroy_device(reshade::api::device *device)
{
    g_a1_bridge.on_destroy_device(device);
}

bool a1_create_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline_layout layout,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects)
{
    return g_a1_bridge.on_create_pipeline(
        device,
        layout,
        subobject_count,
        subobjects);
}

void a1_init_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline_layout layout,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects,
    reshade::api::pipeline pipeline)
{
    g_a1_bridge.on_init_pipeline(
        device,
        layout,
        subobject_count,
        subobjects,
        pipeline);
}

void a1_destroy_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline pipeline)
{
    g_a1_bridge.on_destroy_pipeline(
        device,
        pipeline);
}

void a1_bind_pipeline(
    reshade::api::command_list *,
    reshade::api::pipeline_stage stages,
    reshade::api::pipeline pipeline)
{
    std::uint16_t first_plan = 0xFFFFu;

    if (g_a1_bridge.on_bind_pipeline(
            stages,
            pipeline,
            &first_plan) &&
        first_plan != 0xFFFFu) {
        char line[160]{};
        std::snprintf(
            line,
            sizeof(line),
            "[DSRRL CORE INTEGRATED A2] FIRST_A1_BIND plan=%u",
            static_cast<unsigned>(first_plan));
        reshade::log::message(
            reshade::log::level::info,
            line);
    }
}

void a1_present(
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

void register_a1_events()
{
    reshade::register_event<reshade::addon_event::init_device>(a1_init_device);
    reshade::register_event<reshade::addon_event::destroy_device>(a1_destroy_device);
    reshade::register_event<reshade::addon_event::create_pipeline>(a1_create_pipeline);
    reshade::register_event<reshade::addon_event::init_pipeline>(a1_init_pipeline);
    reshade::register_event<reshade::addon_event::destroy_pipeline>(a1_destroy_pipeline);
    reshade::register_event<reshade::addon_event::bind_pipeline>(a1_bind_pipeline);
    reshade::register_event<reshade::addon_event::present>(a1_present);
}

void unregister_a1_events()
{
    reshade::unregister_event<reshade::addon_event::present>(a1_present);
    reshade::unregister_event<reshade::addon_event::bind_pipeline>(a1_bind_pipeline);
    reshade::unregister_event<reshade::addon_event::destroy_pipeline>(a1_destroy_pipeline);
    reshade::unregister_event<reshade::addon_event::init_pipeline>(a1_init_pipeline);
    reshade::unregister_event<reshade::addon_event::create_pipeline>(a1_create_pipeline);
    reshade::unregister_event<reshade::addon_event::destroy_device>(a1_destroy_device);
    reshade::unregister_event<reshade::addon_event::init_device>(a1_init_device);
}

void unregister_deferred_preflights() noexcept
{
    if (g_envspec_resource_preflight_registered) {
        dsrrl::runtime::envspec::unregister_runtime();
        g_envspec_resource_preflight_registered = false;
    }

    if (g_ul_producer_preflight_registered) {
        dsrrl::runtime::upper_lower::unregister_runtime();
        g_ul_producer_preflight_registered = false;
    }
}

void selector_dispatch(
    void *container,
    void *owner,
    void *ret,
    void *r14,
    void *r15,
    std::int32_t material_index) noexcept
{
    // One EngineBridge selector owner fans out immutable routing state.
    // MR may consume it immediately; U/L receives only an observational
    // producer/selector snapshot because its feature gate remains OFF.
    dsrrl::runtime::mr::selector_event(
        container,
        owner,
        ret,
        r14,
        r15,
        material_index);

    dsrrl::runtime::upper_lower::selector_event(
        container,
        owner,
        ret,
        r14,
        r15,
        material_index);
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
    "Single-addon Renderer Core A2 construction: exact create-time islands plus "
    "material/resource glue with fail-open routing.";

extern "C" __declspec(dllexport)
bool AddonInit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    if (!reshade::register_addon(
            addon_module,
            reshade_module))
        return false;

    if (!dsrrl::runtime::engine::verify_provenance()) {
        reshade::log::message(
            reshade::log::level::error,
            "[DSRRL CORE INTEGRATED A2] EXE/binder provenance mismatch; unload/fail-open.");
        reshade::unregister_addon(
            addon_module,
            reshade_module);
        return false;
    }

    g_a1_bridge.reset();
    g_present_count.store(0);

    if (!apply_integrated_feature_policy()) {
        disable_integrated_features();
        reshade::unregister_addon(
            addon_module,
            reshade_module);
        return false;
    }

    register_a1_events();

    if (!dsrrl::runtime::assets::register_runtime(g_core) ||
        !dsrrl::runtime::mr::register_runtime(g_core)) {
        unregister_deferred_preflights();
        dsrrl::runtime::mr::unregister_runtime();
        dsrrl::runtime::assets::unregister_runtime();
        unregister_a1_events();
        g_a1_bridge.reset();
        disable_integrated_features();
        reshade::unregister_addon(
            addon_module,
            reshade_module);
        return false;
    }

    // Remaining partial islands get pixel-inert producer/resource preflight
    // only. Failure here does not disable the already-closed visible islands.
    g_ul_producer_preflight_registered =
        dsrrl::runtime::upper_lower::register_runtime(
            g_core,
            {.capture_pmetal_env_source = false});

    g_envspec_resource_preflight_registered =
        dsrrl::runtime::envspec::register_runtime();

    if (!g_ul_producer_preflight_registered)
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE INTEGRATED A2] U/L producer preflight unavailable; deferred U/L/HemDir3 remain fail-open.");

    if (!g_envspec_resource_preflight_registered)
        reshade::log::message(
            reshade::log::level::warning,
            "[DSRRL CORE INTEGRATED A2] EnvSpec identity/resource preflight unavailable; deferred EnvSpec remains fail-open.");

    if (!dsrrl::runtime::engine::install(
            &selector_dispatch,
            &dsrrl::runtime::mr::mtd_event,
            &dsrrl::runtime::assets::texture_name_event,
            &dsrrl::runtime::assets::texture_name_clear_event)) {
        dsrrl::runtime::mr::unregister_runtime();
        dsrrl::runtime::assets::unregister_runtime();
        unregister_a1_events();
        g_a1_bridge.reset();
        disable_integrated_features();
        reshade::log::message(
            reshade::log::level::error,
            "[DSRRL CORE INTEGRATED A2] EngineBridge install failed; unload/fail-open.");
        reshade::unregister_addon(
            addon_module,
            reshade_module);
        return false;
    }

    char ready_line[512]{};
    std::snprintf(
        ready_line,
        sizeof(ready_line),
        "[DSRRL CORE INTEGRATED A2] READY: 11 visible islands armed; "
        "UL_producer_preflight=%s EnvSpec_resource_preflight=%s; "
        "U/L consumer, legacy EnvSpec consumer, full PointLight and P_Metal V13/source remain OFF.",
        g_ul_producer_preflight_registered ? "PASS" : "FAIL_OPEN",
        g_envspec_resource_preflight_registered ? "PASS" : "FAIL_OPEN");
    reshade::log::message(
        reshade::log::level::info,
        ready_line);

    return true;
}

extern "C" __declspec(dllexport)
void AddonUninit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    dsrrl::runtime::engine::uninstall();
    unregister_deferred_preflights();
    dsrrl::runtime::mr::unregister_runtime();
    dsrrl::runtime::assets::unregister_runtime();
    unregister_a1_events();

    log_state("UNLOAD");
    g_a1_bridge.reset();
    disable_integrated_features();

    reshade::unregister_addon(
        addon_module,
        reshade_module);
}
