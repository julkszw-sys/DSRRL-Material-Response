#!/usr/bin/env python3
"""V352 additive 30-second QPC Present-cadence profiler + cumulative pipeline stats.
Apply AFTER V351 materializer and BEFORE native CMake build. Never modifies
the material, resource, shader or Draw execution path.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/"integrated/integrated_addon.cpp"
s=p.read_text(encoding="utf-8")

def once(old,new,name):
    global s
    if s.count(old)!=1:
        raise RuntimeError(f"V352 drift {name}: expected one anchor, got {s.count(old)}")
    s=s.replace(old,new,1)

once('#include "dsrrl/runtime/runtime_hot_telemetry.hpp"',
'''#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#if defined(DSRRL_V352_LONG_SOAK_PRESENT)
#include "dsrrl/runtime/v352_long_soak_present_monitor.hpp"
#endif''',"include")
once('std::atomic<std::uint64_t> g_present_count{0};',
'''std::atomic<std::uint64_t> g_present_count{0};
#if defined(DSRRL_V352_LONG_SOAK_PRESENT)
dsrrl::runtime::v352::long_soak_present_monitor g_v352_soak{};
#endif''',"instance")
once('''    g_present_count.store(0);
    g_mr_draw_eval.store(0);''',
'''    g_present_count.store(0);
#if defined(DSRRL_V352_LONG_SOAK_PRESENT)
    g_v352_soak.reset();
#endif
    g_mr_draw_eval.store(0);''',"reset")
once('''    g_clustered_pnts.frame_event(present);
    dsrrl::runtime::v230_failopen_diag::frame();
    if (present == 1u ||
        (g_hot_telemetry_enabled &&
         (present % 300u) == 0u)) {''',
'''    g_clustered_pnts.frame_event(present);
    dsrrl::runtime::v230_failopen_diag::frame();
#if defined(DSRRL_V352_LONG_SOAK_PRESENT)
    const bool telemetry_window = g_v352_soak.present();
#else
    const bool telemetry_window = (present % 300u) == 0u;
#endif
    if (present == 1u ||
        (g_hot_telemetry_enabled && telemetry_window)) {''',"present interval")
once('''    if (g_effect_telemetry_enabled &&
        (present == 1u ||
         (present % 300u) == 0u))
        log_effect_matrix("LIVE");''',
'''    if (g_effect_telemetry_enabled &&
        (present == 1u || telemetry_window))
        log_effect_matrix("LIVE");''',"effect log 30s")
p.write_text(s,encoding="utf-8")
print("V352_30_SECOND_PRESENT_PERF_AND_RUNTIME_CUMULATIVE_LOGGING_PATCH_PASS")
