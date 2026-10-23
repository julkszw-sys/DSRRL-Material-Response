#!/usr/bin/env python3
"""Move exact LightBank producer invalidation behind attested owner gates.
Apply after V230 cleanup and cutscene trace; no fallback/PS/resource change."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"src/runtime/pmetal_env_source_runtime.cpp"
s=p.read_text("utf-8")
def swap(a,b):
    global s
    if s.count(a)!=1:raise RuntimeError("SOURCE1 anchor drift "+repr(a[:65])+" "+str(s.count(a)))
    s=s.replace(a,b,1)
swap("""    // This exact material now owns the next synchronized source publication.
    // Invalidate any older selector fallback before downstream decode.
    pmetal_producer_state_begin(
        material,
        epoch);

    if (telemetry::effect_enabled())""",
"""    // Foreign selector callsites MUST NOT invalidate this exact material.
    if (telemetry::effect_enabled())""")
swap("""    if (descriptor_owner != owner ||
        character > 1u)
        return;

    if (telemetry::effect_enabled())""",
"""    if (descriptor_owner != owner ||
        character > 1u)
        return;

    // Authenticated selector owner/callsite; genuine bad endpoints still
    // invalidate previous exact state and safely fall back to stock/hook.
    pmetal_producer_state_begin(material, epoch);
#if defined(DSRRL_V230_SOURCE1_REPAIR)
    g_source1_descriptor.fetch_add(1u,std::memory_order_relaxed);
#endif
    if (telemetry::effect_enabled())""")
swap("std::atomic_bool g_selector_exact_seen{false};",
"""#if defined(DSRRL_V230_SOURCE1_REPAIR)
std::atomic<std::uint64_t> g_source1_offer{0},g_source1_parent{0};
std::atomic<std::uint64_t> g_source1_descriptor{0},g_source1_publish{0};
std::atomic<std::uint64_t> g_source1_exact{0},g_source1_hook{0};
std::atomic<std::uint64_t> g_source1_reads{0};
void source1_report() noexcept {
    const auto n=g_source1_reads.fetch_add(1u,std::memory_order_relaxed)+1u;
    if(n!=128u && n!=1024u && n!=4096u && n!=16384u) return;
    char buf[350]{};
    std::snprintf(buf,sizeof(buf),
      "[DSRRL SOURCE1] reads=%llu offered=%llu parent=%llu descriptor=%llu published=%llu exact=%llu hook=%llu",
      (unsigned long long)n,
      (unsigned long long)g_source1_offer.load(),
      (unsigned long long)g_source1_parent.load(),
      (unsigned long long)g_source1_descriptor.load(),
      (unsigned long long)g_source1_publish.load(),
      (unsigned long long)g_source1_exact.load(),
      (unsigned long long)g_source1_hook.load());
    reshade::log::message(reshade::log::level::warning,buf);
}
#endif
std::atomic_bool g_selector_exact_seen{false};""")
swap("""    // Foreign selector callsites MUST NOT invalidate this exact material.""",
"""#if defined(DSRRL_V230_SOURCE1_REPAIR)
    g_source1_offer.fetch_add(1u,std::memory_order_relaxed);
#endif
    // Foreign selector callsites MUST NOT invalidate this exact material.""")
swap("""    if (telemetry::effect_enabled())
        g_parent_gate_ok.store(""",
"""#if defined(DSRRL_V230_SOURCE1_REPAIR)
    g_source1_parent.fetch_add(1u,std::memory_order_relaxed);
#endif
    if (telemetry::effect_enabled())
        g_parent_gate_ok.store(""")
swap("""        pmetal_producer_state_publish(
            material,
            next,
            epoch);

        static std::atomic_bool""",
"""        pmetal_producer_state_publish(material,next,epoch);
#if defined(DSRRL_V230_SOURCE1_REPAIR)
        g_source1_publish.fetch_add(1u,std::memory_order_relaxed);
#endif
        static std::atomic_bool""")
swap("""    pmetal_producer_state_publish(
        material,
        next,
        epoch);

    if (telemetry::effect_enabled())""",
"""    pmetal_producer_state_publish(material,next,epoch);
#if defined(DSRRL_V230_SOURCE1_REPAIR)
    g_source1_publish.fetch_add(1u,std::memory_order_relaxed);
#endif
    if (telemetry::effect_enabled())""")
swap("""    const bool local_valid =
        pmetal_producer_state_valid();""",
"""#if defined(DSRRL_V230_SOURCE1_REPAIR)
    source1_report();
#endif
    const bool local_valid =
        pmetal_producer_state_valid();""")
swap("""    if (pmetal_producer_state_latest(
            material,
            epoch,
            out)) {
#if defined(DSRRL_V230_CUTSCENE_TRACE)
        g_v230_cutscene_source_path = 1u;
#endif
        telemetry::hot_count(""",
"""    if (pmetal_producer_state_latest(
            material,
            epoch,
            out)) {
#if defined(DSRRL_V230_CUTSCENE_TRACE)
        g_v230_cutscene_source_path = 1u;
#endif
#if defined(DSRRL_V230_SOURCE1_REPAIR)
        g_source1_exact.fetch_add(1u,std::memory_order_relaxed);
#endif
        telemetry::hot_count(""")
swap("""    if (latest_hook_source(out)) {
#if defined(DSRRL_V230_CUTSCENE_TRACE)
        g_v230_cutscene_source_path = 2u;
#endif
        telemetry::hot_count(""",
"""    if (latest_hook_source(out)) {
#if defined(DSRRL_V230_CUTSCENE_TRACE)
        g_v230_cutscene_source_path = 2u;
#endif
#if defined(DSRRL_V230_SOURCE1_REPAIR)
        g_source1_hook.fetch_add(1u,std::memory_order_relaxed);
#endif
        telemetry::hot_count(""")
p.write_text(s,"utf-8")
print("SOURCE1_REPAIR_SOURCE_PASS")
