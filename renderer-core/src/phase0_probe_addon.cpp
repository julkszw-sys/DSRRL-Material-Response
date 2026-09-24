#include "dsrrl/core/renderer_core.hpp"

#include <reshade.hpp>

namespace {
dsrrl::core::renderer_core g_core;
}

extern "C" __declspec(dllexport) const char *NAME =
    "DSRRL Renderer Core v1 Phase 0 Probe";
extern "C" __declspec(dllexport) const char *DESCRIPTION =
    "Pixel-inert stability probe. No operator islands, EXE hooks, shader replacements, resource replacements or draw replay.";

extern "C" __declspec(dllexport) bool AddonInit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    if (!reshade::register_addon(addon_module, reshade_module))
        return false;

    if (!g_core.phase0_pass_through()) {
        reshade::log::message(
            reshade::log::level::error,
            "[DSRRL CORE V1 P0] invariant failure: core is not pass-through");
        reshade::unregister_addon(addon_module, reshade_module);
        return false;
    }

    reshade::log::message(
        reshade::log::level::info,
        "[DSRRL CORE V1 P0] READY API20 PIXEL-INERT FEATURES=0 HOOKS=0 TX=0");

    return true;
}

extern "C" __declspec(dllexport) void AddonUninit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    reshade::unregister_addon(addon_module, reshade_module);
}
