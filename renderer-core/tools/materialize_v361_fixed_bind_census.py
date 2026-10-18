#!/usr/bin/env python3
"""V361: read-only exact Fixed PS pipeline lifetime/bind identity witness.

Apply AFTER all V360 and historical source contracts, before configure/build.
No change to operator equations, sidecar allowlists, resource binding, draw
preparation, or stock fail-open. Diagnostic-only lock-free handle evidence.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=root/"integrated/integrated_addon.cpp"
s=source.read_text("utf-8")
def patch(old,new,count=1):
    global s
    actual=s.count(old)
    if actual!=count:
        raise RuntimeError(f"V361 source drift: expected {count} got {actual}: {old[:125]!r}")
    s=s.replace(old,new,count)

anchor="std::atomic_bool g_pointlight_once_shader_ready{false};"
witness=r"""
#if defined(DSRRL_V361_FIXED_BIND_CENSUS)
// Debug-only fixed pipeline identity witness. Exact handle membership, not
// approximate/bloom membership. No locks, no heap allocation and no candidate
// registry lookup on the global bind path. A full bucket is counted as an
// identity-coverage gap and never authorizes a route/bridge.
constexpr std::size_t k_v361_fixed_slots = 4096u;
std::array<std::atomic<std::uint64_t>, k_v361_fixed_slots>
    g_v361_fixed_pipeline_handles{};
std::atomic<std::uint64_t> g_v361_init_identified{0};
std::atomic<std::uint64_t> g_v361_init_full{0};
std::atomic<std::uint64_t> g_v361_all_bind_events{0};
std::atomic<std::uint64_t> g_v361_pixel_bind_events{0};
std::atomic<std::uint64_t> g_v361_fixed_handle_bind{0};
std::atomic<std::uint64_t> g_v361_fixed_handle_pixel_bind{0};
std::atomic<std::uint64_t> g_v361_fixed_handle_nonpixel_bind{0};
std::atomic<std::uint64_t> g_v361_fixed_handle_route_missing{0};
std::atomic<std::uint64_t> g_v361_fixed_route_bind{0};
std::atomic<std::uint64_t> g_v361_fixed_route_unknown_handle{0};
std::atomic<std::uint64_t> g_v361_fixed_forward_calls{0};
std::atomic<std::uint64_t> g_v361_fixed_forward_ok{0};
std::atomic<std::uint64_t> g_v361_fixed_forward_reject{0};
std::atomic<std::uint32_t> g_v361_init_log_budget{0};
std::atomic<std::uint32_t> g_v361_bind_log_budget{0};
std::atomic<std::uint32_t> g_v361_mismatch_log_budget{0};

