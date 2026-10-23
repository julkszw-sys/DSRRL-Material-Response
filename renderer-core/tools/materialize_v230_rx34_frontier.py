#!/usr/bin/env python3
"""Bounded receiver-first V230 RX34 earliest-failing-phase instrumentation.
Owner log source1 exact material RX34 has atomic_prepare_fail on five samples;
zeroed prepared fields after false return do not evidence absent PTDE payload.
No runtime shader/resource mutation.
"""
from pathlib import Path
p=Path("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
s=p.read_text(encoding="utf-8")
def swap(a,b):
    global s
    n=s.count(a)
    if n!=1:raise SystemExit("RX34_FRONTIER_SOURCE_DRIFT "+repr(a[:80])+" "+str(n))
    s=s.replace(a,b,1)
swap("std::atomic_bool g_source_cut_logged{false};",
"""#if defined(DSRRL_V230_RX34_FRONTIER)
std::atomic<std::uint32_t> g_rx34_frontier_log_count{0u};
#endif
std::atomic_bool g_source_cut_logged{false};""")
swap("""    pmetal_envspec_receiver_family family,
    prepared_pmetal_envspec_draw &prepared) noexcept
{""",
"""    pmetal_envspec_receiver_family family,
    prepared_pmetal_envspec_draw &prepared) noexcept
{
    prepared = {};
#if defined(DSRRL_V230_RX34_FRONTIER)
    std::uint32_t v230_prepare_phase=0u;
    struct v230_prepare_frontier {
        const mr::material_identity& m;
        const mr::decision& d;
        const prepared_pmetal_envspec_draw& result;
        const std::uint32_t& phase;
        ~v230_prepare_frontier() noexcept {
            if (result.ready || d.receiver_id!=34u ||
                m.route_index!=345u || m.material_slot!=2u ||
                !hashing::matches_hex(m.flver_sha256,
                  "3a82e4f6e45f13a05ac40418ba6ad12ff9acae9cf01e19937bd5c5f45ff2636c") ||
                !hashing::matches_hex(m.raw_mtd_sha256,
                  "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b"))
                return;
            const auto n=g_rx34_frontier_log_count.fetch_add(1u,std::memory_order_relaxed)+1u;
            if(n<=24u || n==32u || n==64u){
                char b[290]{};
                std::snprintf(b,sizeof(b),
                  "[DSRRL RX34 FRONTIER] n=%u last_passed_phase=%u route=%u slot=%u owner=%u active=%u",
                  n,phase,m.route_index,m.material_slot,
                  m.owner_tuple_exact?1u:0u,d.active?1u:0u);
                reshade::log::message(reshade::log::level::info,b);
            }
        }
    } v230_frontier{material,decision,prepared,v230_prepare_phase};
#endif
""")
stages=["material","feature","semantic","source","receiver_source","replacement","probe","spec_rgb","b12"]
for i,key in enumerate(stages,1):
    old=f"    effect_latch(effect_{key}_ready_);"
    swap(old,old+f"\n#if defined(DSRRL_V230_RX34_FRONTIER)\n    v230_prepare_phase={i}u;\n#endif")
p.write_text(s,encoding="utf-8")
print("V230 RX34 FRONTIER source patch PASS; phase 0-9; no replacement behavior changed")
