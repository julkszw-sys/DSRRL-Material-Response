#!/usr/bin/env python3
"""V464 exactly one integrated addon: new *owner identity* evidence only.
One preimage-gated diagnostic ChrIns router hook. NO LightBank change,
Source1/Source2 selection change, R43 change, shader/CB/resource patch or draw replay.
Runs after V459 overlay; source writes exist only inside the derived addon build.
"""
from pathlib import Path
R=Path("renderer-core")
s=R/"src/runtime/pmetal_env_source_runtime.cpp"
i=R/"integrated/integrated_addon.cpp"
h=R/"include/dsrrl/runtime/v438_one_shot_trace.hpp"
def rep(path,a,b):
    x=path.read_text(encoding="utf-8")
    assert x.count(a)==1,(str(path),a[:100],x.count(a))
    assert b not in x,("V464 duplicate overlay",str(path))
    path.write_text(x.replace(a,b,1),encoding="utf-8")
rep(s,
 '#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"',
 '#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"\n'
 '#include "dsrrl/runtime/v464_actor_owner.hpp"')
rep(i,
 '#include "dsrrl/core/renderer_core.hpp"',
 '#include "dsrrl/core/renderer_core.hpp"\n'
 '#include "dsrrl/runtime/v464_actor_owner.hpp"')
rep(h,'V451_MODEL_RESOURCE_AT_ISSUE=21',
      'V451_MODEL_RESOURCE_AT_ISSUE=21,V464_OWNER_HANDLE_AT_ISSUE=22')
rep(h,'t!=V451_MODEL_RESOURCE_AT_ISSUE &&',
      't!=V451_MODEL_RESOURCE_AT_ISSUE &&\n    t!=V464_OWNER_HANDLE_AT_ISSUE &&')
rep(s,
 '''    g_selector_enabled.store(
        true,
        std::memory_order_release);

#if defined(DSRRL_V398_ACTOR_TRANSACTIONAL_SOURCE)''',
 '''    g_selector_enabled.store(
        true,
        std::memory_order_release);
    // V464: optional diagnostic owner identity only; failure does NOT
    // disable or change the existing renderer source/operator paths.
    if (dsrrl::runtime::v464::install(g_source_base)) {
        reshade::log::message(reshade::log::level::warning,
          "[DSRRL V464] CHRINS_OWNER_ROUTER_TYPED_HANDLE probe=ON "
          "row_write=OFF actor_class=EXACT_VTABLE pixel=OPEN");
    } else {
        reshade::log::message(reshade::log::level::warning,
          "[DSRRL V464] CHRINS_OWNER_ROUTER_TYPED_HANDLE probe=FAIL_OPEN "
          "row_write=OFF actor_owner=UNKNOWN");
    }

#if defined(DSRRL_V398_ACTOR_TRANSACTIONAL_SOURCE)''')
rep(s,
 '''void pmetal_env_source_runtime::uninstall() noexcept
{''',
 '''void pmetal_env_source_runtime::uninstall() noexcept
{
    if(!dsrrl::runtime::v464::uninstall())
        reshade::log::message(reshade::log::level::error,
          "[DSRRL V464] CHRINS_OWNER_ROUTER_RESTORE_FAILED");''')
rep(i,
 '''            entity_vt459});''',
 '''            entity_vt459});
        // V464: read the actual registration HANDLE of this exact RX34
        // receiver. Join ONLY to an already observed typed ChrIns model
        // with identical full 32-bit handle (including generation), and
        // revalidate both pointers, class vtables and model relationship.
        // Notably, neither common FLVER nor source/row are actor keys.
        if(dsrrl::runtime::v438::state().active.load(std::memory_order_acquire) &&
           valid_builder451 && (light_status459&0xEu)==0xEu) {
            const auto owner464=dsrrl::runtime::v464::resolve(entity459,true);
            if(dsrrl::runtime::v464::first_or_transition(
                 entity459,owner464.entity_handle,owner464.status)) {
                dsrrl::runtime::v438::emit(
                    dsrrl::runtime::v438::V464_OWNER_HANDLE_AT_ISSUE,{
                      rx447.native_sequence,
                      static_cast<unsigned>(prepared.source_kind),
                      rx447.row_a,entity459,
                      owner464.entity_handle,owner464.status,
                      owner464.owner,owner464.actor_vt,
                      owner464.model,owner464.model_vt,
                      owner464.model_handle,owner464.candidates,
                      dp459,resource451,issued448.w.engine_model,
                      static_cast<unsigned>(issued448.status),
                      material.material_slot,material.route_index,
                      light_status459,
                      dsrrl::runtime::v464::g_router_hits.load(
                        std::memory_order_relaxed),
                      dsrrl::runtime::v464::g_actor_records.load(
                        std::memory_order_relaxed)});
            }
        }''')
print("V464 source overlay PASS: one guarded ChrIns owner hook + one deduped kind22 event, exact live full-handle join; actor-based rendering changes ZERO.")
