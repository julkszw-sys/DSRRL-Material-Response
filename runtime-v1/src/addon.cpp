#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <reshade.hpp>
#include <Windows.h>

#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/engine_hooks.hpp"
#include "dsrrl/runtime/mr_island.hpp"

#include <sstream>

#if RESHADE_API_VERSION != 20
#error DSRRL Renderer Core Runtime v1 requires ReShade Add-on API 20
#endif

namespace {
dsrrl::core::renderer_core g_core;

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
    "Renderer Core v1 runtime integration. Material Response V2.11 active through "
    "Core-owned hooks and transaction manager. SpecRGB/Diffuse/Normal/UpperLower "
    "remain feature-gated until their runtime islands are integrated.";
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

    g_core.features().set(dsrrl::core::operator_id::material_response,true);
    g_core.features().set(dsrrl::core::operator_id::spec_rgb,false);
    g_core.features().set(dsrrl::core::operator_id::diffuse,false);
    g_core.features().set(dsrrl::core::operator_id::normal,false);
    g_core.features().set(dsrrl::core::operator_id::upper_lower,false);

    if(!dsrrl::runtime::mr::register_runtime(g_core)){
        reshade::unregister_addon(addon,reshade_module);
        return false;
    }

    if(!dsrrl::runtime::engine::install(&selector_dispatch,&dsrrl::runtime::mr::mtd_event)){
        dsrrl::runtime::mr::unregister_runtime();
        reshade::log::message(reshade::log::level::error,
            "DSRRL Runtime v1: EngineBridge hook install failed; fail-open/unload.");
        reshade::unregister_addon(addon,reshade_module);
        return false;
    }

    reshade::log::message(reshade::log::level::info,
        "DSRRL Runtime v1: CORE ACTIVE; MR=ON SPECRGB=OFF DIFFUSE=OFF NORMAL=OFF UL=OFF; "
        "single selector owner; exact EXE+binder provenance PASS.");
    return true;
}

extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon,HMODULE reshade_module)
{
    dsrrl::runtime::engine::uninstall();
    dsrrl::runtime::mr::unregister_runtime();
    g_core.features().set(dsrrl::core::operator_id::material_response,false);
    reshade::unregister_addon(addon,reshade_module);
}

BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID)
{
    if(reason==DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(h);
    return TRUE;
}
