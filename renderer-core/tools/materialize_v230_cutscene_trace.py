#!/usr/bin/env python3
"""Read-only stable HemEnv REMO transition trace for the pinned v2.3.0 #235.

Run AFTER V359 materialization and V230 release-cleanup, BEFORE native build.
No shader, operator, LightBank, cache, hook, or D3D transaction mutation.
This file is an explicit diagnostic overlay; it is not included in production.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
paths = {
    "source_h": root / "include/dsrrl/runtime/pmetal_env_source_runtime.hpp",
    "source_cpp": root / "src/runtime/pmetal_env_source_runtime.cpp",
    "draw_h": root / "include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp",
    "draw_cpp": root / "src/runtime/pmetal_envspec_draw_runtime.cpp",
    "integrated": root.parent / "build-v230-release/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp",
}

def edit(name, old, new):
    file = paths[name]
    src = file.read_text("utf-8")
    n = src.count(old)
    if n != 1:
        raise RuntimeError(f"V230 CUTSCENE source drift {name} [{old[:75]!r}]: {n}")
    file.write_text(src.replace(old, new, 1), "utf-8")

edit("source_h",
     "void pmetal_env_source_selector_clear() noexcept;",
     """// Read-only identity of the latest source consumed on this thread:
 // 0 missing, 1 exact material producer, 2 protected native hook fallback.
 #if defined(DSRRL_V230_CUTSCENE_TRACE)
 std::uint32_t v230_cutscene_source_path() noexcept;
 #endif
 void pmetal_env_source_selector_clear() noexcept;""".replace("\n ", "\n"))

edit("source_cpp",
     "thread_local hook_source_record g_hook_source_tls{};",
     """thread_local hook_source_record g_hook_source_tls{};
#if defined(DSRRL_V230_CUTSCENE_TRACE)
thread_local std::uint32_t g_v230_cutscene_source_path = 0u;
#endif""")

edit("source_cpp",
     "} // namespace\n\nvoid pmetal_env_source_cache_invalidate() noexcept",
     """} // namespace

#if defined(DSRRL_V230_CUTSCENE_TRACE)
std::uint32_t v230_cutscene_source_path() noexcept
{
    return g_v230_cutscene_source_path;
}
#endif

void pmetal_env_source_cache_invalidate() noexcept""")

edit("source_cpp",
     "pmetal_envspec_source &out) const noexcept\n{",
     """pmetal_envspec_source &out) const noexcept
{
#if defined(DSRRL_V230_CUTSCENE_TRACE)
    g_v230_cutscene_source_path = 0u;
#endif""")

edit("source_cpp",
     "if (pmetal_producer_state_latest(\n            material,\n            epoch,\n            out)) {",
     """if (pmetal_producer_state_latest(
            material,
            epoch,
            out)) {
#if defined(DSRRL_V230_CUTSCENE_TRACE)
        g_v230_cutscene_source_path = 1u;
#endif""")

edit("source_cpp",
     "if (latest_hook_source(out)) {\n        telemetry::hot_count(",
     """if (latest_hook_source(out)) {
#if defined(DSRRL_V230_CUTSCENE_TRACE)
        g_v230_cutscene_source_path = 2u;
#endif
        telemetry::hot_count(""")

edit("draw_h",
     "    ID3D11Buffer *b12 = nullptr;",
     """    ID3D11Buffer *b12 = nullptr;
#if defined(DSRRL_V230_CUTSCENE_TRACE)
    // Exact CPU-side source used to prepare this draw, not a later global read.
    pmetal_envspec_source v230_cutscene_source{};
    std::uint32_t v230_cutscene_path = 0u;
    bool v230_cutscene_source_valid = false;
#endif""")

edit("draw_cpp",
     "    effect_latch(effect_source_ready_);",
     """    effect_latch(effect_source_ready_);
#if defined(DSRRL_V230_CUTSCENE_TRACE)
    prepared.v230_cutscene_source = source;
    prepared.v230_cutscene_path = v230_cutscene_source_path();
    prepared.v230_cutscene_source_valid = true;
#endif""")

