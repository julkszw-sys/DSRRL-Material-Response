#!/usr/bin/env python3
"""V362 from V361: passive exact stock PntS clustered pipeline lifetime/bind/draw witness.

Applied after V361 materialization and source contracts. Leaves clustered
PointLight hook, materializer, shader replacement and draw bridge disabled.
Does not alter the existing V360 Fixed path or equipment sidecar authorization.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"integrated/integrated_addon.cpp"
s=p.read_text("utf-8")
def patch(old,new,count=1):
    global s
    n=s.count(old)
    if n!=count: raise RuntimeError(f"V362 drift {n}/{count}: {old[:110]!r}")
    s=s.replace(old,new,count)
anchor="std::atomic_bool g_pointlight_once_shader_ready{false};"
w=r"""
#if defined(DSRRL_V362_STOCK_CLUSTERED_BIND_CENSUS)
// Passive exact matching against already-certified PntS stock PS digests.
// This table is observational only; it NEVER authorizes shader swaps/draws.
// Four thousand independent buckets and eight exact probes make table
// overflow explicit rather than inventing "approximate" receiver identity.
constexpr std::size_t k_v362_slots=4096u;
std::array<std::atomic<std::uint64_t>,k_v362_slots> g_v362_handles{};
std::atomic<std::uint64_t> g_v362_cluster_init{0};
std::atomic<std::uint64_t> g_v362_cluster_destroy{0};
std::atomic<std::uint64_t> g_v362_cluster_table_overflow{0};
std::atomic<std::uint64_t> g_v362_pixel_bind_all{0};
std::atomic<std::uint64_t> g_v362_cluster_bind{0};
std::atomic<std::uint64_t> g_v362_cluster_nonpixel_bind{0};
std::atomic<std::uint64_t> g_v362_cluster_draw_regular{0};
std::atomic<std::uint64_t> g_v362_cluster_draw_indexed{0};
std::atomic<std::uint64_t> g_v362_cluster_bind_route_nonzero{0};
std::atomic<std::uint32_t> g_v362_log_budget{0};