std::size_t v361_pipeline_index(std::uint64_t handle) noexcept
{
    std::uint64_t x=handle;
    x^=x>>30;
    x*=0xbf58476d1ce4e5b9ULL;
    x^=x>>27;
    x*=0x94d049bb133111ebULL;
    x^=x>>31;
    return static_cast<std::size_t>(
        x&(k_v361_fixed_slots-1u));
}
void v361_register_fixed_handle(std::uint64_t handle) noexcept
{
    if (handle==0u) return;
    const auto home=v361_pipeline_index(handle);
    for (std::size_t i=0;i<8u;++i) {
        auto &slot=g_v361_fixed_pipeline_handles[
            (home+i)&(k_v361_fixed_slots-1u)];
        std::uint64_t expected=0u;
        if (slot.compare_exchange_strong(
                expected,handle,
                std::memory_order_acq_rel,
                std::memory_order_acquire) ||
            expected==handle) {
            g_v361_init_identified.fetch_add(
                1u,std::memory_order_relaxed);
            return;
        }
    }
    g_v361_init_full.fetch_add(1u,std::memory_order_relaxed);
}
bool v361_is_fixed_handle(std::uint64_t handle) noexcept
{
    if (handle==0u) return false;
    const auto home=v361_pipeline_index(handle);
    for (std::size_t i=0;i<8u;++i)
        if (g_v361_fixed_pipeline_handles[
            (home+i)&(k_v361_fixed_slots-1u)].load(
                std::memory_order_acquire)==handle)
            return true;
    return false;
}
void v361_forget_fixed_handle(std::uint64_t handle) noexcept
{
    if (handle==0u) return;
    const auto home=v361_pipeline_index(handle);
    for (std::size_t i=0;i<8u;++i) {
        auto &slot=g_v361_fixed_pipeline_handles[
            (home+i)&(k_v361_fixed_slots-1u)];
        std::uint64_t expected=handle;
        if (slot.compare_exchange_strong(
                expected,0u,
                std::memory_order_acq_rel,
                std::memory_order_acquire))
            return;
    }
}
void v361_log_bind_witness(
    std::uint64_t handle,
    std::uint32_t stage_mask,
    bool pixel,
    std::uint32_t route_mask,
    bool exact_handle) noexcept
{
    g_v361_all_bind_events.fetch_add(
        1u,std::memory_order_relaxed);
    if (pixel)
        g_v361_pixel_bind_events.fetch_add(
            1u,std::memory_order_relaxed);
    if ((route_mask&0x40u)!=0u) {
        g_v361_fixed_route_bind.fetch_add(
            1u,std::memory_order_relaxed);
        if (!exact_handle)
            g_v361_fixed_route_unknown_handle.fetch_add(
                1u,std::memory_order_relaxed);
    }
    if (!exact_handle) return;
    g_v361_fixed_handle_bind.fetch_add(
        1u,std::memory_order_relaxed);
    if (pixel)
        g_v361_fixed_handle_pixel_bind.fetch_add(
            1u,std::memory_order_relaxed);
    else
        g_v361_fixed_handle_nonpixel_bind.fetch_add(
            1u,std::memory_order_relaxed);
    if ((route_mask&0x40u)==0u)
        g_v361_fixed_handle_route_missing.fetch_add(
            1u,std::memory_order_relaxed);
    const auto budget=g_v361_bind_log_budget.fetch_add(
        1u,std::memory_order_relaxed);
    if (budget<12u) {
        char line[288]{};
        std::snprintf(line,sizeof(line),
            "[DSRRL V361 FIXED BIND] event=exact_handle_bind "
            "handle=%016llx stages=0x%08x pixel=%u route=0x%02x",
            static_cast<unsigned long long>(handle),
            stage_mask,pixel?1u:0u,route_mask);
        reshade::log::message(
            reshade::log::level::info,line);
    }
}
void v361_report_bind_witness(const char *tag) noexcept
{
    char line[600]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V361 FIXED BIND] tag=%s "
        "init_exact=%llu hash_table_full=%llu "
        "bind_all=%llu bind_pixel=%llu "
        "fixed_exact_handle=%llu fixed_pixel=%llu fixed_nonpixel=%llu "
        "fixed_route_missing=%llu route_fixed=%llu route_unknown_handle=%llu "
        "forward=%llu success=%llu reject=%llu "
        "pixel_equivalence=OPEN",
        tag,
        static_cast<unsigned long long>(g_v361_init_identified.load()),
        static_cast<unsigned long long>(g_v361_init_full.load()),
        static_cast<unsigned long long>(g_v361_all_bind_events.load()),
        static_cast<unsigned long long>(g_v361_pixel_bind_events.load()),
        static_cast<unsigned long long>(g_v361_fixed_handle_bind.load()),
        static_cast<unsigned long long>(g_v361_fixed_handle_pixel_bind.load()),
        static_cast<unsigned long long>(g_v361_fixed_handle_nonpixel_bind.load()),
        static_cast<unsigned long long>(g_v361_fixed_handle_route_missing.load()),
        static_cast<unsigned long long>(g_v361_fixed_route_bind.load()),
        static_cast<unsigned long long>(g_v361_fixed_route_unknown_handle.load()),
        static_cast<unsigned long long>(g_v361_fixed_forward_calls.load()),
        static_cast<unsigned long long>(g_v361_fixed_forward_ok.load()),
        static_cast<unsigned long long>(g_v361_fixed_forward_reject.load()));
    reshade::log::message(reshade::log::level::info,line);
}
void v361_reset_bind_witness() noexcept
{
    for(auto &slot:g_v361_fixed_pipeline_handles)
        slot.store(0u,std::memory_order_relaxed);
    g_v361_init_identified.store(0u);
    g_v361_init_full.store(0u);
    g_v361_all_bind_events.store(0u);
    g_v361_pixel_bind_events.store(0u);
    g_v361_fixed_handle_bind.store(0u);
    g_v361_fixed_handle_pixel_bind.store(0u);
    g_v361_fixed_handle_nonpixel_bind.store(0u);
    g_v361_fixed_handle_route_missing.store(0u);
    g_v361_fixed_route_bind.store(0u);
    g_v361_fixed_route_unknown_handle.store(0u);
    g_v361_fixed_forward_calls.store(0u);
    g_v361_fixed_forward_ok.store(0u);
    g_v361_fixed_forward_reject.store(0u);
    g_v361_init_log_budget.store(0u);
    g_v361_bind_log_budget.store(0u);
    g_v361_mismatch_log_budget.store(0u);
}
#endif
"""
patch(anchor, witness+"\n"+anchor)

# Match the same exact pipeline that on_init_pipeline certified, and witness
# its bytes. Sample only the first eight identities, but count all.
anchor="""    motion_blur_camera_fallback_disable::
        on_init_pipeline(
            subobject_count,"""
inject=r"""#if defined(DSRRL_V361_FIXED_BIND_CENSUS)
    if (fixed_pointlight_init_exact) {
        v361_register_fixed_handle(pipeline.handle);
        const auto budget=g_v361_init_log_budget.fetch_add(
            1u,std::memory_order_relaxed);
        if (budget<8u) {
            const auto *ps=find_pixel_shader(
                subobject_count,subobjects);
            if (ps!=nullptr && ps->code!=nullptr &&
                ps->code_size!=0u) {
                const auto sha=dsrrl::operators::legacy_plan::hashing::sha256(
                    static_cast<const std::uint8_t *>(ps->code),
                    ps->code_size);
                char line[320]{};
                std::snprintf(line,sizeof(line),
                    "[DSRRL V361 FIXED BIND] event=exact_pipeline_init "
                    "handle=%016llx code_bytes=%llu sha256_prefix=%02x%02x%02x%02x%02x%02x%02x%02x",
                    static_cast<unsigned long long>(pipeline.handle),
                    static_cast<unsigned long long>(ps->code_size),
                    sha[0],sha[1],sha[2],sha[3],
                    sha[4],sha[5],sha[6],sha[7]);
                reshade::log::message(
                    reshade::log::level::info,line);
            }
        }
    }
#endif
"""
patch(anchor,inject+anchor)

patch("""        g_fixed_pointlight_pipeline.on_destroy_pipeline(pipeline);""",
      """#if defined(DSRRL_V361_FIXED_BIND_CENSUS)
        v361_forget_fixed_handle(pipeline.handle);
#endif
        g_fixed_pointlight_pipeline.on_destroy_pipeline(pipeline);""")

anchor="""    auto route_mask =
        observe_integrated_draw_route_bind(
            cmd_list,
            pixel_stage_bound,
            pipeline.handle);"""
patch(anchor,anchor+r"""
#if defined(DSRRL_V361_FIXED_BIND_CENSUS)
    // Query exact handle evidence independent of the integrated route mask.
    // This is the distinguishing test: never infer routing from init alone.
    const bool v361_exact_handle =
        v361_is_fixed_handle(pipeline.handle);
    v361_log_bind_witness(
        pipeline.handle,
        static_cast<std::uint32_t>(stages),
        pixel_stage_bound,
        route_mask,
        v361_exact_handle);
#endif""")

# Instrument the actual fixed pipeline binding call (not the unrelated
# effect matrix). Leave original true/false return behavior unchanged.
anchor="""    if (pixel_stage_bound &&
        (route_mask &
         k_route_fixed_pointlight) != 0u)
        (void)g_fixed_pointlight_pipeline.on_bind_pipeline(
            cmd_list,
            stages,
            pipeline);"""
repl="""    if (pixel_stage_bound &&
        (route_mask &
         k_route_fixed_pointlight) != 0u) {
#if defined(DSRRL_V361_FIXED_BIND_CENSUS)
        g_v361_fixed_forward_calls.fetch_add(
            1u,std::memory_order_relaxed);
#endif
        const bool fixed_bound =
            g_fixed_pointlight_pipeline.on_bind_pipeline(
                cmd_list, stages, pipeline);
#if defined(DSRRL_V361_FIXED_BIND_CENSUS)
        if (fixed_bound)
            g_v361_fixed_forward_ok.fetch_add(
                1u,std::memory_order_relaxed);
        else
            g_v361_fixed_forward_reject.fetch_add(
                1u,std::memory_order_relaxed);
#endif
        (void)fixed_bound;
    }"""
patch(anchor,repl)

patch("""    reshade::log::message(
        reshade::log::level::info,
        fixed_pl_line);""",
      """    reshade::log::message(
        reshade::log::level::info,
        fixed_pl_line);
#if defined(DSRRL_V361_FIXED_BIND_CENSUS)
    v361_report_bind_witness(tag);
#endif""")
patch("    g_fixed_draw_fail_open.store(0);",
      """    g_fixed_draw_fail_open.store(0);
#if defined(DSRRL_V361_FIXED_BIND_CENSUS)
    v361_reset_bind_witness();
#endif""")

source.write_text(s,encoding="utf-8")
print("V361_FIXED_PIPELINE_INIT_BIND_ROUTE_CENSUS_MATERIALIZED")
