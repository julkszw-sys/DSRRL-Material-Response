#!/usr/bin/env python3
"""V466: preserve PRE-window typed ChrIns owner observations, never promote them to a live join.

Run after the V465 native owner hook and Ally log-only overlays. Modifies ONLY
read-only diagnostics in the same compiled ReShade addon; no new hook, no
LightBank/CB/shader/Source1/Source2/R43 or on-disk EXE modification.
"""
from pathlib import Path

ROOT = Path("renderer-core")
HOOK = ROOT / "src/runtime/flver_engine_hooks.cpp"
TRACE = ROOT / "include/dsrrl/runtime/v438_one_shot_trace.hpp"

def replace_exact(text, old, new, label):
    assert text.count(old) == 1, (label, text.count(old))
    return text.replace(old, new, 1)

src = HOOK.read_text(encoding="utf-8")
src = replace_exact(
    src,
    """if(owner!=nullptr &&
       dsrrl::runtime::v438::state().active.load(std::memory_order_acquire)) {""",
    """// V466: record bounded, read-only owner sightings even before RX34
    // triggers its automatic window. A pre-window sighting is NOT a live join.
    if(owner!=nullptr &&
       !dsrrl::runtime::v438::state().finished.load(std::memory_order_acquire)) {""",
    "owner hook prewindow")
HOOK.write_text(src, encoding="utf-8")

text = TRACE.read_text(encoding="utf-8")
text = replace_exact(
    text,
    "std::uint64_t owner_hits=0u,draw_hits=0u,row30=0u,row31=0u;",
    "std::uint64_t owner_hits=0u,pre_owner_hits=0u,live_owner_hits=0u,\n"
    "               draw_hits=0u,row30=0u,row31=0u;",
    "per-handle event phases")
text = replace_exact(
    text,
    """inline void observe_owner(
    std::uintptr_t owner,std::uintptr_t model,std::uint32_t h,
    std::uint32_t type,std::uint32_t flags) noexcept {
 if(!dsrrl::runtime::v438::state().active.load(std::memory_order_acquire))return;""",
    """inline void observe_owner(
    std::uintptr_t owner,std::uintptr_t model,std::uint32_t h,
    std::uint32_t type,std::uint32_t flags) noexcept {
 // The native owner method can run before the RX34 capture trigger.
 // Keep prior sightings as CANDIDATES, never as same-window owner proof.
 auto &session=dsrrl::runtime::v438::state();
 if(session.finished.load(std::memory_order_acquire))return;
 const bool in_window=session.active.load(std::memory_order_acquire);""",
    "owner capture phase")
text = replace_exact(
    text,
    "   ++entry->owner_hits;\n",
    "   ++entry->owner_hits;\n"
    "   if(in_window)++entry->live_owner_hits;\n"
    "   else ++entry->pre_owner_hits;\n",
    "owner phase counters")
text = replace_exact(
    text,
    " unsigned entries=0u,joined=0u,unknown=0u,conflicts=0u;\n"
    " std::uint64_t owners=0u,draws=0u;",
    " unsigned entries=0u,joined=0u,unknown=0u,conflicts=0u,prior_only=0u;\n"
    " std::uint64_t owners=0u,draws=0u,preowners=0u,liveowners=0u;",
    "summary initializers")
text = replace_exact(
    text,
    "    ++entries;owners+=v.owner_hits;draws+=v.draw_hits;",
    "    ++entries;owners+=v.owner_hits;draws+=v.draw_hits;\n"
    "    preowners+=v.pre_owner_hits;liveowners+=v.live_owner_hits;",
    "summary phase totals")