# Called only after original V230 telemetry suppression: these INFO lines
# deliberately survive so the owner can return one ReShade.log.
observer = r"""
#if defined(DSRRL_V230_CUTSCENE_TRACE)
// A low-frequency, read-only observation at the REAL draw dispatch boundary.
// It covers P_Metal and non-P_Metal routes and never alters shader/CB/SRV state.
// The post-dispatch PSGetSRV reads observe restored stock slots, not the
// replacement SRVs active during the replay; the actual injected PTDE probe
// ordinal comes from prepared.envspec.env_resources.
void v230_cutscene_emit(
    const dsrrl::operators::material_response::material_identity &material,
    const dsrrl::operators::material_response::decision &decision,
    const dsrrl::runtime::prepared_pmetal_envspec_draw *prepared,
    reshade::api::command_list *cmd_list,
    const char *stage,
    bool draw_issued) noexcept
{
    const auto rx = decision.receiver_id;
    if ((rx != 33u && rx != 34u) || !material.valid)
        return;

    auto hash = std::uint64_t{14695981039346656037ull};
    const auto add = [&](std::uint8_t c) {
        hash = (hash ^ c) * 1099511628211ull;
    };
    for (auto v : material.flver_sha256) add(v);
    for (auto v : material.raw_mtd_sha256) add(v);
    add(static_cast<std::uint8_t>(material.material_slot));
    add(static_cast<std::uint8_t>(rx));
    add(static_cast<std::uint8_t>(decision.route_index));
    add(static_cast<std::uint8_t>(decision.route_index >> 8u));

    static std::array<std::atomic<std::uint64_t>,512> last_tick{};
    static std::atomic<std::uint32_t> total{0u};
    const auto tick = static_cast<std::uint64_t>(GetTickCount64());
    auto &previous = last_tick[hash % last_tick.size()];
    auto old = previous.load(std::memory_order_relaxed);
    if (tick >= old && tick - old < 900u)
        return;
    if (!previous.compare_exchange_strong(
            old, tick, std::memory_order_relaxed))
        return;
    if (total.fetch_add(1u, std::memory_order_relaxed) >= 2600u)
        return;

    auto hex = [](const std::array<std::uint8_t,32> &sha,
                  char (&dst)[65]) noexcept {
        constexpr char digits[] = "0123456789abcdef";
        for (std::size_t i=0; i<sha.size(); ++i) {
            dst[i*2] = digits[sha[i]>>4u];
            dst[i*2+1] = digits[sha[i]&15u];
        }
        dst[64] = '\0';
    };
    char flver[65]{}, mtd[65]{};
    hex(material.flver_sha256, flver);
    hex(material.raw_mtd_sha256, mtd);

    // Sample host t11 and t12 only AFTER a cool-down; PSGetSRV AddRefs
    // the two views; both are released immediately.
    ID3D11ShaderResourceView *views[2]{};
    if (cmd_list != nullptr) {
        auto *ctx = reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
        if (ctx != nullptr)
            ctx->PSGetShaderResources(11u,2u,views);
    }
    const auto native_t11 = reinterpret_cast<std::uintptr_t>(views[0]);
    const auto native_t12 = reinterpret_cast<std::uintptr_t>(views[1]);
    if (views[0]) views[0]->Release();
    if (views[1]) views[1]->Release();

    const bool atomic = prepared != nullptr &&
        prepared->v230_cutscene_source_valid;
    const auto source = atomic
        ? prepared->v230_cutscene_source
        : dsrrl::runtime::pmetal_envspec_source{};
    const auto kind = atomic ? prepared->v230_cutscene_path : 0u;
    const auto probe_a = atomic ? prepared->env_resources.probe_a : 0xffffu;
    const auto probe_b = atomic ? prepared->env_resources.probe_b : 0xffffu;
    const auto shader = atomic
        ? reinterpret_cast<std::uintptr_t>(prepared->shader) : 0u;
    const auto envdiffuse_view = atomic
        ? reinterpret_cast<std::uintptr_t>(
              prepared->envdiffuse_resources.ptde_a) : 0u;

    char line[1150]{};
    std::snprintf(
        line, sizeof(line),
        "[DSRRL V230 CUTSCENE] tick=%llu stage=%s issued=%u "
        "rx=%u route=%u slot=%u atomic=%u source_kind=%u "
        "bank_a=%016llx row_a=%u bank_b=%016llx row_b=%u beta=%.8g "
        "ptde_envspec_a=%.7g,%.7g,%.7g "
        "ptde_c86_a_cpu=%.7g,%.7g,%.7g "
        "ptde_c86_b_cpu=%.7g,%.7g,%.7g "
        "probe_a=%u probe_b=%u ptde_t11=%p stock_t11=%p stock_t12=%p "
        "replacement_ps=%p flver=%s mtd=%s",
        static_cast<unsigned long long>(tick),
        stage, draw_issued ? 1u : 0u,
        static_cast<unsigned>(rx),
        static_cast<unsigned>(decision.route_index),
        static_cast<unsigned>(material.material_slot),
        atomic ? 1u : 0u, kind,
        static_cast<unsigned long long>(source.bank_signature_a),
        static_cast<unsigned>(source.row_id_a),
        static_cast<unsigned long long>(source.bank_signature_b),
        static_cast<unsigned>(source.row_id_b),
        static_cast<double>(source.beta),
        static_cast<double>(source.a[0]),static_cast<double>(source.a[1]),
        static_cast<double>(source.a[2]),
        static_cast<double>(source.envdiffuse_a[0]),
        static_cast<double>(source.envdiffuse_a[1]),
        static_cast<double>(source.envdiffuse_a[2]),
        static_cast<double>(source.envdiffuse_b[0]),
        static_cast<double>(source.envdiffuse_b[1]),
        static_cast<double>(source.envdiffuse_b[2]),
        static_cast<unsigned>(probe_a),
        static_cast<unsigned>(probe_b),
        reinterpret_cast<void *>(envdiffuse_view),
        reinterpret_cast<void *>(native_t11),
        reinterpret_cast<void *>(native_t12),
        reinterpret_cast<void *>(shader),flver,mtd);
    reshade::log::message(reshade::log::level::info,line);
}
#endif // DSRRL_V230_CUTSCENE_TRACE

"""
edit("integrated",
     "effect_probe_mask resource_effect_mask(",
     observer + "effect_probe_mask resource_effect_mask(")

