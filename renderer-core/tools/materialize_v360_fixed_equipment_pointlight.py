#!/usr/bin/env python3
"""V360: V359 descendant, Fixed PntSS/PntSSSS on exact PTDE equipment sidecars only.

Never widens the equipment tuple allowlist. Clustered PointLights stay stock.
The tested operator is full Fixed local diffuse/specular/attenuation, not a
global material or light intensity multiplier.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]


def change(path, old, new, count=1):
    p = root / path
    data = p.read_text(encoding="utf-8")
    actual = data.count(old)
    if actual != count:
        raise RuntimeError(f"V360 drift {path}: found {actual} vs {count}: {old[:110]!r}")
    p.write_text(data.replace(old, new, count), encoding="utf-8")


cmake = "integrated/CMakeLists.txt"
change(cmake,
       "NOT DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL)",
       "NOT DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL AND\n"
       "       NOT DSRRL_V360_FIXED_EQUIP_TEST)")

source = "integrated/integrated_addon.cpp"

# Explicit fixed-only policy. Preserve PointLight draw-time transport for the
# fixed producer but never install the clustered producer/pipeline.
change(source,
       "constexpr bool k_pointlight_drawtime_runtime_enabled = true;\n#endif",
       """constexpr bool k_pointlight_drawtime_runtime_enabled = true;
#endif

#if defined(DSRRL_V360_FIXED_EQUIP_TEST)
constexpr bool k_clustered_pointlight_runtime_enabled = false;
#else
constexpr bool k_clustered_pointlight_runtime_enabled =
    k_pointlight_drawtime_runtime_enabled;
#endif""")

# The creator had a single PointLight enabled guard which also built
# clustered shader candidates. Keep fixed local-specular construction, but
# skip clustered shader work even before bind registration.
change(source,
       """        if (k_pointlight_drawtime_runtime_enabled) {
            clustered_pnts =
                dsrrl::operators::point_light::
                    materialize_clustered_pnts_direct_ptde(""",
       """        if (k_pointlight_drawtime_runtime_enabled) {
            if (k_clustered_pointlight_runtime_enabled) {
            clustered_pnts =
                dsrrl::operators::point_light::
                    materialize_clustered_pnts_direct_ptde(""")
change(source,
       """                    clustered_pnts_direct_materialize_result::applied;

        // Observe the exact original DSR local-specular host""",
       """                    clustered_pnts_direct_materialize_result::applied;
            }

        // Observe the exact original DSR local-specular host""")
change(source,
       """    if (k_pointlight_drawtime_runtime_enabled &&
        clustered_pnts_candidate) {""",
       """    if (k_clustered_pointlight_runtime_enabled &&
        clustered_pnts_candidate) {""")
change(source,
       """    const bool clustered_pointlight_init_exact =
        k_pointlight_drawtime_runtime_enabled &&""",
       """    const bool clustered_pointlight_init_exact =
        k_clustered_pointlight_runtime_enabled &&""")
change(source,
       """    const bool clustered_pointlight_hooks =
        k_pointlight_drawtime_runtime_enabled &&""",
       """    const bool clustered_pointlight_hooks =
        k_clustered_pointlight_runtime_enabled &&""")
change(source,
       """    if (k_pointlight_drawtime_runtime_enabled &&
        !clustered_pointlight_hooks) {""",
       """    if (k_clustered_pointlight_runtime_enabled &&
        !clustered_pointlight_hooks) {""")
# The FLVER install's first argument owns the clustered builder, not Fixed.
# Do not arm a hidden clustered CPU producer while reporting clustered=STOCK.
change(source,
       """flver_identity_transport::install(
            k_pointlight_drawtime_runtime_enabled,
            upper_lower_enabled,
            hemdir3_enabled)""",
       """flver_identity_transport::install(
            k_clustered_pointlight_runtime_enabled,
            upper_lower_enabled,
            hemdir3_enabled)""")

# Preserve both audits, but require the *new* exact V360 split instead of
# their original PointLight-all-on/off spelling.
hot="tools/audit_active_islands_hotpath.py"
change(hot,
       "k_pointlight_drawtime_runtime_enabled,\\n            upper_lower_enabled",
       "k_clustered_pointlight_runtime_enabled,\\n            upper_lower_enabled")
change(hot,
       "if (k_pointlight_drawtime_runtime_enabled) {\\n            clustered_pnts =",
       "if (k_pointlight_drawtime_runtime_enabled) {\\n            if (k_clustered_pointlight_runtime_enabled) {\\n            clustered_pnts =")
change(hot,
       "if (k_pointlight_drawtime_runtime_enabled &&\\n        clustered_pnts_candidate)",
       "if (k_clustered_pointlight_runtime_enabled &&\\n        clustered_pnts_candidate)")
cluster_audit="tools/audit_clustered_pnts_activation_sources.py"
change(cluster_audit,
       "k_pointlight_drawtime_runtime_enabled,\\n            upper_lower_enabled",
       "k_clustered_pointlight_runtime_enabled,\\n            upper_lower_enabled")

change(source,
       "    g_clustered_pnts.frame_event(present);",
       "    if (k_clustered_pointlight_runtime_enabled)\n"
       "        g_clustered_pnts.frame_event(present);")