text = replace_exact(
    text,
    """    const bool joined_ok=v.owner && v.entity &&
      (v.owner_type==1u || v.owner_type==2u) &&
      !v.ownership_conflict && !v.entity_conflict &&
      ((v.row30>0u) != (v.row31>0u));
    if(joined_ok)++joined;else ++unknown;""",
    """    const bool identity_candidate=v.owner && v.entity &&
      (v.owner_type==1u || v.owner_type==2u) &&
      !v.ownership_conflict && !v.entity_conflict &&
      (v.row30>0u || v.row31>0u);
    const bool joined_ok=identity_candidate && v.live_owner_hits>0u;
    const bool prior_candidate=identity_candidate && !joined_ok &&
      v.pre_owner_hits>0u;
    if(joined_ok)++joined;
    else {
      ++unknown; // PRETRIGGER_ONLY is never counted as CPU_JOIN.
      if(prior_candidate)++prior_only;
    }
    // Same native handle can issue both rows across time. Not same-pixel proof.
    const char *row_scope=(v.row30>0u && v.row31>0u)?"BOTH_ROWS":
      v.row30>0u?"ROW30":v.row31>0u?"ROW31":"NONE";""",
    "no historical owner promoted to live join")
text = replace_exact(
    text,
    """      "[DSRRL V465] ACTOR_HANDLE status=%s handle=%08x class=%s owner=%llx model=%llx entity=%llx row30=%llu row31=%llu owner_hits=%llu draw_hits=%llu conflicting_owner=%u conflicting_receiver=%u",
      joined_ok?"CPU_JOIN":"UNKNOWN",v.full_handle,""",
    """      "[DSRRL V466] ACTOR_HANDLE status=%s row_scope=%s handle=%08x class=%s owner=%llx model=%llx entity=%llx row30=%llu row31=%llu owner_hits=%llu pre_owner_hits=%llu live_owner_hits=%llu draw_hits=%llu conflicting_owner=%u conflicting_receiver=%u",
      joined_ok?"CPU_JOIN":prior_candidate?"PRETRIGGER_ONLY":"UNKNOWN",row_scope,v.full_handle,""",
    "phase-aware log marker")
text = replace_exact(
    text,
    """      static_cast<unsigned long long>(v.owner_hits),
      static_cast<unsigned long long>(v.draw_hits),""",
    """      static_cast<unsigned long long>(v.owner_hits),
      static_cast<unsigned long long>(v.pre_owner_hits),
      static_cast<unsigned long long>(v.live_owner_hits),
      static_cast<unsigned long long>(v.draw_hits),""",
    "phase-aware log arguments")
text = replace_exact(
    text,
    """   "[DSRRL V465] SUMMARY handles=%u joined=%u unknown=%u conflicts=%u owner_samples=%llu rx34_draw_samples=%llu rx34_trigger_hits=%llu invalid_owner=%llu invalid_draw=%llu lock_drops=%llu overflow=%llu actor_pixel=OPEN PTDE_pixel=OPEN",
   entries,joined,unknown,conflicts,
   static_cast<unsigned long long>(owners),
   static_cast<unsigned long long>(draws),""",
    """   "[DSRRL V466] SUMMARY handles=%u live_joined=%u prior_only=%u unknown=%u conflicts=%u owner_samples=%llu pre_owner_samples=%llu live_owner_samples=%llu rx34_draw_samples=%llu rx34_trigger_hits=%llu invalid_owner=%llu invalid_draw=%llu lock_drops=%llu overflow=%llu actor_pixel=OPEN PTDE_pixel=OPEN",
   entries,joined,prior_only,unknown,conflicts,
   static_cast<unsigned long long>(owners),
   static_cast<unsigned long long>(preowners),
   static_cast<unsigned long long>(liveowners),
   static_cast<unsigned long long>(draws),""",
    "summary phase counters and labels")
# Prevent an observer that races with window closure from being newly accepted
# while final aggregation is already underway. The existing mutex still
# protects the bounded table; no new locks or callbacks are introduced.
text = replace_exact(
    text,
    """ s.active.store(false,std::memory_order_release);
 dsrrl::runtime::v465::finish();
 s.finished.store(true,std::memory_order_release);""",
    """ s.active.store(false,std::memory_order_release);
 s.finished.store(true,std::memory_order_release);
 dsrrl::runtime::v465::finish();""",
    "finished flag before reporting")
TRACE.write_text(text, encoding="utf-8")
print("V466 PASS: pre-window owner sightings retained as non-authoritative; CPU_JOIN requires a live owner hit; same one addon")