edit("integrated",
     '''            return false;
        }

#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)
        if (material.route_index != 345u) {''',
     '''#if defined(DSRRL_V230_CUTSCENE_TRACE)
            v230_cutscene_emit(material, decision, nullptr, cmd_list,
                              "atomic_prepare_fail", false);
#endif
            return false;
        }

#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)
        if (material.route_index != 345u) {''')

# Both non-indexed and indexed draw dispatch callsites need the same witness.
# An RX33/RX34 indexed draw must not disappear from the diagnostic.
def edit_all(name, old, new, expected):
    file = paths[name]
    src = file.read_text("utf-8")
    count = src.count(old)
    if count != expected:
        for needle in ("g_raw_draw_replay_recursing = false;", "dispatch_island_draw_batch(", "dispatch_island_draw_indexed_batch(", "draw_tx_issued(dispatch.transaction)", "#if defined(DSRRL_V291_LERP_CUBEMAP_CENSUS)"):
            at = src.find(needle)
            print("V230_TRACE_DEBUG",needle,"COUNT",src.count(needle),"EXCERPT",repr(src[max(0,at-130):at+160]))
        raise RuntimeError(f"V230 CUTSCENE callsite drift {name}: expected {expected}, got {count}")
    file.write_text(src.replace(old, new), "utf-8")

edit_all("integrated",
     """    if (direct_current_native)
        g_raw_draw_replay_recursing = false;

#if defined(DSRRL_V291_LERP_CUBEMAP_CENSUS)""",
     """    if (direct_current_native)
        g_raw_draw_replay_recursing = false;

#if defined(DSRRL_V230_CUTSCENE_TRACE)
    v230_cutscene_emit(
        material, decision,
        prepared.envspec_in_batch ? &prepared.envspec : nullptr,
        cmd_list, "post_dispatch",
        dsrrl::runtime::draw_tx_issued(dispatch.transaction));
#endif

#if defined(DSRRL_V291_LERP_CUBEMAP_CENSUS)""", 2)

print("V230_CUTSCENE_TRACE_OVERLAY_PASS source_kind+exact c86+GI ordinal+post_draw")
