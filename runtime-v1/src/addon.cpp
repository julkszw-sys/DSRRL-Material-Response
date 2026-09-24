#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <reshade.hpp>
#include <Windows.h>

#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/a1_create_pipeline_bridge.hpp"
#include "dsrrl/runtime/engine_hooks.hpp"
#include "dsrrl/runtime/mr_island.hpp"
#include "dsrrl/runtime/asset_bridges.hpp"

#include <atomic>
#include <cstdio>
#include <sstream>

#if RESHADE_API_VERSION != 20
#error DSRRL Renderer Core Runtime v1 requires ReShade Add-on API 20
#endif

namespace {
dsrrl::core::renderer_core g_core;
dsrrl::runtime::a1_create_pipeline_bridge g_a1_bridge(g_core.features());
std::atomic<std::uint64_t> g_a1_present_count{0};

constexpr dsrrl::core::operator_id k_a1_islands[] = {
    dsrrl::core::operator_id::terminal_sat_rgb,
    dsrrl::core::operator_id::diffuse_material_domain,
    dsrrl::core::operator_id::pointlight_pnts_attenuation,
    dsrrl::core::operator_id::envspec_nospc_delete,
    dsrrl::core::operator_id::fixed_postfog_identity
};

bool enable_a1_islands() noexcept
{
    for (const auto op : k_a1_islands)
        if (!g_core.features().set(op, true))
            return false;
    return true;
}

void log_a1_state(const char *tag) noexcept
{
    const auto t=g_a1_bridge.telemetry();
    char line[512]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL Runtime v1 A1] %s create=%llu candidate=%llu exact=%llu materialized=%llu "
        "unknown=%llu no_owner=%llu failopen=%llu init_ok=%llu init_bad=%llu binds=%llu quarantine=%u",
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
        t.quarantined?1u:0u);
    reshade::log::message(reshade::log::level::info,line);
}

void a1_init_device(reshade::api::device *d){g_a1_bridge.on_init_device(d);}
void a1_destroy_device(reshade::api::device *d){g_a1_bridge.on_destroy_device(d);}
bool a1_create_pipeline(reshade::api::device *d,reshade::api::pipeline_layout l,
                        std::uint32_t n,const reshade::api::pipeline_subobject *so)
{return g_a1_bridge.on_create_pipeline(d,l,n,so);}
void a1_init_pipeline(reshade::api::device *d,reshade::api::pipeline_layout l,
                      std::uint32_t n,const reshade::api::pipeline_subobject *so,
                      reshade::api::pipeline p)
{g_a1_bridge.on_init_pipeline(d,l,n,so,p);}
void a1_destroy_pipeline(reshade::api::device *d,reshade::api::pipeline p)
{g_a1_bridge.on_destroy_pipeline(d,p);}
void a1_bind_pipeline(reshade::api::command_list *,reshade::api::pipeline_stage stages,
                      reshade::api::pipeline p)
{
    std::uint16_t first=0xffffu;
    if(g_a1_bridge.on_bind_pipeline(stages,p,&first) && first!=0xffffu){
        char line[160]{};
        std::snprintf(line,sizeof(line),"[DSRRL Runtime v1 A1] FIRST_BIND plan=%u",
                      static_cast<unsigned>(first));
        reshade::log::message(reshade::log::level::info,line);
    }
}
void a1_present(reshade::api::command_queue *,reshade::api::swapchain *,
                const reshade::api::rect *,const reshade::api::rect *,
                std::uint32_t,const reshade::api::rect *)
{
    const auto p=++g_a1_present_count;
    if(p==1u || (p%300u)==0u) log_a1_state("LIVE");
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

void selector_dispatch(void *container,void *owner,void *ret,void *r14,void *r15,
                       std::int32_t material_index) noexcept
{
    dsrrl::runtime::mr::selector_event(container,owner,ret,r14,r15,material_index);
    // Upper/Lower will subscribe here; no second selector detour is permitted.
}

} // namespace

extern "C" dsrrl::runtime::engine::selector_callback dsrrl_runtime_selector_dispatch() noexcept
{
    return &selector_dispatch;
}

extern "C" {
__declspec(dllexport) const char *NAME="DSRRL Renderer Core Runtime v1";
__declspec(dllexport) const char *AUTHOR="DSR Restored Lighting";
__declspec(dllexport) const char *DESCRIPTION=
    "Renderer Core v1 A2 unified runtime. Proven A1 create-time islands plus "
    "Material Response V2.11 and exact V12-gated PTDE Diffuse t0 / Normal t2 "
    "resource islands in one addon. SpecRGB and Upper/Lower remain fail-open.";
}

extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon,HMODULE reshade_module)
{
    if(!reshade::register_addon(addon,reshade_module))
        return false;

    if(!dsrrl::runtime::engine::verify_provenance()){
        reshade::log::message(reshade::log::level::error,
            "DSRRL Runtime v1: EXE/binder provenance mismatch; fail-open/unload.");
        reshade::unregister_addon(addon,reshade_module);
        return false;
    }

    g_a1_bridge.reset();
    g_a1_present_count.store(0);
    if(!enable_a1_islands()){
        reshade::unregister_addon(addon,reshade_module);
        return false;
    }

    g_core.features().set(dsrrl::core::operator_id::material_response,true);
    g_core.features().set(dsrrl::core::operator_id::spec_rgb,true);
    g_core.features().set(dsrrl::core::operator_id::diffuse,true);
    g_core.features().set(dsrrl::core::operator_id::normal,true);
    g_core.features().set(dsrrl::core::operator_id::upper_lower,false);

    register_a1_events();

    if(!dsrrl::runtime::assets::register_runtime(g_core) ||
       !dsrrl::runtime::mr::register_runtime(g_core)){
        dsrrl::runtime::assets::unregister_runtime();
        dsrrl::runtime::mr::unregister_runtime();
        unregister_a1_events();
        g_a1_bridge.reset();
        reshade::unregister_addon(addon,reshade_module);
        return false;
    }

    if(!dsrrl::runtime::engine::install(
            &selector_dispatch,
            &dsrrl::runtime::mr::mtd_event,
            &dsrrl::runtime::assets::texture_name_event,
            &dsrrl::runtime::assets::texture_name_clear_event)){
        dsrrl::runtime::mr::unregister_runtime();
        dsrrl::runtime::assets::unregister_runtime();
        unregister_a1_events();
        g_a1_bridge.reset();
        reshade::log::message(reshade::log::level::error,
            "DSRRL Runtime v1: EngineBridge hook install failed; fail-open/unload.");
        reshade::unregister_addon(addon,reshade_module);
        return false;
    }

    reshade::log::message(reshade::log::level::info,
        "DSRRL Runtime v1 A2: CORE ACTIVE; A1x5=ON MR=ON SPECRGB=ON DIFFUSE=ON NORMAL=ON UL=OFF; "
        "single EngineBridge hook owner; A1/MR exact-SHA sets disjoint; exact EXE+binder provenance PASS.");
    return true;
}

extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon,HMODULE reshade_module)
{
    dsrrl::runtime::engine::uninstall();
    dsrrl::runtime::mr::unregister_runtime();
    dsrrl::runtime::assets::unregister_runtime();
    unregister_a1_events();
    log_a1_state("UNLOAD");
    g_a1_bridge.reset();
    g_core.features().set(dsrrl::core::operator_id::material_response,false);
    g_core.features().set(dsrrl::core::operator_id::diffuse,false);
    g_core.features().set(dsrrl::core::operator_id::normal,false);
    reshade::unregister_addon(addon,reshade_module);
}

BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID)
{
    if(reason==DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(h);
    return TRUE;
}