std::size_t v362_index(std::uint64_t x) noexcept
{
    x^=x>>30; x*=0xbf58476d1ce4e5b9ULL;
    x^=x>>27; x*=0x94d049bb133111ebULL;
    x^=x>>31;
    return static_cast<std::size_t>(x&(k_v362_slots-1u));
}
bool v362_exact(std::uint64_t h) noexcept
{
    if (h==0u) return false;
    const auto home=v362_index(h);
    for (std::size_t i=0;i<8u;++i)
        if(g_v362_handles[(home+i)&(k_v362_slots-1u)].load(
                std::memory_order_acquire)==h) return true;
    return false;
}
void v362_register(std::uint64_t h) noexcept
{
    if(h==0u) return;
    const auto home=v362_index(h);
    for(std::size_t i=0;i<8u;++i) {
        auto &slot=g_v362_handles[(home+i)&(k_v362_slots-1u)];
        std::uint64_t expected=0u;
        if(slot.compare_exchange_strong(expected,h,
                std::memory_order_acq_rel,
                std::memory_order_acquire) ||
            expected==h) {
            g_v362_cluster_init.fetch_add(1u,std::memory_order_relaxed);
            return;
        }
    }
    g_v362_cluster_table_overflow.fetch_add(
        1u,std::memory_order_relaxed);
}
void v362_forget(std::uint64_t h) noexcept
{
    if (h==0u) return;
    const auto home=v362_index(h);
    for(std::size_t i=0;i<8u;++i) {
        auto &slot=g_v362_handles[(home+i)&(k_v362_slots-1u)];
        std::uint64_t expected=h;
        if(slot.compare_exchange_strong(expected,0u,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            g_v362_cluster_destroy.fetch_add(
                1u,std::memory_order_relaxed);
            return;
        }
    }
}
struct v362_bound_tls_state {
    const void *command=nullptr;
    bool clustered=false;
};
thread_local v362_bound_tls_state g_v362_current{};
void v362_bind(
    reshade::api::command_list *cmd_list,
    std::uint64_t handle,
    bool pixel,
    std::uint32_t route) noexcept
{
    if (pixel) {
        g_v362_pixel_bind_all.fetch_add(1u,std::memory_order_relaxed);
        const bool clustered=v362_exact(handle);
        g_v362_current={cmd_list,clustered};
        if (!clustered) return;
        g_v362_cluster_bind.fetch_add(
            1u,std::memory_order_relaxed);
        if (route!=0u)
            g_v362_cluster_bind_route_nonzero.fetch_add(
                1u,std::memory_order_relaxed);
        const auto budget=g_v362_log_budget.fetch_add(
            1u,std::memory_order_relaxed);
        if (budget<12u) {
            char line[240]{};
            std::snprintf(line,sizeof(line),
                "[DSRRL V362 STOCK PNTS] event=exact_cluster_bind "
                "handle=%016llx stage=pixel route=0x%02x "
                "operator=STOCK_DSR",
                static_cast<unsigned long long>(handle),route);
            reshade::log::message(reshade::log::level::info,line);
        }
    } else if(v362_exact(handle)) {
        g_v362_cluster_nonpixel_bind.fetch_add(
            1u,std::memory_order_relaxed);
    }
}
void v362_draw(reshade::api::command_list *cmd_list,bool indexed) noexcept
{
    if (g_v362_current.command!=cmd_list ||
        !g_v362_current.clustered) return;
    if(indexed)
        g_v362_cluster_draw_indexed.fetch_add(
            1u,std::memory_order_relaxed);
    else
        g_v362_cluster_draw_regular.fetch_add(
            1u,std::memory_order_relaxed);
}
void v362_snapshot(const char *tag) noexcept
{
    char line[550]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V362 STOCK PNTS] tag=%s "
        "cluster_init=%llu cluster_destroy=%llu overflow=%llu "
        "pixel_bind_all=%llu cluster_pixel_bind=%llu "
        "cluster_nonpixel_bind=%llu cluster_route_nonzero=%llu "
        "cluster_draw_regular=%llu cluster_draw_indexed=%llu "
        "clustered_bridge=DISABLED stock=UNCHANGED pixel=OPEN",
        tag,
        static_cast<unsigned long long>(g_v362_cluster_init.load()),
        static_cast<unsigned long long>(g_v362_cluster_destroy.load()),
        static_cast<unsigned long long>(g_v362_cluster_table_overflow.load()),
        static_cast<unsigned long long>(g_v362_pixel_bind_all.load()),
        static_cast<unsigned long long>(g_v362_cluster_bind.load()),
        static_cast<unsigned long long>(g_v362_cluster_nonpixel_bind.load()),
        static_cast<unsigned long long>(g_v362_cluster_bind_route_nonzero.load()),
        static_cast<unsigned long long>(g_v362_cluster_draw_regular.load()),
        static_cast<unsigned long long>(g_v362_cluster_draw_indexed.load()));
    reshade::log::message(reshade::log::level::info,line);
}
void v362_reset() noexcept
{
    for(auto &slot:g_v362_handles)
        slot.store(0u,std::memory_order_relaxed);
    g_v362_cluster_init.store(0u);
    g_v362_cluster_destroy.store(0u);
    g_v362_cluster_table_overflow.store(0u);
    g_v362_pixel_bind_all.store(0u);
    g_v362_cluster_bind.store(0u);
    g_v362_cluster_nonpixel_bind.store(0u);
    g_v362_cluster_draw_regular.store(0u);
    g_v362_cluster_draw_indexed.store(0u);
    g_v362_cluster_bind_route_nonzero.store(0u);
    g_v362_log_budget.store(0u);
    g_v362_current={};
}
#endif
"""
patch(anchor,w+"\n"+anchor)
init_anchor="""    motion_blur_camera_fallback_disable::
        on_init_pipeline(
            subobject_count,
            subobjects,
            pipeline);"""
inject=r"""
#if defined(DSRRL_V362_STOCK_CLUSTERED_BIND_CENSUS)
    // This is an exact SHA-256 match to the read-only source census.
    // Only the pre-existing PntS clustered_spc_pnts category is recorded.
    const auto *v362_ps=find_pixel_shader(subobject_count,subobjects);
    if(v362_ps!=nullptr && v362_ps->code!=nullptr &&
       v362_ps->code_size!=0u) {
        dsrrl::operators::point_light::
            local_specular_receiver_identity identity{};
        if(dsrrl::operators::point_light::
                local_specular_receiver_for_shader(
                    v362_ps->code,v362_ps->code_size,identity) &&
           identity.receiver_class==
                dsrrl::operators::point_light::
                    local_specular_receiver_class::clustered_spc_pnts)
            v362_register(pipeline.handle);
    }