# Fixed material path is intentionally untouched:
# - exact FLVER/material slot owner
# - exact t0/t1/t2 and blended t3/t4/t5 stock tuple + logical texture
# - equipment allowlist
# - PTDE exact companion SpecRGB/Diffuse/Normal
# - authenticated Fixed shader family + b12 + t19, one atomic transaction
#
# Existing FIXED_PL_TX already accounts producer, selector, shader, sidecars,
# CB, t19 and batch. Add truly draw-issued / restore outcomes separately.
change(source,
       "std::atomic_bool g_clustered_pointlight_selection_transport_active{false};",
       """std::atomic_bool g_clustered_pointlight_selection_transport_active{false};
#if defined(DSRRL_V360_FIXED_EQUIP_TEST)
std::atomic<std::uint64_t> g_v360_fixed_issued{0u};
std::atomic<std::uint64_t> g_v360_fixed_not_issued{0u};
std::atomic<std::uint64_t> g_v360_fixed_restore_failed{0u};
std::atomic_bool g_v360_fixed_first_applied{false};
#endif""")
change(source,
       """    const bool mr_in_batch =
        prepared.mr_in_batch;""",
       """    const bool fixed_in_batch =
        prepared.fixed_in_batch;
    const bool mr_in_batch =
        prepared.mr_in_batch;""", 2)

account = """    account_effect_dispatch(
        effect_prepared,
        dispatch.transaction,
        receiver_id,
        decision.route_index);"""
inject = account + """

#if defined(DSRRL_V360_FIXED_EQUIP_TEST)
    if (fixed_in_batch) {
        if (dsrrl::runtime::draw_tx_issued(dispatch.transaction)) {
            g_v360_fixed_issued.fetch_add(
                1u, std::memory_order_relaxed);
            if (!g_v360_fixed_first_applied.exchange(true))
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL V360 FIXED EQUIPMENT] stage=draw_issued "
                    "fixed_only=1 sidecars=EXACT_EQUIPMENT "
                    "shader=PTDE_FIXED_LOCAL_SPEC_DIFF_ATTN "
                    "pixel=OPEN");
        } else {
            g_v360_fixed_not_issued.fetch_add(
                1u, std::memory_order_relaxed);
        }
        if (dispatch.transaction ==
            dsrrl::runtime::draw_tx_result::issued_restore_failed)
            g_v360_fixed_restore_failed.fetch_add(
                1u, std::memory_order_relaxed);
    }
#endif"""
change(source, account, inject, 2)

# Periodic bounded aggregate telemetry (startup and every 300 presents);
# the existing FIXED_PL_TX line distinguishes earlier gate failures.
change(source,
       """    reshade::log::message(
        reshade::log::level::info,
        fixed_pl_line);""",
       """    reshade::log::message(
        reshade::log::level::info,
        fixed_pl_line);
#if defined(DSRRL_V360_FIXED_EQUIP_TEST)
    char v360_fixed_line[768]{};
    std::snprintf(
        v360_fixed_line, sizeof(v360_fixed_line),
        "[DSRRL V360 FIXED EQUIPMENT] tag=%s "
        "fixed_only=1 clustered=STOCK_DSR "
        "candidate=%llu material=%llu sidecars=%llu "
        "b12=%llu t19=%llu batch=%llu fail_open=%llu "
        "draw_issued=%llu draw_not_issued=%llu restore_failed=%llu "
        "pixel=OPEN",
        tag,
        static_cast<unsigned long long>(g_fixed_draw_candidates.load()),
        static_cast<unsigned long long>(g_fixed_draw_material_ready.load()),
        static_cast<unsigned long long>(g_fixed_draw_spec_ready.load()),
        static_cast<unsigned long long>(g_fixed_draw_b12_ready.load()),
        static_cast<unsigned long long>(g_fixed_draw_t19_ready.load()),
        static_cast<unsigned long long>(g_fixed_draw_batch_ready.load()),
        static_cast<unsigned long long>(g_fixed_draw_fail_open.load()),
        static_cast<unsigned long long>(g_v360_fixed_issued.load()),
        static_cast<unsigned long long>(g_v360_fixed_not_issued.load()),
        static_cast<unsigned long long>(g_v360_fixed_restore_failed.load()));
    reshade::log::message(
        reshade::log::level::info, v360_fixed_line);
#endif""")

change(source,
       """    g_hot_telemetry_enabled =
        runtime_hot_telemetry_requested();""",
       """    g_hot_telemetry_enabled =
        runtime_hot_telemetry_requested();
#if defined(DSRRL_V360_FIXED_EQUIP_TEST)
    reshade::log::message(
        reshade::log::level::info,
        "[DSRRL V360 FIXED EQUIPMENT] stage=addon_loaded "
        "fixed=ENABLED clustered=BYPASSED "
        "scope=EXACT_PTDE_EQUIPMENT_SIDECARS telemetry=ON pixel=OPEN");
#endif""")
change(source,
       "    g_fixed_draw_fail_open.store(0);",
       """    g_fixed_draw_fail_open.store(0);
#if defined(DSRRL_V360_FIXED_EQUIP_TEST)
    g_v360_fixed_issued.store(0u);
    g_v360_fixed_not_issued.store(0u);
    g_v360_fixed_restore_failed.store(0u);
    g_v360_fixed_first_applied.store(false);
#endif""")

print("V360_FIXED_ONLY_EXACT_EQUIPMENT_SIDE_CAR_RUNTIME_TELEMETRY_MATERIALIZED")
