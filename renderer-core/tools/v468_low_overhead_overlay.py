#!/usr/bin/env python3
"""V468 performance isolation (read-only): no native ChrIns detour, no
new renderer hooks; preserve V467's strictly typed WorldChrMan RX34 probe
only for an already active 25-second auto capture.

Never change LightBank, shaders, PS/CB/SRV, materials, Source1/2/R43,
draw replay or vanilla EXE bytes on disk. Single integrated addon.
"""
from pathlib import Path

R=Path("renderer-core")
F=R/"src/runtime/flver_engine_hooks.cpp"
I=R/"integrated/integrated_addon.cpp"
H=R/"include/dsrrl/runtime/v438_one_shot_trace.hpp"

def replace_exact(p,old,new,label):
 s=p.read_text(encoding="utf8")
 count=s.count(old)
 assert count==1,(label,count)
 p.write_text(s.replace(old,new,1),encoding="utf8")

# V465 created an always-on native detour, even though V466's 25s window
# recorded 0 owner hits on an otherwise live 276-draw RX34 route.
# Disable native observer installation entirely; keep the original code
# untouched and explicitly mark the diagnostic-only hook as disabled.
replace_exact(
 F,
 " if(v465_prep_chrins()){",
 " if(false && v465_prep_chrins()){",
 "no native ChrIns detour")
replace_exact(
 F,
 "[DSRRL V465] CHRINS_OWNER_HOOK_FAILOPEN PREIMAGE_MISMATCH",
 "[DSRRL V468] CHRINS_OWNER_HOOK_DISABLED_PERF original_game_code=1 no_native_detour=1",
 "explicit disabled status")
replace_exact(
 H,
 "source_hook_call=OPEN_UNTIL_ENTRY_SEEN",
 "source_hook_call=DISABLED_V468_NOT_TESTED",
 "native entry count cannot be interpreted")

# V465 recorded receiver identity beyond the actual active diagnostic
# window because entity+0x10 was read before observe_draw's active guard.
# Avoid those pointer validation reads outside capture.
replace_exact(
 I,
 """        if(valid_builder451 && base459 && entity459 &&
           dsrrl::runtime::v443::read(entity459,vt465) &&""",
 """        if(dsrrl::runtime::v438::state().active.load(
               std::memory_order_relaxed) &&
           valid_builder451 && base459 && entity459 &&
           dsrrl::runtime::v443::read(entity459,vt465) &&""",
 "receiver handle read active only")

# V467's typed global -> PlayerIns -> ChrModel -> full handle probe also ran
# outside capture, even though observe_world_player() discarded those
# results. Guard BEFORE its first v443::read / VirtualQuery call.
replace_exact(
 I,
 """        if(static_cast<unsigned>(prepared.source_kind)==1u &&
           (status465&7u)==7u) {""",
 """        if(dsrrl::runtime::v438::state().active.load(
               std::memory_order_relaxed) &&
           static_cast<unsigned>(prepared.source_kind)==1u &&
           (status465&7u)==7u) {""",
 "WorldChrMan typed probe active only")

# Pre-trigger RX34 eligible-draw counting is required for auto start, but
# no longer needs an atomic increment after the one-shot test finishes.
replace_exact(
 H,
 """inline void note_rx34() noexcept {
 samples().scene_rx34.fetch_add(1u,std::memory_order_relaxed);
}""",
 """inline void note_rx34() noexcept {
 if(dsrrl::runtime::v438::state().finished.load(
      std::memory_order_relaxed))return;
 samples().scene_rx34.fetch_add(1u,std::memory_order_relaxed);
}""",
 "one-shot RX34 trigger counter")

print("V468 overlay PASS: native ChrIns hook DISABLED; receiver/world VM reads ONLY inside active capture; one addon; no renderer mutation")