#endif
"""
patch(init_anchor,inject+init_anchor)
patch("""#if defined(DSRRL_V361_FIXED_BIND_CENSUS)
        v361_forget_fixed_handle(pipeline.handle);
#endif""",
      """#if defined(DSRRL_V362_STOCK_CLUSTERED_BIND_CENSUS)
        v362_forget(pipeline.handle);
#endif
#if defined(DSRRL_V361_FIXED_BIND_CENSUS)
        v361_forget_fixed_handle(pipeline.handle);
#endif""")
patch("""        v361_exact_handle);
#endif
#if defined(DSRRL_V291_MATERIAL_STAGE_CENSUS)""",
      """        v361_exact_handle);
#endif
#if defined(DSRRL_V362_STOCK_CLUSTERED_BIND_CENSUS)
    v362_bind(cmd_list,pipeline.handle,pixel_stage_bound,route_mask);
#endif
#if defined(DSRRL_V291_MATERIAL_STAGE_CENSUS)""")
drawanchor="""    hot_count(g_draw_events);
    draw_semantic_selection_guard semantic_guard{};"""
if s.count(drawanchor)!=2: raise RuntimeError("both draws absent")
s=s.replace(drawanchor,
"""#if defined(DSRRL_V362_STOCK_CLUSTERED_BIND_CENSUS)
    v362_draw(cmd_list,false);
#endif
"""+drawanchor,1)
s=s.replace(drawanchor,
"""#if defined(DSRRL_V362_STOCK_CLUSTERED_BIND_CENSUS)
    v362_draw(cmd_list,true);
#endif
"""+drawanchor,1) if False else s
# Second occurrence must be placed after first injected one.
loc=s.find(drawanchor)
loc=s.find(drawanchor,loc+len(drawanchor))
if loc<0: raise RuntimeError("indexed draw anchor not found")
s=s[:loc]+"""#if defined(DSRRL_V362_STOCK_CLUSTERED_BIND_CENSUS)
    v362_draw(cmd_list,true);
#endif
"""+s[loc:]
patch("""#if defined(DSRRL_V361_FIXED_BIND_CENSUS)
    v361_report_bind_witness(tag);
#endif""",
      """#if defined(DSRRL_V361_FIXED_BIND_CENSUS)
    v361_report_bind_witness(tag);
#endif
#if defined(DSRRL_V362_STOCK_CLUSTERED_BIND_CENSUS)
    v362_snapshot(tag);
#endif""")
patch("""#if defined(DSRRL_V361_FIXED_BIND_CENSUS)
    v361_reset_bind_witness();
#endif""",
      """#if defined(DSRRL_V361_FIXED_BIND_CENSUS)
    v361_reset_bind_witness();
#endif
#if defined(DSRRL_V362_STOCK_CLUSTERED_BIND_CENSUS)
    v362_reset();
#endif""")
p.write_text(s,encoding="utf-8")
print("V362_READONLY_EXACT_CLUSTERED_STOCK_BIND_DRAW_WITNESS_MATERIALIZED")
